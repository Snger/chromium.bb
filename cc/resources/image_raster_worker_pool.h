// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_IMAGE_RASTER_WORKER_POOL_H_
#define CC_RESOURCES_IMAGE_RASTER_WORKER_POOL_H_

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "cc/resources/raster_worker_pool.h"
#include "cc/resources/rasterizer.h"

namespace cc {
class ResourceProvider;

class CC_EXPORT ImageRasterWorkerPool : public RasterWorkerPool,
                                        public Rasterizer,
                                        public internal::RasterizerTaskClient {
 public:
  virtual ~ImageRasterWorkerPool();

  static scoped_ptr<RasterWorkerPool> Create(
      base::SequencedTaskRunner* task_runner,
      internal::TaskGraphRunner* task_graph_runner,
      ResourceProvider* resource_provider);

  // Overridden from RasterWorkerPool:
  virtual Rasterizer* AsRasterizer() OVERRIDE;

  // Overridden from Rasterizer:
  virtual void SetClient(RasterizerClient* client) OVERRIDE;
  virtual void Shutdown() OVERRIDE;
  virtual void ScheduleTasks(RasterTaskQueue* queue) OVERRIDE;
  virtual void CheckForCompletedTasks() OVERRIDE;

  // Overridden from internal::RasterizerTaskClient:
  virtual SkCanvas* AcquireCanvasForRaster(internal::RasterTask* task) OVERRIDE;
  virtual void ReleaseCanvasForRaster(internal::RasterTask* task) OVERRIDE;

 protected:
  ImageRasterWorkerPool(base::SequencedTaskRunner* task_runner,
                        internal::TaskGraphRunner* task_graph_runner,
                        ResourceProvider* resource_provider);

 private:
  void OnRasterFinished();
  void OnRasterRequiredForActivationFinished();
  scoped_ptr<base::Value> StateAsValue() const;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  internal::TaskGraphRunner* task_graph_runner_;
  const internal::NamespaceToken namespace_token_;
  RasterizerClient* client_;
  ResourceProvider* resource_provider_;

  bool raster_tasks_pending_;
  bool raster_tasks_required_for_activation_pending_;

  base::WeakPtrFactory<ImageRasterWorkerPool> raster_finished_weak_ptr_factory_;

  scoped_refptr<internal::RasterizerTask> raster_finished_task_;
  scoped_refptr<internal::RasterizerTask>
      raster_required_for_activation_finished_task_;

  // Task graph used when scheduling tasks and vector used to gather
  // completed tasks.
  internal::TaskGraph graph_;
  internal::Task::Vector completed_tasks_;

  DISALLOW_COPY_AND_ASSIGN(ImageRasterWorkerPool);
};

}  // namespace cc

#endif  // CC_RESOURCES_IMAGE_RASTER_WORKER_POOL_H_
