// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include <base/logging.h>
#include <gtest/gtest.h>

#include "gestures/include/map.h"

using std::pair;
using std::make_pair;

namespace gestures {

class MapTest : public ::testing::Test {};

TEST(MapTest, SimpleTest) {
  const int kMax = 5;

  map<int, pair<int, int>, kMax> map_a;
  map<int, pair<int, int>, kMax> map_b;
  EXPECT_TRUE(map_a.empty());
  EXPECT_TRUE(map_a == map_b);
  map_a[2] = make_pair(1, 4);
  EXPECT_EQ(1, map_a[2].first);
  EXPECT_EQ(4, map_a[2].second);
  for (map<int, pair<int, int>, kMax>::iterator it = map_a.begin(),
           e = map_a.end(); it != e; ++it) {
    EXPECT_EQ(2, (*it).first);
    EXPECT_TRUE(make_pair(1, 4) == (*it).second);
  }

  map_a.insert(make_pair(10, make_pair(5, 6)));
  EXPECT_FALSE(map_a.empty());
  EXPECT_EQ(2, map_a.size());
  map_b = map_a;
  EXPECT_FALSE(map_b != map_a);
  EXPECT_EQ(1, map_a.erase(10));
  EXPECT_TRUE(map_b != map_a);
  EXPECT_FALSE(map_a.empty());
  map_a.clear();
  EXPECT_TRUE(map_a.empty());
  EXPECT_FALSE(map_b.empty());
  map_b = map_a;
  EXPECT_TRUE(map_b.empty());
}

TEST(MapTest, SizeTest) {
  map<int, int, 2> small;
  map<int, int, 3> big;
  big[2] = 20;
  big[3] = 30;
  big[4] = 40;
  big[5] = 50;
  EXPECT_TRUE(big.find(2) != big.end());
  EXPECT_TRUE(big.find(3) != big.end());
  EXPECT_TRUE(big.find(4) != big.end());
  EXPECT_TRUE(big.find(5) == big.end());
  EXPECT_EQ(1, big.erase(4));
  EXPECT_EQ(2, big.size());
  small = big;
  EXPECT_TRUE(small == big);
  EXPECT_EQ(2, small.size());
  EXPECT_EQ(0, small.erase(999));
  EXPECT_EQ(2, small.size());
}

TEST(MapTest, InsertTest) {
  map<int, int, 2> mp;
  typedef map<int, int, 2>::iterator Iter;
  {
    pair<Iter, bool> rc(mp.insert(make_pair(1, 2)));
    EXPECT_TRUE(rc.second);
    EXPECT_EQ(1, (*rc.first).first);
    EXPECT_EQ(2, (*rc.first).second);
    EXPECT_EQ(1, mp.size());
  }
  {
    pair<Iter, bool> rc(mp.insert(make_pair(1, 2)));
    EXPECT_FALSE(rc.second);
    EXPECT_EQ(1, (*rc.first).first);
    EXPECT_EQ(2, (*rc.first).second);
    EXPECT_EQ(1, mp.size());
  }
  {
    pair<Iter, bool> rc(mp.insert(make_pair(1, 3)));
    EXPECT_FALSE(rc.second);
    EXPECT_EQ(1, (*rc.first).first);
    EXPECT_EQ(3, (*rc.first).second);
    EXPECT_EQ(1, mp.size());
  }
}

TEST(MapTest, IteratorTest) {
  map<int, int, 3> mp;
  mp[1] = 10;
  mp[2] = 20;
  mp[3] = 30;
  int found = 0;
  for (map<int, int, 3>::iterator it = mp.begin(), e = mp.end(); it != e; ++it)
    found |= 1 << (*it).first;
  EXPECT_EQ(14, found);
}

TEST(MapTest, ConstAccessTest) {
  map<int, int, 3> mp;
  mp[1] = 2;
  const map<int, int, 3>& const_mp = mp;
  EXPECT_EQ(2, const_mp[1]);
}

}  // namespace gestures
