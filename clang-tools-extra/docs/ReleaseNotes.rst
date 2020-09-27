=====================================
Extra Clang Tools 8.0.0 Release Notes
=====================================

.. contents::
   :local:
   :depth: 3

Written by the `LLVM Team <https://llvm.org/>`_

Introduction
============

This document contains the release notes for the Extra Clang Tools, part of the
Clang release 8.0.0. Here we describe the status of the Extra Clang Tools in
some detail, including major improvements from the previous release and new
feature work. All LLVM releases may be downloaded
from the `LLVM releases web site <https://releases.llvm.org/>`_.

For more information about Clang or LLVM, including information about
the latest release, please see the `Clang Web Site <https://clang.llvm.org>`_ or
the `LLVM Web Site <https://llvm.org>`_.

What's New in Extra Clang Tools 8.0.0?
======================================

Some of the major new features and improvements to Extra Clang Tools are listed
here. Generic improvements to Extra Clang Tools as a whole or to its underlying
infrastructure are described first, followed by tool-specific sections.


Improvements to clangd
----------------------

- clangd now adds namespace qualifiers in code completion, for example, if you
  type "``vec``", the list of completions will include "``std::vector``".

  See also: `r343248 <https://reviews.llvm.org/rL343248>`__.

- When a :ref:`global index <project-wide-index>` is available, clangd will use it to augment the
  results of "go to definition" and "find references" queries. Global index
  also enables global code completion, which suggests symbols that are not
  imported in the current file and automatically inserts the missing
  ``#include`` directives.

- clangd stores the symbol index on disk in a new compact binary serialization
  format.  It is 10x more compact than YAML and 40% more compact than gzipped
  YAML.

  See also: `r341375 <https://reviews.llvm.org/rL341375>`__.

- clangd has a new efficient symbol index suitable for complex and fuzzy
  queries and large code bases (e.g., LLVM, Chromium).  This index is used for
  code completion, go to definition, and cross-references.  The architecture of
  the index allows for complex and fuzzy retrieval criteria and sophisticated
  scoring.

  See also: `discussion on the mailing list
  <http://lists.llvm.org/pipermail/cfe-dev/2018-July/058487.html>`__, `design
  doc
  <https://docs.google.com/document/d/1C-A6PGT6TynyaX4PXyExNMiGmJ2jL1UwV91Kyx11gOI/edit>`__.

- clangd has a new LSP extension that communicates information about activity
  on clangd's per-file worker thread.  This information can be displayed to
  users to let them know that the language server is busy with something.  For
  example, in clangd, building the AST blocks many other operations.

  More info: :ref:`lsp-extension-file-status`.

- clangd has a new LSP extension that allows the client to supply the
  compilation commands over LSP, instead of finding compile_commands.json on
  disk.

  More info: :ref:`lsp-extension-compilation-commands`.

- clangd has a new LSP extension that allows the client to request fixes to be
  sent together with diagnostics, instead of asynchronously.

  More info: :ref:`lsp-extension-code-actions-in-diagnostics`.

- clangd has a new LSP extension that allows the client to resolve a symbol in
  a light-weight manner, without retrieving further information (like
  definition location, which may require consulting an index).

  More info: :ref:`lsp-extension-symbol-info`.


Improvements to clang-query
---------------------------

- A new command line parameter ``--preload`` was added to
  run commands from a file and then start the interactive interpreter.

- The command ``q`` can was added as an alias for ``quit`` to exit the
  ``clang-query`` interpreter.

- It is now possible to bind to named values (the result of ``let``
  expressions). For example:

  .. code-block:: none

    let fn functionDecl()
    match fn.bind("foo")

- It is now possible to write comments in ``clang-query`` code. This
  is primarily useful when using script-mode. Comments are all content
  following the ``#`` character on a line:

  .. code-block:: none

    # This is a comment
    match fn.bind("foo") # This is a trailing comment

- The new ``set print-matcher true`` command now causes ``clang-query`` to
  print the evaluated matcher together with the resulting bindings.

- A new output mode ``detailed-ast`` was added to ``clang-query``. The
  existing ``dump`` output mode is now a deprecated alias
  for ``detailed-ast``

- Output modes can now be enabled or disabled non-exclusively.  For example,

  .. code-block:: none

    # Enable detailed-ast without disabling other output, such as diag
    enable output detailed-ast
    m functionDecl()

    # Disable detailed-ast only
    disable output detailed-ast
    m functionDecl()

Improvements to clang-tidy
--------------------------

- New :doc:`abseil-duration-comparison
  <clang-tidy/checks/abseil-duration-comparison>` check.

  Checks for comparisons which should be done in the ``absl::Duration`` domain
  instead of the float of integer domains.

- New :doc:`abseil-duration-division
  <clang-tidy/checks/abseil-duration-division>` check.

  Checks for uses of ``absl::Duration`` division that is done in a
  floating-point context, and recommends the use of a function that
  returns a floating-point value.

- New :doc:`abseil-duration-factory-float
  <clang-tidy/checks/abseil-duration-factory-float>` check.

  Checks for cases where the floating-point overloads of various
  ``absl::Duration`` factory functions are called when the more-efficient
  integer versions could be used instead.

- New :doc:`abseil-duration-factory-scale
  <clang-tidy/checks/abseil-duration-factory-scale>` check.

  Checks for cases where arguments to ``absl::Duration`` factory functions are
  scaled internally and could be changed to a different factory function.

- New :doc:`abseil-duration-subtraction
  <clang-tidy/checks/abseil-duration-subtraction>` check.

  Checks for cases where subtraction should be performed in the
  ``absl::Duration`` domain.

- New :doc:`abseil-faster-strsplit-delimiter
  <clang-tidy/checks/abseil-faster-strsplit-delimiter>` check.

  Finds instances of ``absl::StrSplit()`` or ``absl::MaxSplits()`` where the
  delimiter is a single character string literal and replaces with a character.

- New :doc:`abseil-no-internal-dependencies
  <clang-tidy/checks/abseil-no-internal-dependencies>` check.

  Gives a warning if code using Abseil depends on internal details.

- New :doc:`abseil-no-namespace
  <clang-tidy/checks/abseil-no-namespace>` check.

  Ensures code does not open ``namespace absl`` as that violates Abseil's
  compatibility guidelines.

- New :doc:`abseil-redundant-strcat-calls
  <clang-tidy/checks/abseil-redundant-strcat-calls>` check.

  Suggests removal of unnecessary calls to ``absl::StrCat`` when the result is
  being passed to another ``absl::StrCat`` or ``absl::StrAppend``.

- New :doc:`abseil-str-cat-append
  <clang-tidy/checks/abseil-str-cat-append>` check.

  Flags uses of ``absl::StrCat()`` to append to a ``std::string``. Suggests
  ``absl::StrAppend()`` should be used instead.

- New :doc:`abseil-upgrade-duration-conversions
  <clang-tidy/checks/abseil-upgrade-duration-conversions>` check.

  Finds calls to ``absl::Duration`` arithmetic operators and factories whose
  argument needs an explicit cast to continue compiling after upcoming API
  changes.

- New :doc:`bugprone-too-small-loop-variable
  <clang-tidy/checks/bugprone-too-small-loop-variable>` check.

  Detects those ``for`` loops that have a loop variable with a "too small" type
  which means this type can't represent all values which are part of the
  iteration range.

- New :doc:`cppcoreguidelines-macro-usage
  <clang-tidy/checks/cppcoreguidelines-macro-usage>` check.

  Finds macro usage that is considered problematic because better language
  constructs exist for the task.

- New :doc:`google-objc-function-naming
  <clang-tidy/checks/google-objc-function-naming>` check.

  Checks that function names in function declarations comply with the naming
  conventions described in the Google Objective-C Style Guide.

- New :doc:`misc-non-private-member-variables-in-classes
  <clang-tidy/checks/misc-non-private-member-variables-in-classes>` check.

  Finds classes that not only contain the data (non-static member variables),
  but also have logic (non-static member functions), and diagnoses all member
  variables that have any other scope other than ``private``.

- New :doc:`modernize-avoid-c-arrays
  <clang-tidy/checks/modernize-avoid-c-arrays>` check.

  Finds C-style array types and recommend to use ``std::array<>`` /
  ``std::vector<>``.

- New :doc:`modernize-concat-nested-namespaces
  <clang-tidy/checks/modernize-concat-nested-namespaces>` check.

  Checks for uses of nested namespaces in the form of
  ``namespace a { namespace b { ... }}`` and offers change to
  syntax introduced in C++17 standard: ``namespace a::b { ... }``.

- New :doc:`modernize-deprecated-ios-base-aliases
  <clang-tidy/checks/modernize-deprecated-ios-base-aliases>` check.

  Detects usage of the deprecated member types of ``std::ios_base`` and replaces
  those that have a non-deprecated equivalent.

- New :doc:`modernize-use-nodiscard
  <clang-tidy/checks/modernize-use-nodiscard>` check.

  Adds ``[[nodiscard]]`` attributes (introduced in C++17) to member functions
  to highlight at compile time which return values should not be ignored.

- New :doc:`readability-const-return-type
  <clang-tidy/checks/readability-const-return-type>` check.

  Checks for functions with a ``const``-qualified return type and recommends
  removal of the ``const`` keyword.

- New :doc:`readability-isolate-decl
  <clang-tidy/checks/readability-isolate-declaration>` check.

  Detects local variable declarations declaring more than one variable and
  tries to refactor the code to one statement per declaration.

- New :doc:`readability-magic-numbers
  <clang-tidy/checks/readability-magic-numbers>` check.

  Detects usage of magic numbers, numbers that are used as literals instead of
  introduced via constants or symbols.

- New :doc:`readability-redundant-preprocessor
  <clang-tidy/checks/readability-redundant-preprocessor>` check.

  Finds potentially redundant preprocessor directives.

- New :doc:`readability-uppercase-literal-suffix
  <clang-tidy/checks/readability-uppercase-literal-suffix>` check.

  Detects when the integral literal or floating point literal has non-uppercase
  suffix, and suggests to make the suffix uppercase. The list of destination
  suffixes can be optionally provided.

- New alias :doc:`cert-dcl16-c
  <clang-tidy/checks/cert-dcl16-c>` to :doc:`readability-uppercase-literal-suffix
  <clang-tidy/checks/readability-uppercase-literal-suffix>`
  added.

- New alias :doc:`cppcoreguidelines-avoid-c-arrays
  <clang-tidy/checks/cppcoreguidelines-avoid-c-arrays>`
  to :doc:`modernize-avoid-c-arrays
  <clang-tidy/checks/modernize-avoid-c-arrays>` added.

- New alias :doc:`cppcoreguidelines-non-private-member-variables-in-classes
  <clang-tidy/checks/cppcoreguidelines-non-private-member-variables-in-classes>`
  to :doc:`misc-non-private-member-variables-in-classes
  <clang-tidy/checks/misc-non-private-member-variables-in-classes>`
  added.

- New alias :doc:`hicpp-avoid-c-arrays
  <clang-tidy/checks/hicpp-avoid-c-arrays>`
  to :doc:`modernize-avoid-c-arrays
  <clang-tidy/checks/modernize-avoid-c-arrays>` added.

- New alias :doc:`hicpp-uppercase-literal-suffix
  <clang-tidy/checks/hicpp-uppercase-literal-suffix>` to
  :doc:`readability-uppercase-literal-suffix
  <clang-tidy/checks/readability-uppercase-literal-suffix>`
  added.

- The :doc:`cppcoreguidelines-narrowing-conversions
  <clang-tidy/checks/cppcoreguidelines-narrowing-conversions>` check now
  detects more narrowing conversions:
  - integer to narrower signed integer (this is compiler implementation defined),
  - integer - floating point narrowing conversions,
  - floating point - integer narrowing conversions,
  - constants with narrowing conversions (even in ternary operator).

- The :doc:`objc-property-declaration
  <clang-tidy/checks/objc-property-declaration>` check now ignores the
  `Acronyms` and `IncludeDefaultAcronyms` options.

- The :doc:`readability-redundant-smartptr-get
  <clang-tidy/checks/readability-redundant-smartptr-get>` check does not warn
  about calls inside macros anymore by default.

- The :doc:`readability-uppercase-literal-suffix
  <clang-tidy/checks/readability-uppercase-literal-suffix>` check does not warn
  about literal suffixes inside macros anymore by default.
