#include "gui/widgets/integer-range.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <limits>
#include <vector>

using namespace recap::labeller::gui::widgets;

namespace {

using UIntRange = IntegerRange<unsigned int>;
using UIntList = std::vector<UIntRange>;

constexpr auto UINT_MAX_V = std::numeric_limits<unsigned int>::max();

} // namespace

TEST(TestParseIntegerRange, SingleNumber)
{
    UIntRange r;
    ASSERT_TRUE(parse_integer_range("5", r));
    EXPECT_EQ(r, (UIntRange{5, 5}));
}

TEST(TestParseIntegerRange, ZeroValue)
{
    UIntRange r;
    ASSERT_TRUE(parse_integer_range("0", r));
    EXPECT_EQ(r, (UIntRange{0, 0}));
}

TEST(TestParseIntegerRange, ClosedRange)
{
    UIntRange r;
    ASSERT_TRUE(parse_integer_range("3-7", r));
    EXPECT_EQ(r, (UIntRange{3, 7}));
}

TEST(TestParseIntegerRange, EqualBounds)
{
    UIntRange r;
    ASSERT_TRUE(parse_integer_range("5-5", r));
    EXPECT_EQ(r, (UIntRange{5, 5}));
}

TEST(TestParseIntegerRange, OpenEndedRange)
{
    UIntRange r;
    ASSERT_TRUE(parse_integer_range("10-", r));
    EXPECT_EQ(r, (UIntRange{10, UINT_MAX_V}));
}

TEST(TestParseIntegerRange, ReversedBoundsAreSwapped)
{
    UIntRange r;
    ASSERT_TRUE(parse_integer_range("10-3", r));
    EXPECT_EQ(r, (UIntRange{3, 10}));
}

TEST(TestParseIntegerRange, WhitespaceAroundDash)
{
    UIntRange r;
    ASSERT_TRUE(parse_integer_range("1 - 5", r));
    EXPECT_EQ(r, (UIntRange{1, 5}));
}

TEST(TestParseIntegerRange, WhitespaceBeforeOpenEnd)
{
    UIntRange r;
    ASSERT_TRUE(parse_integer_range("7 -", r));
    EXPECT_EQ(r, (UIntRange{7, UINT_MAX_V}));
}

TEST(TestParseIntegerRange, RejectsLeadingDash)
{
    UIntRange r;
    EXPECT_FALSE(parse_integer_range("-5", r));
}

TEST(TestParseIntegerRange, RejectsDashOnly)
{
    UIntRange r;
    EXPECT_FALSE(parse_integer_range("-", r));
}

TEST(TestParseIntegerRange, RejectsNonNumeric)
{
    UIntRange r;
    EXPECT_FALSE(parse_integer_range("abc", r));
    EXPECT_FALSE(parse_integer_range("1-x", r));
    EXPECT_FALSE(parse_integer_range("x-2", r));
}

TEST(TestParseIntegerRange, RejectsExtraDash)
{
    UIntRange r;
    EXPECT_FALSE(parse_integer_range("1-2-3", r));
}

TEST(TestParseIntegerRange, RejectsOverflow)
{
    UIntRange r;
    // 99999999999 > max uint32 (~4.3e9); the value will overflow even uint64
    // is irrelevant -- it doesn't fit in unsigned int.
    EXPECT_FALSE(parse_integer_range("99999999999", r));
    EXPECT_FALSE(parse_integer_range("1-99999999999", r));
}

TEST(TestParseIntegerRangeList, EmptyInput)
{
    UIntList ranges{{1, 2}}; // pre-populate to verify untouched on success-with-empty
    ASSERT_TRUE(parse_integer_range_list("", ranges));
    EXPECT_TRUE(ranges.empty());
}

TEST(TestParseIntegerRangeList, WhitespaceOnly)
{
    UIntList ranges;
    ASSERT_TRUE(parse_integer_range_list("   ", ranges));
    EXPECT_TRUE(ranges.empty());
}

TEST(TestParseIntegerRangeList, SingleNumber)
{
    UIntList ranges;
    ASSERT_TRUE(parse_integer_range_list("5", ranges));
    EXPECT_EQ(ranges, (UIntList{{5, 5}}));
}

TEST(TestParseIntegerRangeList, MultipleTokens)
{
    UIntList ranges;
    ASSERT_TRUE(parse_integer_range_list("1, 3-5, 10-", ranges));
    EXPECT_EQ(ranges, (UIntList{{1, 1}, {3, 5}, {10, UINT_MAX_V}}));
}

TEST(TestParseIntegerRangeList, SortsOutput)
{
    UIntList ranges;
    ASSERT_TRUE(parse_integer_range_list("5, 1-2", ranges));
    EXPECT_EQ(ranges, (UIntList{{1, 2}, {5, 5}}));
}

TEST(TestParseIntegerRangeList, WhitespaceTolerance)
{
    UIntList ranges;
    ASSERT_TRUE(parse_integer_range_list(" 1 , 2 - 3 ", ranges));
    EXPECT_EQ(ranges, (UIntList{{1, 1}, {2, 3}}));
}

TEST(TestParseIntegerRangeList, TrailingCommaAllowed)
{
    UIntList ranges;
    ASSERT_TRUE(parse_integer_range_list("1,", ranges));
    EXPECT_EQ(ranges, (UIntList{{1, 1}}));
}

TEST(TestParseIntegerRangeList, TrailingCommaWithWhitespace)
{
    UIntList ranges;
    ASSERT_TRUE(parse_integer_range_list("1, 2-3,  ", ranges));
    EXPECT_EQ(ranges, (UIntList{{1, 1}, {2, 3}}));
}

TEST(TestParseIntegerRangeList, RejectsDoubleComma)
{
    UIntList ranges;
    EXPECT_FALSE(parse_integer_range_list("1,,2", ranges));
}

TEST(TestParseIntegerRangeList, RejectsLeadingComma)
{
    UIntList ranges;
    EXPECT_FALSE(parse_integer_range_list(",1", ranges));
}

TEST(TestParseIntegerRangeList, RejectsInvalidToken)
{
    UIntList ranges;
    EXPECT_FALSE(parse_integer_range_list("1, abc, 2", ranges));
}

TEST(TestParseIntegerRangeList, RejectsLeadingDashInToken)
{
    UIntList ranges;
    EXPECT_FALSE(parse_integer_range_list("1, -5, 7", ranges));
}

TEST(TestParseIntegerRangeList, WorksWithSizeT)
{
    std::vector<IntegerRange<std::size_t>> ranges;
    ASSERT_TRUE(parse_integer_range_list("1-100, 200-", ranges));
    ASSERT_EQ(ranges.size(), 2U);
    EXPECT_EQ(ranges[0], (IntegerRange<std::size_t>{1, 100}));
    EXPECT_EQ(ranges[1], (IntegerRange<std::size_t>{200, std::numeric_limits<std::size_t>::max()}));
}
