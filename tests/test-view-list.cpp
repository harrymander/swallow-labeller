#include "view-list.hpp"

#include <gtest/gtest.h>

#include <iterator>
#include <stdexcept>

using recap::labeller::app::ViewList;

namespace {

template <typename T, typename Range> std::vector<T> to_vector(Range&& range)
{
    return std::vector<T>(std::begin(range), std::end(range));
}

}; // namespace

TEST(TestViewList, TestListIsSortedOnConstructionWithShuffleFalse)
{
    ViewList<int> list({5, 3, 2, 1, 4}, false);
    std::vector<int> expected_items = {1, 2, 3, 4, 5};
    ASSERT_EQ(to_vector<int>(list.items()), expected_items);
}

TEST(TestViewList, TestCurrentItemMaintainedOnShuffle)
{
    ViewList<int> list({1, 2, 3, 10, 12, 123, -100, 200}, false);
    list.set_index(4);
    int item = list.index_item();
    list.shuffle();
    ASSERT_EQ(list.index_item(), item);
}

TEST(TestViewList, TestCurrentItemMaintainedOnUnshuffle)
{
    ViewList<int> list({1, 2, 3, 10, 12, 123, -100, 200}, true);
    list.set_index(4);
    int item = list.index_item();
    list.unshuffle();
    ASSERT_EQ(list.index_item(), item);
}

TEST(TestViewList, TestNoHistoryAfterConstructionNotShuffled)
{
    ViewList<int> list({5, 3, 2, 1, 4}, false);
    EXPECT_FALSE(list.can_go_back());
    EXPECT_FALSE(list.can_go_forward());
}

TEST(TestViewList, TestNoHistoryAfterConstructionAndSettingSameIndexNotShuffled)
{
    ViewList<int> list({5, 3, 2, 1, 4}, false);
    list.set_index(0);
    EXPECT_FALSE(list.can_go_back());
    EXPECT_FALSE(list.can_go_forward());
}

TEST(TestViewList, TestNoHistoryAfterConstructionShuffled)
{
    ViewList<int> list({5, 3, 2, 1, 4}, true);
    EXPECT_FALSE(list.can_go_back());
    EXPECT_FALSE(list.can_go_forward());
}

TEST(TestViewList, TestNoHistoryAfterConstructionAndSettingSameIndexShuffled)
{
    ViewList<int> list({5, 3, 2, 1, 4}, true);
    list.set_index(0);
    EXPECT_FALSE(list.can_go_back());
    EXPECT_FALSE(list.can_go_forward());
}

TEST(TestViewList, TestGoBack)
{
    ViewList<int> list({1, 2, 3, 4, 5}, false);
    list.set_index(3);
    list.set_index(4);

    ASSERT_TRUE(list.can_go_back());
    ASSERT_EQ(list.index(), 4);

    list.go_back();
    ASSERT_EQ(list.index(), 3);
    ASSERT_TRUE(list.can_go_back());

    list.go_back();
    ASSERT_EQ(list.index(), 0);
    ASSERT_FALSE(list.can_go_back());
}

TEST(TestViewList, TestSettingSameIndexDoesNotAddToHistory)
{
    ViewList<int> list({1, 2, 3, 4, 5}, false);
    list.set_index(0);
    list.set_index(0);
    ASSERT_FALSE(list.can_go_back());

    list.set_index(2);
    list.set_index(2);
    ASSERT_TRUE(list.can_go_back());
    list.go_back();
    ASSERT_EQ(list.index(), 0);
    ASSERT_FALSE(list.can_go_back());
}

TEST(TestViewList, TestGoForward)
{
    ViewList<int> list({1, 2, 3, 4, 5}, false);

    list.set_index(1);
    list.set_index(2);
    list.go_back();
    list.go_back();

    ASSERT_TRUE(list.can_go_forward());
    list.go_forward();
    ASSERT_EQ(list.index(), 1);

    ASSERT_TRUE(list.can_go_forward());
    list.go_forward();
    ASSERT_EQ(list.index(), 2);

    ASSERT_FALSE(list.can_go_forward());
}

TEST(TestViewList, TestGoBackAndForward)
{
    ViewList<int> list({1, 2, 3, 4, 5}, false);

    list.set_index(1);
    list.set_index(2);
    list.set_index(3);

    list.go_back();
    ASSERT_EQ(list.index(), 2);

    ASSERT_TRUE(list.can_go_back());
    list.go_back();
    ASSERT_EQ(list.index(), 1);

    ASSERT_TRUE(list.can_go_back());
    list.go_back();
    ASSERT_EQ(list.index(), 0);
    ASSERT_FALSE(list.can_go_back());

    ASSERT_TRUE(list.can_go_forward());
    list.go_forward();
    ASSERT_EQ(list.index(), 1);

    ASSERT_TRUE(list.can_go_forward());
    list.go_forward();
    ASSERT_EQ(list.index(), 2);

    ASSERT_TRUE(list.can_go_forward());
    list.go_forward();
    ASSERT_EQ(list.index(), 3);
    ASSERT_FALSE(list.can_go_forward());

    ASSERT_TRUE(list.can_go_back());
    list.go_back();
    ASSERT_EQ(list.index(), 2);

    list.go_back();
    ASSERT_EQ(list.index(), 1);

    ASSERT_TRUE(list.can_go_back());
    list.go_back();
    ASSERT_EQ(list.index(), 0);
    ASSERT_FALSE(list.can_go_back());
}

static ViewList<int> view_list_with_back_and_forward_histories(bool shuffled)
{
    ViewList<int> list({1, 2, 3, 4, 5}, shuffled);
    list.set_index(1);
    list.set_index(2);
    list.set_index(3);
    list.go_back();
    list.go_back();

    if (!list.can_go_back()) {
        throw std::logic_error("Cannot go back!");
    }
    if (!list.can_go_forward()) {
        throw std::logic_error("Cannot go forward!");
    }

    return list;
}

TEST(TestViewList, TestSettingIndexResetsForwardHistory)
{
    auto list = view_list_with_back_and_forward_histories(false);

    ASSERT_TRUE(list.can_go_forward());
    list.set_index(4);
    ASSERT_FALSE(list.can_go_forward());

    ASSERT_TRUE(list.can_go_back());
    list.go_back();
    ASSERT_EQ(list.index(), 1);

    ASSERT_TRUE(list.can_go_back());
    list.go_back();
    ASSERT_EQ(list.index(), 0);
    ASSERT_FALSE(list.can_go_back());
}

TEST(TestViewList, TestSettingSameIndexDoesNotResetForwardHistory)
{
    auto list = view_list_with_back_and_forward_histories(false);

    ASSERT_EQ(list.index(), 1);
    list.set_index(1);

    ASSERT_TRUE(list.can_go_forward());
    list.go_forward();
    ASSERT_EQ(list.index(), 2);

    ASSERT_TRUE(list.can_go_forward());
    list.go_forward();
    ASSERT_EQ(list.index(), 3);
    ASSERT_FALSE(list.can_go_forward());
}

TEST(TestViewList, TestShufflingResetsHistory)
{
    auto list = view_list_with_back_and_forward_histories(false);

    list.shuffle();
    ASSERT_FALSE(list.can_go_back());
    ASSERT_FALSE(list.can_go_forward());
}

TEST(TestViewList, TestUnshufflingResetsHistory)
{
    auto list = view_list_with_back_and_forward_histories(true);

    list.unshuffle();
    ASSERT_FALSE(list.can_go_back());
    ASSERT_FALSE(list.can_go_forward());
}

TEST(TestViewList, TestShufflingAlreadyShuffledDoesNotResetHistory)
{
    auto list = view_list_with_back_and_forward_histories(true);

    list.shuffle();
    ASSERT_TRUE(list.can_go_back());
    ASSERT_TRUE(list.can_go_forward());
}

TEST(TestViewList, TestUnshufflingAlreadyUnshuffledDoesNotResetHistory)
{
    auto list = view_list_with_back_and_forward_histories(false);

    list.unshuffle();
    ASSERT_TRUE(list.can_go_back());
    ASSERT_TRUE(list.can_go_forward());
}
