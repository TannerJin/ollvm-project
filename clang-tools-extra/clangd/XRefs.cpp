//===--- XRefs.cpp -----------------------------------------------*- C++-*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
#include "XRefs.h"
#include "AST.h"
#include "Logger.h"
#include "SourceCode.h"
#include "URI.h"
#include "clang/AST/DeclTemplate.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Index/IndexDataConsumer.h"
#include "clang/Index/IndexingAction.h"
#include "clang/Index/USRGeneration.h"
#include "llvm/Support/Path.h"

namespace clang {
namespace clangd {
namespace {

// Get the definition from a given declaration `D`.
// Return nullptr if no definition is found, or the declaration type of `D` is
// not supported.
const Decl *getDefinition(const Decl *D) {
  assert(D);
  if (const auto *TD = dyn_cast<TagDecl>(D))
    return TD->getDefinition();
  else if (const auto *VD = dyn_cast<VarDecl>(D))
    return VD->getDefinition();
  else if (const auto *FD = dyn_cast<FunctionDecl>(D))
    return FD->getDefinition();
  return nullptr;
}

void logIfOverflow(const SymbolLocation &Loc) {
  if (Loc.Start.hasOverflow() || Loc.End.hasOverflow())
    log("Possible overflow in symbol location: {0}", Loc);
}

// Convert a SymbolLocation to LSP's Location.
// TUPath is used to resolve the path of URI.
// FIXME: figure out a good home for it, and share the implementation with
// FindSymbols.
llvm::Optional<Location> toLSPLocation(const SymbolLocation &Loc,
                                       llvm::StringRef TUPath) {
  if (!Loc)
    return None;
  auto Uri = URI::parse(Loc.FileURI);
  if (!Uri) {
    elog("Could not parse URI {0}: {1}", Loc.FileURI, Uri.takeError());
    return None;
  }
  auto U = URIForFile::fromURI(*Uri, TUPath);
  if (!U) {
    elog("Could not resolve URI {0}: {1}", Loc.FileURI, U.takeError());
    return None;
  }

  Location LSPLoc;
  LSPLoc.uri = std::move(*U);
  LSPLoc.range.start.line = Loc.Start.line();
  LSPLoc.range.start.character = Loc.Start.column();
  LSPLoc.range.end.line = Loc.End.line();
  LSPLoc.range.end.character = Loc.End.column();
  logIfOverflow(Loc);
  return LSPLoc;
}

struct MacroDecl {
  llvm::StringRef Name;
  const MacroInfo *Info;
};

struct DeclInfo {
  const Decl *D;
  // Indicates the declaration is referenced by an explicit AST node.
  bool IsReferencedExplicitly = false;
};

/// Finds declarations locations that a given source location refers to.
class DeclarationAndMacrosFinder : public index::IndexDataConsumer {
  std::vector<MacroDecl> MacroInfos;
  // The value of the map indicates whether the declaration has been referenced
  // explicitly in the code.
  // True means the declaration is explicitly referenced at least once; false
  // otherwise.
  llvm::DenseMap<const Decl *, bool> Decls;
  const SourceLocation &SearchedLocation;
  const ASTContext &AST;
  Preprocessor &PP;

public:
  DeclarationAndMacrosFinder(const SourceLocation &SearchedLocation,
                             ASTContext &AST, Preprocessor &PP)
      : SearchedLocation(SearchedLocation), AST(AST), PP(PP) {}

  // Get all DeclInfo of the found declarations.
  // The results are sorted by "IsReferencedExplicitly" and declaration
  // location.
  std::vector<DeclInfo> getFoundDecls() const {
    std::vector<DeclInfo> Result;
    for (auto It : Decls) {
      Result.emplace_back();
      Result.back().D = It.first;
      Result.back().IsReferencedExplicitly = It.second;
    }

    // Sort results. Declarations being referenced explicitly come first.
    llvm::sort(Result, [](const DeclInfo &L, const DeclInfo &R) {
      if (L.IsReferencedExplicitly != R.IsReferencedExplicitly)
        return L.IsReferencedExplicitly > R.IsReferencedExplicitly;
      return L.D->getBeginLoc() < R.D->getBeginLoc();
    });
    return Result;
  }

  std::vector<MacroDecl> takeMacroInfos() {
    // Don't keep the same Macro info multiple times.
    llvm::sort(MacroInfos, [](const MacroDecl &Left, const MacroDecl &Right) {
      return Left.Info < Right.Info;
    });

    auto Last = std::unique(MacroInfos.begin(), MacroInfos.end(),
                            [](const MacroDecl &Left, const MacroDecl &Right) {
                              return Left.Info == Right.Info;
                            });
    MacroInfos.erase(Last, MacroInfos.end());
    return std::move(MacroInfos);
  }

  bool
  handleDeclOccurence(const Decl *D, index::SymbolRoleSet Roles,
                      llvm::ArrayRef<index::SymbolRelation> Relations,
                      SourceLocation Loc,
                      index::IndexDataConsumer::ASTNodeInfo ASTNode) override {
    if (Loc == SearchedLocation) {
      auto isImplicitExpr = [](const Expr *E) {
        if (!E)
          return false;
        // We assume that a constructor expression is implict (was inserted by
        // clang) if it has an invalid paren/brace location, since such
        // experssion is impossible to write down.
        if (const auto *CtorExpr = dyn_cast<CXXConstructExpr>(E))
          return CtorExpr->getNumArgs() > 0 &&
                 CtorExpr->getParenOrBraceRange().isInvalid();
        return isa<ImplicitCastExpr>(E);
      };

      bool IsExplicit = !isImplicitExpr(ASTNode.OrigE);
      // Find and add definition declarations (for GoToDefinition).
      // We don't use parameter `D`, as Parameter `D` is the canonical
      // declaration, which is the first declaration of a redeclarable
      // declaration, and it could be a forward declaration.
      if (const auto *Def = getDefinition(D)) {
        Decls[Def] |= IsExplicit;
      } else {
        // Couldn't find a definition, fall back to use `D`.
        Decls[D] |= IsExplicit;
      }
    }
    return true;
  }

private:
  void finish() override {
    // Also handle possible macro at the searched location.
    Token Result;
    auto &Mgr = AST.getSourceManager();
    if (!Lexer::getRawToken(Mgr.getSpellingLoc(SearchedLocation), Result, Mgr,
                            AST.getLangOpts(), false)) {
      if (Result.is(tok::raw_identifier)) {
        PP.LookUpIdentifierInfo(Result);
      }
      IdentifierInfo *IdentifierInfo = Result.getIdentifierInfo();
      if (IdentifierInfo && IdentifierInfo->hadMacroDefinition()) {
        std::pair<FileID, unsigned int> DecLoc =
            Mgr.getDecomposedExpansionLoc(SearchedLocation);
        // Get the definition just before the searched location so that a macro
        // referenced in a '#undef MACRO' can still be found.
        SourceLocation BeforeSearchedLocation = Mgr.getMacroArgExpandedLocation(
            Mgr.getLocForStartOfFile(DecLoc.first)
                .getLocWithOffset(DecLoc.second - 1));
        MacroDefinition MacroDef =
            PP.getMacroDefinitionAtLoc(IdentifierInfo, BeforeSearchedLocation);
        MacroInfo *MacroInf = MacroDef.getMacroInfo();
        if (MacroInf) {
          MacroInfos.push_back(MacroDecl{IdentifierInfo->getName(), MacroInf});
          assert(Decls.empty());
        }
      }
    }
  }
};

struct IdentifiedSymbol {
  std::vector<DeclInfo> Decls;
  std::vector<MacroDecl> Macros;
};

IdentifiedSymbol getSymbolAtPosition(ParsedAST &AST, SourceLocation Pos) {
  auto DeclMacrosFinder = DeclarationAndMacrosFinder(Pos, AST.getASTContext(),
                                                     AST.getPreprocessor());
  index::IndexingOptions IndexOpts;
  IndexOpts.SystemSymbolFilter =
      index::IndexingOptions::SystemSymbolFilterKind::All;
  IndexOpts.IndexFunctionLocals = true;
  indexTopLevelDecls(AST.getASTContext(), AST.getPreprocessor(),
                     AST.getLocalTopLevelDecls(), DeclMacrosFinder, IndexOpts);

  return {DeclMacrosFinder.getFoundDecls(), DeclMacrosFinder.takeMacroInfos()};
}

Range getTokenRange(ParsedAST &AST, SourceLocation TokLoc) {
  const SourceManager &SourceMgr = AST.getASTContext().getSourceManager();
  SourceLocation LocEnd = Lexer::getLocForEndOfToken(
      TokLoc, 0, SourceMgr, AST.getASTContext().getLangOpts());
  return {sourceLocToPosition(SourceMgr, TokLoc),
          sourceLocToPosition(SourceMgr, LocEnd)};
}

llvm::Optional<Location> makeLocation(ParsedAST &AST, SourceLocation TokLoc,
                                      llvm::StringRef TUPath) {
  const SourceManager &SourceMgr = AST.getASTContext().getSourceManager();
  const FileEntry *F = SourceMgr.getFileEntryForID(SourceMgr.getFileID(TokLoc));
  if (!F)
    return None;
  auto FilePath = getCanonicalPath(F, SourceMgr);
  if (!FilePath) {
    log("failed to get path!");
    return None;
  }
  Location L;
  L.uri = URIForFile::canonicalize(*FilePath, TUPath);
  L.range = getTokenRange(AST, TokLoc);
  return L;
}

} // namespace

std::vector<Location> findDefinitions(ParsedAST &AST, Position Pos,
                                      const SymbolIndex *Index) {
  const auto &SM = AST.getASTContext().getSourceManager();
  auto MainFilePath =
      getCanonicalPath(SM.getFileEntryForID(SM.getMainFileID()), SM);
  if (!MainFilePath) {
    elog("Failed to get a path for the main file, so no references");
    return {};
  }

  std::vector<Location> Result;
  // Handle goto definition for #include.
  for (auto &Inc : AST.getIncludeStructure().MainFileIncludes) {
    if (!Inc.Resolved.empty() && Inc.R.start.line == Pos.line)
      Result.push_back(
          Location{URIForFile::canonicalize(Inc.Resolved, *MainFilePath), {}});
  }
  if (!Result.empty())
    return Result;

  // Identified symbols at a specific position.
  SourceLocation SourceLocationBeg =
      getBeginningOfIdentifier(AST, Pos, SM.getMainFileID());
  auto Symbols = getSymbolAtPosition(AST, SourceLocationBeg);

  for (auto Item : Symbols.Macros) {
    auto Loc = Item.Info->getDefinitionLoc();
    auto L = makeLocation(AST, Loc, *MainFilePath);
    if (L)
      Result.push_back(*L);
  }

  // Declaration and definition are different terms in C-family languages, and
  // LSP only defines the "GoToDefinition" specification, so we try to perform
  // the "most sensible" GoTo operation:
  //
  //  - We use the location from AST and index (if available) to provide the
  //    final results. When there are duplicate results, we prefer AST over
  //    index because AST is more up-to-date.
  //
  //  - For each symbol, we will return a location of the canonical declaration
  //    (e.g. function declaration in header), and a location of definition if
  //    they are available.
  //
  // So the work flow:
  //
  //   1. Identify the symbols being search for by traversing the AST.
  //   2. Populate one of the locations with the AST location.
  //   3. Use the AST information to query the index, and populate the index
  //      location (if available).
  //   4. Return all populated locations for all symbols, definition first (
  //      which  we think is the users wants most often).
  struct CandidateLocation {
    llvm::Optional<Location> Def;
    llvm::Optional<Location> Decl;
  };
  // We respect the order in Symbols.Decls.
  llvm::SmallVector<CandidateLocation, 8> ResultCandidates;
  llvm::DenseMap<SymbolID, size_t> CandidatesIndex;

  // Emit all symbol locations (declaration or definition) from AST.
  for (const DeclInfo &DI : Symbols.Decls) {
    const Decl *D = DI.D;
    // Fake key for symbols don't have USR (no SymbolID).
    // Ideally, there should be a USR for each identified symbols. Symbols
    // without USR are rare and unimportant cases, we use the a fake holder to
    // minimize the invasiveness of these cases.
    SymbolID Key("");
    if (auto ID = getSymbolID(D))
      Key = *ID;

    auto R = CandidatesIndex.try_emplace(Key, ResultCandidates.size());
    if (R.second) // new entry
      ResultCandidates.emplace_back();
    auto &Candidate = ResultCandidates[R.first->second];

    auto Loc = findNameLoc(D);
    auto L = makeLocation(AST, Loc, *MainFilePath);
    // The declaration in the identified symbols is a definition if possible
    // otherwise it is declaration.
    bool IsDef = getDefinition(D) == D;
    // Populate one of the slots with location for the AST.
    if (!IsDef)
      Candidate.Decl = L;
    else
      Candidate.Def = L;
  }

  if (Index) {
    LookupRequest QueryRequest;
    // Build request for index query, using SymbolID.
    for (auto It : CandidatesIndex)
      QueryRequest.IDs.insert(It.first);
    std::string TUPath;
    const FileEntry *FE = SM.getFileEntryForID(SM.getMainFileID());
    if (auto Path = getCanonicalPath(FE, SM))
      TUPath = *Path;
    // Query the index and populate the empty slot.
    Index->lookup(QueryRequest, [&TUPath, &ResultCandidates,
                                 &CandidatesIndex](const Symbol &Sym) {
      auto It = CandidatesIndex.find(Sym.ID);
      assert(It != CandidatesIndex.end());
      auto &Value = ResultCandidates[It->second];

      if (!Value.Def)
        Value.Def = toLSPLocation(Sym.Definition, TUPath);
      if (!Value.Decl)
        Value.Decl = toLSPLocation(Sym.CanonicalDeclaration, TUPath);
    });
  }

  // Populate the results, definition first.
  for (const auto &Candidate : ResultCandidates) {
    if (Candidate.Def)
      Result.push_back(*Candidate.Def);
    if (Candidate.Decl &&
        Candidate.Decl != Candidate.Def) // Decl and Def might be the same
      Result.push_back(*Candidate.Decl);
  }

  return Result;
}

namespace {

/// Collects references to symbols within the main file.
class ReferenceFinder : public index::IndexDataConsumer {
public:
  struct Reference {
    const Decl *CanonicalTarget;
    SourceLocation Loc;
    index::SymbolRoleSet Role;
  };

  ReferenceFinder(ASTContext &AST, Preprocessor &PP,
                  const std::vector<const Decl *> &TargetDecls)
      : AST(AST) {
    for (const Decl *D : TargetDecls)
      CanonicalTargets.insert(D->getCanonicalDecl());
  }

  std::vector<Reference> take() && {
    llvm::sort(References, [](const Reference &L, const Reference &R) {
      return std::tie(L.Loc, L.CanonicalTarget, L.Role) <
             std::tie(R.Loc, R.CanonicalTarget, R.Role);
    });
    // We sometimes see duplicates when parts of the AST get traversed twice.
    References.erase(
        std::unique(References.begin(), References.end(),
                    [](const Reference &L, const Reference &R) {
                      return std::tie(L.CanonicalTarget, L.Loc, L.Role) ==
                             std::tie(R.CanonicalTarget, R.Loc, R.Role);
                    }),
        References.end());
    return std::move(References);
  }

  bool
  handleDeclOccurence(const Decl *D, index::SymbolRoleSet Roles,
                      llvm::ArrayRef<index::SymbolRelation> Relations,
                      SourceLocation Loc,
                      index::IndexDataConsumer::ASTNodeInfo ASTNode) override {
    assert(D->isCanonicalDecl() && "expect D to be a canonical declaration");
    const SourceManager &SM = AST.getSourceManager();
    Loc = SM.getFileLoc(Loc);
    if (SM.isWrittenInMainFile(Loc) && CanonicalTargets.count(D))
      References.push_back({D, Loc, Roles});
    return true;
  }

private:
  llvm::SmallSet<const Decl *, 4> CanonicalTargets;
  std::vector<Reference> References;
  const ASTContext &AST;
};

std::vector<ReferenceFinder::Reference>
findRefs(const std::vector<const Decl *> &Decls, ParsedAST &AST) {
  ReferenceFinder RefFinder(AST.getASTContext(), AST.getPreprocessor(), Decls);
  index::IndexingOptions IndexOpts;
  IndexOpts.SystemSymbolFilter =
      index::IndexingOptions::SystemSymbolFilterKind::All;
  IndexOpts.IndexFunctionLocals = true;
  indexTopLevelDecls(AST.getASTContext(), AST.getPreprocessor(),
                     AST.getLocalTopLevelDecls(), RefFinder, IndexOpts);
  return std::move(RefFinder).take();
}

} // namespace

std::vector<DocumentHighlight> findDocumentHighlights(ParsedAST &AST,
                                                      Position Pos) {
  const SourceManager &SM = AST.getASTContext().getSourceManager();
  auto Symbols = getSymbolAtPosition(
      AST, getBeginningOfIdentifier(AST, Pos, SM.getMainFileID()));
  std::vector<const Decl *> TargetDecls;
  for (const DeclInfo &DI : Symbols.Decls) {
    TargetDecls.push_back(DI.D);
  }
  auto References = findRefs(TargetDecls, AST);

  std::vector<DocumentHighlight> Result;
  for (const auto &Ref : References) {
    DocumentHighlight DH;
    DH.range = getTokenRange(AST, Ref.Loc);
    if (Ref.Role & index::SymbolRoleSet(index::SymbolRole::Write))
      DH.kind = DocumentHighlightKind::Write;
    else if (Ref.Role & index::SymbolRoleSet(index::SymbolRole::Read))
      DH.kind = DocumentHighlightKind::Read;
    else
      DH.kind = DocumentHighlightKind::Text;
    Result.push_back(std::move(DH));
  }
  return Result;
}

static PrintingPolicy printingPolicyForDecls(PrintingPolicy Base) {
  PrintingPolicy Policy(Base);

  Policy.AnonymousTagLocations = false;
  Policy.TerseOutput = true;
  Policy.PolishForDeclaration = true;
  Policy.ConstantsAsWritten = true;
  Policy.SuppressTagKeyword = false;

  return Policy;
}

/// Return a string representation (e.g. "class MyNamespace::MyClass") of
/// the type declaration \p TD.
static std::string typeDeclToString(const TypeDecl *TD) {
  QualType Type = TD->getASTContext().getTypeDeclType(TD);

  PrintingPolicy Policy =
      printingPolicyForDecls(TD->getASTContext().getPrintingPolicy());

  std::string Name;
  llvm::raw_string_ostream Stream(Name);
  Type.print(Stream, Policy);

  return Stream.str();
}

/// Return a string representation (e.g. "namespace ns1::ns2") of
/// the named declaration \p ND.
static std::string namedDeclQualifiedName(const NamedDecl *ND,
                                          llvm::StringRef Prefix) {
  PrintingPolicy Policy =
      printingPolicyForDecls(ND->getASTContext().getPrintingPolicy());

  std::string Name;
  llvm::raw_string_ostream Stream(Name);
  Stream << Prefix << ' ';
  ND->printQualifiedName(Stream, Policy);

  return Stream.str();
}

/// Given a declaration \p D, return a human-readable string representing the
/// scope in which it is declared.  If the declaration is in the global scope,
/// return the string "global namespace".
static llvm::Optional<std::string> getScopeName(const Decl *D) {
  const DeclContext *DC = D->getDeclContext();

  if (isa<TranslationUnitDecl>(DC))
    return std::string("global namespace");
  if (const TypeDecl *TD = dyn_cast<TypeDecl>(DC))
    return typeDeclToString(TD);
  else if (const NamespaceDecl *ND = dyn_cast<NamespaceDecl>(DC))
    return namedDeclQualifiedName(ND, "namespace");
  else if (const FunctionDecl *FD = dyn_cast<FunctionDecl>(DC))
    return namedDeclQualifiedName(FD, "function");

  return None;
}

/// Generate a \p Hover object given the declaration \p D.
static Hover getHoverContents(const Decl *D) {
  Hover H;
  llvm::Optional<std::string> NamedScope = getScopeName(D);

  // Generate the "Declared in" section.
  if (NamedScope) {
    assert(!NamedScope->empty());

    H.contents.value += "Declared in ";
    H.contents.value += *NamedScope;
    H.contents.value += "\n\n";
  }

  // We want to include the template in the Hover.
  if (TemplateDecl *TD = D->getDescribedTemplate())
    D = TD;

  std::string DeclText;
  llvm::raw_string_ostream OS(DeclText);

  PrintingPolicy Policy =
      printingPolicyForDecls(D->getASTContext().getPrintingPolicy());

  D->print(OS, Policy);

  OS.flush();

  H.contents.value += DeclText;
  return H;
}

/// Generate a \p Hover object given the type \p T.
static Hover getHoverContents(QualType T, ASTContext &ASTCtx) {
  Hover H;
  std::string TypeText;
  llvm::raw_string_ostream OS(TypeText);
  PrintingPolicy Policy = printingPolicyForDecls(ASTCtx.getPrintingPolicy());
  T.print(OS, Policy);
  OS.flush();
  H.contents.value += TypeText;
  return H;
}

/// Generate a \p Hover object given the macro \p MacroInf.
static Hover getHoverContents(llvm::StringRef MacroName) {
  Hover H;

  H.contents.value = "#define ";
  H.contents.value += MacroName;

  return H;
}

namespace {
/// Computes the deduced type at a given location by visiting the relevant
/// nodes. We use this to display the actual type when hovering over an "auto"
/// keyword or "decltype()" expression.
/// FIXME: This could have been a lot simpler by visiting AutoTypeLocs but it
/// seems that the AutoTypeLocs that can be visited along with their AutoType do
/// not have the deduced type set. Instead, we have to go to the appropriate
/// DeclaratorDecl/FunctionDecl and work our back to the AutoType that does have
/// a deduced type set. The AST should be improved to simplify this scenario.
class DeducedTypeVisitor : public RecursiveASTVisitor<DeducedTypeVisitor> {
  SourceLocation SearchedLocation;
  llvm::Optional<QualType> DeducedType;

public:
  DeducedTypeVisitor(SourceLocation SearchedLocation)
      : SearchedLocation(SearchedLocation) {}

  llvm::Optional<QualType> getDeducedType() { return DeducedType; }

  // Handle auto initializers:
  //- auto i = 1;
  //- decltype(auto) i = 1;
  //- auto& i = 1;
  //- auto* i = &a;
  bool VisitDeclaratorDecl(DeclaratorDecl *D) {
    if (!D->getTypeSourceInfo() ||
        D->getTypeSourceInfo()->getTypeLoc().getBeginLoc() != SearchedLocation)
      return true;

    if (auto *AT = D->getType()->getContainedAutoType()) {
      if (!AT->getDeducedType().isNull())
        DeducedType = AT->getDeducedType();
    }
    return true;
  }

  // Handle auto return types:
  //- auto foo() {}
  //- auto& foo() {}
  //- auto foo() -> int {}
  //- auto foo() -> decltype(1+1) {}
  //- operator auto() const { return 10; }
  bool VisitFunctionDecl(FunctionDecl *D) {
    if (!D->getTypeSourceInfo())
      return true;
    // Loc of auto in return type (c++14).
    auto CurLoc = D->getReturnTypeSourceRange().getBegin();
    // Loc of "auto" in operator auto()
    if (CurLoc.isInvalid() && dyn_cast<CXXConversionDecl>(D))
      CurLoc = D->getTypeSourceInfo()->getTypeLoc().getBeginLoc();
    // Loc of "auto" in function with traling return type (c++11).
    if (CurLoc.isInvalid())
      CurLoc = D->getSourceRange().getBegin();
    if (CurLoc != SearchedLocation)
      return true;

    const AutoType *AT = D->getReturnType()->getContainedAutoType();
    if (AT && !AT->getDeducedType().isNull()) {
      DeducedType = AT->getDeducedType();
    } else if (auto DT = dyn_cast<DecltypeType>(D->getReturnType())) {
      // auto in a trailing return type just points to a DecltypeType and
      // getContainedAutoType does not unwrap it.
      if (!DT->getUnderlyingType().isNull())
        DeducedType = DT->getUnderlyingType();
    } else if (!D->getReturnType().isNull()) {
      DeducedType = D->getReturnType();
    }
    return true;
  }

  // Handle non-auto decltype, e.g.:
  // - auto foo() -> decltype(expr) {}
  // - decltype(expr);
  bool VisitDecltypeTypeLoc(DecltypeTypeLoc TL) {
    if (TL.getBeginLoc() != SearchedLocation)
      return true;

    // A DecltypeType's underlying type can be another DecltypeType! E.g.
    //  int I = 0;
    //  decltype(I) J = I;
    //  decltype(J) K = J;
    const DecltypeType *DT = dyn_cast<DecltypeType>(TL.getTypePtr());
    while (DT && !DT->getUnderlyingType().isNull()) {
      DeducedType = DT->getUnderlyingType();
      DT = dyn_cast<DecltypeType>(DeducedType->getTypePtr());
    }
    return true;
  }
};
} // namespace

/// Retrieves the deduced type at a given location (auto, decltype).
llvm::Optional<QualType> getDeducedType(ParsedAST &AST,
                                        SourceLocation SourceLocationBeg) {
  Token Tok;
  auto &ASTCtx = AST.getASTContext();
  // Only try to find a deduced type if the token is auto or decltype.
  if (!SourceLocationBeg.isValid() ||
      Lexer::getRawToken(SourceLocationBeg, Tok, ASTCtx.getSourceManager(),
                         ASTCtx.getLangOpts(), false) ||
      !Tok.is(tok::raw_identifier)) {
    return {};
  }
  AST.getPreprocessor().LookUpIdentifierInfo(Tok);
  if (!(Tok.is(tok::kw_auto) || Tok.is(tok::kw_decltype)))
    return {};

  DeducedTypeVisitor V(SourceLocationBeg);
  V.TraverseAST(AST.getASTContext());
  return V.getDeducedType();
}

llvm::Optional<Hover> getHover(ParsedAST &AST, Position Pos) {
  const SourceManager &SourceMgr = AST.getASTContext().getSourceManager();
  SourceLocation SourceLocationBeg =
      getBeginningOfIdentifier(AST, Pos, SourceMgr.getMainFileID());
  // Identified symbols at a specific position.
  auto Symbols = getSymbolAtPosition(AST, SourceLocationBeg);

  if (!Symbols.Macros.empty())
    return getHoverContents(Symbols.Macros[0].Name);

  if (!Symbols.Decls.empty())
    return getHoverContents(Symbols.Decls[0].D);

  auto DeducedType = getDeducedType(AST, SourceLocationBeg);
  if (DeducedType && !DeducedType->isNull())
    return getHoverContents(*DeducedType, AST.getASTContext());

  return None;
}

std::vector<Location> findReferences(ParsedAST &AST, Position Pos,
                                     uint32_t Limit, const SymbolIndex *Index) {
  if (!Limit)
    Limit = std::numeric_limits<uint32_t>::max();
  std::vector<Location> Results;
  const SourceManager &SM = AST.getASTContext().getSourceManager();
  auto MainFilePath =
      getCanonicalPath(SM.getFileEntryForID(SM.getMainFileID()), SM);
  if (!MainFilePath) {
    elog("Failed to get a path for the main file, so no references");
    return Results;
  }
  auto Loc = getBeginningOfIdentifier(AST, Pos, SM.getMainFileID());
  auto Symbols = getSymbolAtPosition(AST, Loc);

  std::vector<const Decl *> TargetDecls;
  for (const DeclInfo &DI : Symbols.Decls) {
    if (DI.IsReferencedExplicitly)
      TargetDecls.push_back(DI.D);
  }

  // We traverse the AST to find references in the main file.
  // TODO: should we handle macros, too?
  auto MainFileRefs = findRefs(TargetDecls, AST);
  for (const auto &Ref : MainFileRefs) {
    Location Result;
    Result.range = getTokenRange(AST, Ref.Loc);
    Result.uri = URIForFile::canonicalize(*MainFilePath, *MainFilePath);
    Results.push_back(std::move(Result));
  }

  // Now query the index for references from other files.
  if (Index && Results.size() < Limit) {
    RefsRequest Req;
    Req.Limit = Limit;

    for (const Decl *D : TargetDecls) {
      // Not all symbols can be referenced from outside (e.g. function-locals).
      // TODO: we could skip TU-scoped symbols here (e.g. static functions) if
      // we know this file isn't a header. The details might be tricky.
      if (D->getParentFunctionOrMethod())
        continue;
      if (auto ID = getSymbolID(D))
        Req.IDs.insert(*ID);
    }
    if (Req.IDs.empty())
      return Results;
    Index->refs(Req, [&](const Ref &R) {
      auto LSPLoc = toLSPLocation(R.Location, *MainFilePath);
      // Avoid indexed results for the main file - the AST is authoritative.
      if (LSPLoc && LSPLoc->uri.file() != *MainFilePath)
        Results.push_back(std::move(*LSPLoc));
    });
  }
  if (Results.size() > Limit)
    Results.resize(Limit);
  return Results;
}

std::vector<SymbolDetails> getSymbolInfo(ParsedAST &AST, Position Pos) {
  const SourceManager &SM = AST.getASTContext().getSourceManager();

  auto Loc = getBeginningOfIdentifier(AST, Pos, SM.getMainFileID());
  auto Symbols = getSymbolAtPosition(AST, Loc);

  std::vector<SymbolDetails> Results;

  for (const auto &Sym : Symbols.Decls) {
    SymbolDetails NewSymbol;
    if (const NamedDecl *ND = dyn_cast<NamedDecl>(Sym.D)) {
      std::string QName = printQualifiedName(*ND);
      std::tie(NewSymbol.containerName, NewSymbol.name) =
          splitQualifiedName(QName);

      if (NewSymbol.containerName.empty()) {
        if (const auto *ParentND =
                dyn_cast_or_null<NamedDecl>(ND->getDeclContext()))
          NewSymbol.containerName = printQualifiedName(*ParentND);
      }
    }
    llvm::SmallString<32> USR;
    if (!index::generateUSRForDecl(Sym.D, USR)) {
      NewSymbol.USR = USR.str();
      NewSymbol.ID = SymbolID(NewSymbol.USR);
    }
    Results.push_back(std::move(NewSymbol));
  }

  for (const auto &Macro : Symbols.Macros) {
    SymbolDetails NewMacro;
    NewMacro.name = Macro.Name;
    llvm::SmallString<32> USR;
    if (!index::generateUSRForMacro(NewMacro.name, Loc, SM, USR)) {
      NewMacro.USR = USR.str();
      NewMacro.ID = SymbolID(NewMacro.USR);
    }
    Results.push_back(std::move(NewMacro));
  }

  return Results;
}

} // namespace clangd
} // namespace clang
