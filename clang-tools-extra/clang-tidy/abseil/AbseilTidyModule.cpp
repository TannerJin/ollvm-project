//===------- AbseilTidyModule.cpp - clang-tidy ----------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "../ClangTidy.h"
#include "../ClangTidyModule.h"
#include "../ClangTidyModuleRegistry.h"
#include "DurationComparisonCheck.h"
#include "DurationDivisionCheck.h"
#include "DurationFactoryFloatCheck.h"
#include "DurationFactoryScaleCheck.h"
#include "DurationSubtractionCheck.h"
#include "FasterStrsplitDelimiterCheck.h"
#include "NoInternalDependenciesCheck.h"
#include "NoNamespaceCheck.h"
#include "RedundantStrcatCallsCheck.h"
#include "StringFindStartswithCheck.h"
#include "StrCatAppendCheck.h"
#include "UpgradeDurationConversionsCheck.h"

namespace clang {
namespace tidy {
namespace abseil {

class AbseilModule : public ClangTidyModule {
public:
  void addCheckFactories(ClangTidyCheckFactories &CheckFactories) override {
    CheckFactories.registerCheck<DurationComparisonCheck>(
        "abseil-duration-comparison");
    CheckFactories.registerCheck<DurationDivisionCheck>(
        "abseil-duration-division");
    CheckFactories.registerCheck<DurationFactoryFloatCheck>(
        "abseil-duration-factory-float");
    CheckFactories.registerCheck<DurationFactoryScaleCheck>(
        "abseil-duration-factory-scale");
    CheckFactories.registerCheck<DurationSubtractionCheck>(
        "abseil-duration-subtraction");
    CheckFactories.registerCheck<FasterStrsplitDelimiterCheck>(
        "abseil-faster-strsplit-delimiter");
    CheckFactories.registerCheck<NoInternalDependenciesCheck>(
        "abseil-no-internal-dependencies");
    CheckFactories.registerCheck<NoNamespaceCheck>("abseil-no-namespace");
    CheckFactories.registerCheck<RedundantStrcatCallsCheck>(
        "abseil-redundant-strcat-calls");
    CheckFactories.registerCheck<StrCatAppendCheck>(
        "abseil-str-cat-append");
    CheckFactories.registerCheck<StringFindStartswithCheck>(
        "abseil-string-find-startswith");
    CheckFactories.registerCheck<UpgradeDurationConversionsCheck>(
        "abseil-upgrade-duration-conversions");
  }
};

// Register the AbseilModule using this statically initialized variable.
static ClangTidyModuleRegistry::Add<AbseilModule> X("abseil-module",
                                                    "Add Abseil checks.");

} // namespace abseil

// This anchor is used to force the linker to link in the generated object file
// and thus register the AbseilModule.
volatile int AbseilModuleAnchorSource = 0;

} // namespace tidy
} // namespace clang
