//===-- SourceCodeTests.cpp  ------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
#include "SourceCode.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/raw_os_ostream.h"
#include "llvm/Testing/Support/Error.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace clang {
namespace clangd {
namespace {

MATCHER_P2(Pos, Line, Col, "") {
  return arg.line == Line && arg.character == Col;
}

// The = → 🡆 below are ASCII (1 byte), BMP (3 bytes), and astral (4 bytes).
const char File[] = R"(0:0 = 0
1:0 → 8
2:0 🡆 18)";

/// A helper to make tests easier to read.
Position position(int line, int character) {
  Position Pos;
  Pos.line = line;
  Pos.character = character;
  return Pos;
}

Range range(const std::pair<int, int> p1, const std::pair<int, int> p2) {
  Range range;
  range.start = position(p1.first, p1.second);
  range.end = position(p2.first, p2.second);
  return range;
}

TEST(SourceCodeTests, lspLength) {
  EXPECT_EQ(lspLength(""), 0UL);
  EXPECT_EQ(lspLength("ascii"), 5UL);
  // BMP
  EXPECT_EQ(lspLength("↓"), 1UL);
  EXPECT_EQ(lspLength("¥"), 1UL);
  // astral
  EXPECT_EQ(lspLength("😂"), 2UL);
}

TEST(SourceCodeTests, PositionToOffset) {
  // line out of bounds
  EXPECT_THAT_EXPECTED(positionToOffset(File, position(-1, 2)), llvm::Failed());
  // first line
  EXPECT_THAT_EXPECTED(positionToOffset(File, position(0, -1)),
                       llvm::Failed()); // out of range
  EXPECT_THAT_EXPECTED(positionToOffset(File, position(0, 0)),
                       llvm::HasValue(0)); // first character
  EXPECT_THAT_EXPECTED(positionToOffset(File, position(0, 3)),
                       llvm::HasValue(3)); // middle character
  EXPECT_THAT_EXPECTED(positionToOffset(File, position(0, 6)),
                       llvm::HasValue(6)); // last character
  EXPECT_THAT_EXPECTED(positionToOffset(File, position(0, 7)),
                       llvm::HasValue(7)); // the newline itself
  EXPECT_THAT_EXPECTED(positionToOffset(File, position(0, 7), false),
                       llvm::HasValue(7));
  EXPECT_THAT_EXPECTED(positionToOffset(File, position(0, 8)),
                       llvm::HasValue(7)); // out of range
  EXPECT_THAT_EXPECTED(positionToOffset(File, position(0, 8), false),
                       llvm::Failed()); // out of range
  // middle line
  EXPECT_THAT_EXPECTED(positionToOffset(File, position(1, -1)),
                       llvm::Failed()); // out of range
  EXPECT_THAT_EXPECTED(positionToOffset(File, position(1, 0)),
                       llvm::HasValue(8)); // first character
  EXPECT_THAT_EXPECTED(positionToOffset(File, position(1, 3)),
                       llvm::HasValue(11)); // middle character
  EXPECT_THAT_EXPECTED(positionToOffset(File, position(1, 3), false),
                       llvm::HasValue(11));
  EXPECT_THAT_EXPECTED(positionToOffset(File, position(1, 6)),
                       llvm::HasValue(16)); // last character
  EXPECT_THAT_EXPECTED(positionToOffset(File, position(1, 7)),
                       llvm::HasValue(17)); // the newline itself
  EXPECT_THAT_EXPECTED(positionToOffset(File, position(1, 8)),
                       llvm::HasValue(17)); // out of range
  EXPECT_THAT_EXPECTED(positionToOffset(File, position(1, 8), false),
                       llvm::Failed()); // out of range
  // last line
  EXPECT_THAT_EXPECTED(positionToOffset(File, position(2, -1)),
                       llvm::Failed()); // out of range
  EXPECT_THAT_EXPECTED(positionToOffset(File, position(2, 0)),
                       llvm::HasValue(18)); // first character
  EXPECT_THAT_EXPECTED(positionToOffset(File, position(2, 3)),
                       llvm::HasValue(21)); // middle character
  EXPECT_THAT_EXPECTED(positionToOffset(File, position(2, 5), false),
                       llvm::Failed()); // middle of surrogate pair
  EXPECT_THAT_EXPECTED(positionToOffset(File, position(2, 5)),
                       llvm::HasValue(26)); // middle of surrogate pair
  EXPECT_THAT_EXPECTED(positionToOffset(File, position(2, 6), false),
                       llvm::HasValue(26)); // end of surrogate pair
  EXPECT_THAT_EXPECTED(positionToOffset(File, position(2, 8)),
                       llvm::HasValue(28)); // last character
  EXPECT_THAT_EXPECTED(positionToOffset(File, position(2, 9)),
                       llvm::HasValue(29)); // EOF
  EXPECT_THAT_EXPECTED(positionToOffset(File, position(2, 10), false),
                       llvm::Failed()); // out of range
  // line out of bounds
  EXPECT_THAT_EXPECTED(positionToOffset(File, position(3, 0)), llvm::Failed());
  EXPECT_THAT_EXPECTED(positionToOffset(File, position(3, 1)), llvm::Failed());
}

TEST(SourceCodeTests, OffsetToPosition) {
  EXPECT_THAT(offsetToPosition(File, 0), Pos(0, 0)) << "start of file";
  EXPECT_THAT(offsetToPosition(File, 3), Pos(0, 3)) << "in first line";
  EXPECT_THAT(offsetToPosition(File, 6), Pos(0, 6)) << "end of first line";
  EXPECT_THAT(offsetToPosition(File, 7), Pos(0, 7)) << "first newline";
  EXPECT_THAT(offsetToPosition(File, 8), Pos(1, 0)) << "start of second line";
  EXPECT_THAT(offsetToPosition(File, 12), Pos(1, 4)) << "before BMP char";
  EXPECT_THAT(offsetToPosition(File, 13), Pos(1, 5)) << "in BMP char";
  EXPECT_THAT(offsetToPosition(File, 15), Pos(1, 5)) << "after BMP char";
  EXPECT_THAT(offsetToPosition(File, 16), Pos(1, 6)) << "end of second line";
  EXPECT_THAT(offsetToPosition(File, 17), Pos(1, 7)) << "second newline";
  EXPECT_THAT(offsetToPosition(File, 18), Pos(2, 0)) << "start of last line";
  EXPECT_THAT(offsetToPosition(File, 21), Pos(2, 3)) << "in last line";
  EXPECT_THAT(offsetToPosition(File, 22), Pos(2, 4)) << "before astral char";
  EXPECT_THAT(offsetToPosition(File, 24), Pos(2, 6)) << "in astral char";
  EXPECT_THAT(offsetToPosition(File, 26), Pos(2, 6)) << "after astral char";
  EXPECT_THAT(offsetToPosition(File, 28), Pos(2, 8)) << "end of last line";
  EXPECT_THAT(offsetToPosition(File, 29), Pos(2, 9)) << "EOF";
  EXPECT_THAT(offsetToPosition(File, 30), Pos(2, 9)) << "out of bounds";
}

TEST(SourceCodeTests, IsRangeConsecutive) {
  EXPECT_TRUE(IsRangeConsecutive(range({2, 2}, {2, 3}), range({2, 3}, {2, 4})));
  EXPECT_FALSE(
      IsRangeConsecutive(range({0, 2}, {0, 3}), range({2, 3}, {2, 4})));
  EXPECT_FALSE(
      IsRangeConsecutive(range({2, 2}, {2, 3}), range({2, 4}, {2, 5})));
}

} // namespace
} // namespace clangd
} // namespace clang
