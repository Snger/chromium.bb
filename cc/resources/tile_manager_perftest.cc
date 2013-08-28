// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/time/time.h"
#include "cc/resources/tile.h"
#include "cc/resources/tile_priority.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/fake_output_surface_client.h"
#include "cc/test/fake_picture_pile_impl.h"
#include "cc/test/fake_tile_manager.h"
#include "cc/test/fake_tile_manager_client.h"
#include "cc/test/test_tile_priorities.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace cc {

namespace {

static const int kTimeLimitMillis = 2000;
static const int kWarmupRuns = 5;
static const int kTimeCheckInterval = 10;

class TileManagerPerfTest : public testing::Test {
 public:
  typedef std::vector<std::pair<scoped_refptr<Tile>, ManagedTileBin> >
      TileBinVector;

  TileManagerPerfTest() : num_runs_(0) {}

  // Overridden from testing::Test:
  virtual void SetUp() OVERRIDE {
    output_surface_ = FakeOutputSurface::Create3d();
    CHECK(output_surface_->BindToClient(&output_surface_client_));

    resource_provider_ = ResourceProvider::Create(output_surface_.get(), 0);
    tile_manager_ = make_scoped_ptr(
        new FakeTileManager(&tile_manager_client_, resource_provider_.get()));

    GlobalStateThatImpactsTilePriority state;
    gfx::Size tile_size = settings_.default_tile_size;
    state.memory_limit_in_bytes =
        10000 * 4 * tile_size.width() * tile_size.height();
    state.memory_limit_policy = ALLOW_ANYTHING;
    state.tree_priority = SMOOTHNESS_TAKES_PRIORITY;

    tile_manager_->SetGlobalState(state);
    picture_pile_ = FakePicturePileImpl::CreatePile();
  }

  virtual void TearDown() OVERRIDE {
    tile_manager_.reset(NULL);
    picture_pile_ = NULL;
  }

  void EndTest() {
    elapsed_ = base::TimeTicks::HighResNow() - start_time_;
  }

  void AfterTest(const std::string test_name) {
    // Format matches chrome/test/perf/perf_test.h:PrintResult
    printf("*RESULT %s: %.2f runs/s\n",
           test_name.c_str(),
           num_runs_ / elapsed_.InSecondsF());
  }

  bool DidRun() {
    ++num_runs_;
    if (num_runs_ == kWarmupRuns)
      start_time_ = base::TimeTicks::HighResNow();

    if (!start_time_.is_null() && (num_runs_ % kTimeCheckInterval) == 0) {
      base::TimeDelta elapsed = base::TimeTicks::HighResNow() - start_time_;
      if (elapsed >= base::TimeDelta::FromMilliseconds(kTimeLimitMillis)) {
        elapsed_ = elapsed;
        return false;
      }
    }

    return true;
  }

  TilePriority GetTilePriorityFromBin(ManagedTileBin bin) {
    switch (bin) {
      case NOW_AND_READY_TO_DRAW_BIN:
      case NOW_BIN:
        return TilePriorityForNowBin();
      case SOON_BIN:
        return TilePriorityForSoonBin();
      case EVENTUALLY_AND_ACTIVE_BIN:
      case EVENTUALLY_BIN:
        return TilePriorityForEventualBin();
      case AT_LAST_BIN:
      case AT_LAST_AND_ACTIVE_BIN:
      case NEVER_BIN:
        return TilePriority();
      default:
        NOTREACHED();
        return TilePriority();
    }
  }

  ManagedTileBin GetNextBin(ManagedTileBin bin) {
    switch (bin) {
      case NOW_AND_READY_TO_DRAW_BIN:
      case NOW_BIN:
        return SOON_BIN;
      case SOON_BIN:
        return EVENTUALLY_BIN;
      case EVENTUALLY_AND_ACTIVE_BIN:
      case EVENTUALLY_BIN:
        return NEVER_BIN;
      case AT_LAST_BIN:
      case AT_LAST_AND_ACTIVE_BIN:
      case NEVER_BIN:
        return NOW_BIN;
      default:
        NOTREACHED();
        return NEVER_BIN;
    }
  }

  void CreateBinTiles(int count, ManagedTileBin bin, TileBinVector* tiles) {
    for (int i = 0; i < count; ++i) {
      scoped_refptr<Tile> tile =
          make_scoped_refptr(new Tile(tile_manager_.get(),
                                      picture_pile_.get(),
                                      settings_.default_tile_size,
                                      gfx::Rect(),
                                      gfx::Rect(),
                                      1.0,
                                      0,
                                      0,
                                      true));
      tile->SetPriority(ACTIVE_TREE, GetTilePriorityFromBin(bin));
      tile->SetPriority(PENDING_TREE, GetTilePriorityFromBin(bin));
      tiles->push_back(std::make_pair(tile, bin));
    }
  }

  void CreateTiles(int count, TileBinVector* tiles) {
    // Roughly an equal amount of all bins.
    int count_per_bin = count / NUM_BINS;
    CreateBinTiles(count_per_bin, NOW_BIN, tiles);
    CreateBinTiles(count_per_bin, SOON_BIN, tiles);
    CreateBinTiles(count_per_bin, EVENTUALLY_BIN, tiles);
    CreateBinTiles(count - 3 * count_per_bin, NEVER_BIN, tiles);
  }

  void RunManageTilesTest(const std::string test_name,
                          unsigned tile_count,
                          unsigned priority_change_percent) {
    DCHECK_GE(tile_count, 100u);
    DCHECK_LE(priority_change_percent, 100u);
    num_runs_ = 0;
    TileBinVector tiles;
    CreateTiles(tile_count, &tiles);
    start_time_ = base::TimeTicks();
    do {
      if (priority_change_percent) {
        for (unsigned i = 0;
             i < tile_count;
             i += 100 / priority_change_percent) {
          Tile* tile = tiles[i].first;
          ManagedTileBin bin = GetNextBin(tiles[i].second);
          tile->SetPriority(ACTIVE_TREE, GetTilePriorityFromBin(bin));
          tile->SetPriority(PENDING_TREE, GetTilePriorityFromBin(bin));
          tiles[i].second = bin;
        }
      }

      tile_manager_->ManageTiles();
    } while (DidRun());

    AfterTest(test_name);
  }

 private:
  FakeTileManagerClient tile_manager_client_;
  LayerTreeSettings settings_;
  scoped_ptr<FakeTileManager> tile_manager_;
  scoped_refptr<FakePicturePileImpl> picture_pile_;
  FakeOutputSurfaceClient output_surface_client_;
  scoped_ptr<FakeOutputSurface> output_surface_;
  scoped_ptr<ResourceProvider> resource_provider_;

  base::TimeTicks start_time_;
  base::TimeDelta elapsed_;
  int num_runs_;
};

TEST_F(TileManagerPerfTest, ManageTiles) {
  RunManageTilesTest("manage_tiles_100_0", 100, 0);
  RunManageTilesTest("manage_tiles_1000_0", 1000, 0);
  RunManageTilesTest("manage_tiles_10000_0", 10000, 0);
  RunManageTilesTest("manage_tiles_100_10", 100, 10);
  RunManageTilesTest("manage_tiles_1000_10", 1000, 10);
  RunManageTilesTest("manage_tiles_10000_10", 10000, 10);
  RunManageTilesTest("manage_tiles_100_100", 100, 100);
  RunManageTilesTest("manage_tiles_1000_100", 1000, 100);
  RunManageTilesTest("manage_tiles_10000_100", 10000, 100);
}

}  // namespace

}  // namespace cc
