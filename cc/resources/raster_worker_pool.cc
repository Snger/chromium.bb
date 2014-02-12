// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/raster_worker_pool.h"

#include <algorithm>

#include "base/debug/trace_event_synthetic_delay.h"
#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/metrics/histogram.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "cc/debug/devtools_instrumentation.h"
#include "cc/debug/traced_value.h"
#include "cc/resources/picture_pile_impl.h"
#include "cc/resources/resource.h"
#include "cc/resources/resource_provider.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "skia/ext/paint_simplifier.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkPixelRef.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "third_party/skia/include/gpu/SkGpuDevice.h"

namespace cc {
namespace {

// Subclass of Allocator that takes a suitably allocated pointer and uses
// it as the pixel memory for the bitmap.
class IdentityAllocator : public SkBitmap::Allocator {
 public:
  explicit IdentityAllocator(void* buffer) : buffer_(buffer) {}
  virtual bool allocPixelRef(SkBitmap* dst, SkColorTable*) OVERRIDE {
    dst->setPixels(buffer_);
    return true;
  }

 private:
  void* buffer_;
};

// Flag to indicate whether we should try and detect that
// a tile is of solid color.
const bool kUseColorEstimator = true;

// Synthetic delay for raster tasks that are required for activation. Global to
// avoid static initializer on critical path.
struct RasterRequiredForActivationSyntheticDelayInitializer {
  RasterRequiredForActivationSyntheticDelayInitializer()
      : delay(base::debug::TraceEventSyntheticDelay::Lookup(
            "cc.RasterRequiredForActivation")) {}
  base::debug::TraceEventSyntheticDelay* delay;
};
static base::LazyInstance<RasterRequiredForActivationSyntheticDelayInitializer>
    g_raster_required_for_activation_delay = LAZY_INSTANCE_INITIALIZER;

class DisableLCDTextFilter : public SkDrawFilter {
 public:
  // SkDrawFilter interface.
  virtual bool filter(SkPaint* paint, SkDrawFilter::Type type) OVERRIDE {
    if (type != SkDrawFilter::kText_Type)
      return true;

    paint->setLCDRenderText(false);
    return true;
  }
};

class RasterWorkerPoolTaskImpl : public internal::RasterWorkerPoolTask {
 public:
  RasterWorkerPoolTaskImpl(const Resource* resource,
                           PicturePileImpl* picture_pile,
                           const gfx::Rect& content_rect,
                           float contents_scale,
                           RasterMode raster_mode,
                           TileResolution tile_resolution,
                           int layer_id,
                           const void* tile_id,
                           int source_frame_number,
                           bool use_gpu_rasterization,
                           RenderingStatsInstrumentation* rendering_stats,
                           const RasterWorkerPool::RasterTask::Reply& reply,
                           internal::Task::Vector* dependencies)
      : internal::RasterWorkerPoolTask(resource,
                                       dependencies,
                                       use_gpu_rasterization),
        picture_pile_(picture_pile),
        content_rect_(content_rect),
        contents_scale_(contents_scale),
        raster_mode_(raster_mode),
        tile_resolution_(tile_resolution),
        layer_id_(layer_id),
        tile_id_(tile_id),
        source_frame_number_(source_frame_number),
        rendering_stats_(rendering_stats),
        reply_(reply),
        buffer_(NULL),
        stride_(0) {}

  void RunAnalysisOnThread(unsigned thread_index) {
    TRACE_EVENT1("cc",
                 "RasterWorkerPoolTaskImpl::RunAnalysisOnThread",
                 "data",
                 TracedValue::FromValue(DataAsValue().release()));

    DCHECK(picture_pile_.get());
    DCHECK(rendering_stats_);

    PicturePileImpl* picture_clone =
        picture_pile_->GetCloneForDrawingOnThread(thread_index);

    DCHECK(picture_clone);

    picture_clone->AnalyzeInRect(
        content_rect_, contents_scale_, &analysis_, rendering_stats_);

    // Record the solid color prediction.
    UMA_HISTOGRAM_BOOLEAN("Renderer4.SolidColorTilesAnalyzed",
                          analysis_.is_solid_color);

    // Clear the flag if we're not using the estimator.
    analysis_.is_solid_color &= kUseColorEstimator;
  }

  void RunRasterOnThread(unsigned thread_index,
                         void* buffer,
                         const gfx::Size& size,
                         int stride) {
    TRACE_EVENT2(
        "cc",
        "RasterWorkerPoolTaskImpl::RunRasterOnThread",
        "data",
        TracedValue::FromValue(DataAsValue().release()),
        "raster_mode",
        TracedValue::FromValue(RasterModeAsValue(raster_mode_).release()));

    devtools_instrumentation::ScopedLayerTask raster_task(
        devtools_instrumentation::kRasterTask, layer_id_);

    DCHECK(picture_pile_.get());
    DCHECK(buffer);

    SkBitmap bitmap;
    switch (resource()->format()) {
      case RGBA_4444:
        // Use the default stride if we will eventually convert this
        // bitmap to 4444.
        bitmap.setConfig(
            SkBitmap::kARGB_8888_Config, size.width(), size.height());
        bitmap.allocPixels();
        break;
      case RGBA_8888:
      case BGRA_8888:
        bitmap.setConfig(
            SkBitmap::kARGB_8888_Config, size.width(), size.height(), stride);
        bitmap.setPixels(buffer);
        break;
      case LUMINANCE_8:
      case RGB_565:
      case ETC1:
        NOTREACHED();
        break;
    }

    SkBitmapDevice device(bitmap);
    SkCanvas canvas(&device);
    Raster(picture_pile_->GetCloneForDrawingOnThread(thread_index), &canvas);
    ChangeBitmapConfigIfNeeded(bitmap, buffer);
  }

  // Overridden from internal::Task:
  virtual void RunOnWorkerThread(unsigned thread_index) OVERRIDE {
    // TODO(alokp): For now run-on-worker-thread implies software rasterization.
    DCHECK(!use_gpu_rasterization());
    RunAnalysisOnThread(thread_index);
    if (buffer_ && !analysis_.is_solid_color)
      RunRasterOnThread(thread_index, buffer_, resource()->size(), stride_);
  }

  // Overridden from internal::WorkerPoolTask:
  virtual void ScheduleOnOriginThread(internal::WorkerPoolTaskClient* client)
      OVERRIDE {
    if (use_gpu_rasterization())
      return;
    DCHECK(!buffer_);
    buffer_ = client->AcquireBufferForRaster(this, &stride_);
  }
  virtual void CompleteOnOriginThread(internal::WorkerPoolTaskClient* client)
      OVERRIDE {
    if (use_gpu_rasterization())
      return;
    buffer_ = NULL;
    client->OnRasterCompleted(this, analysis_);
  }
  virtual void RunReplyOnOriginThread() OVERRIDE {
    DCHECK(!buffer_);
    reply_.Run(analysis_, !HasFinishedRunning());
  }

  // Overridden from internal::RasterWorkerPoolTask:
  virtual void RunOnOriginThread(ResourceProvider* resource_provider,
                                 ContextProvider* context_provider) OVERRIDE {
    // TODO(alokp): Use a trace macro to push/pop markers.
    // Using push/pop functions directly incurs cost to evaluate function
    // arguments even when tracing is disabled.
    context_provider->ContextGL()->PushGroupMarkerEXT(
        0,
        base::StringPrintf(
            "Raster-%d-%d-%p", source_frame_number_, layer_id_, tile_id_)
            .c_str());
    // TODO(alokp): For now run-on-origin-thread implies gpu rasterization.
    DCHECK(use_gpu_rasterization());
    ResourceProvider::ScopedWriteLockGL lock(resource_provider,
                                             resource()->id());
    DCHECK_NE(lock.texture_id(), 0u);

    GrBackendTextureDesc desc;
    desc.fFlags = kRenderTarget_GrBackendTextureFlag;
    desc.fWidth = content_rect_.width();
    desc.fHeight = content_rect_.height();
    desc.fConfig = ToGrFormat(resource()->format());
    desc.fOrigin = kTopLeft_GrSurfaceOrigin;
    desc.fTextureHandle = lock.texture_id();

    GrContext* gr_context = context_provider->GrContext();
    skia::RefPtr<GrTexture> texture =
        skia::AdoptRef(gr_context->wrapBackendTexture(desc));
    skia::RefPtr<SkGpuDevice> device =
        skia::AdoptRef(SkGpuDevice::Create(texture.get()));
    skia::RefPtr<SkCanvas> canvas = skia::AdoptRef(new SkCanvas(device.get()));

    Raster(picture_pile_, canvas.get());
    context_provider->ContextGL()->PopGroupMarkerEXT();
  }

 protected:
  virtual ~RasterWorkerPoolTaskImpl() { DCHECK(!buffer_); }

 private:
  scoped_ptr<base::Value> DataAsValue() const {
    scoped_ptr<base::DictionaryValue> res(new base::DictionaryValue());
    res->Set("tile_id", TracedValue::CreateIDRef(tile_id_).release());
    res->Set("resolution", TileResolutionAsValue(tile_resolution_).release());
    res->SetInteger("source_frame_number", source_frame_number_);
    res->SetInteger("layer_id", layer_id_);
    return res.PassAs<base::Value>();
  }

  static GrPixelConfig ToGrFormat(ResourceFormat format) {
    switch (format) {
      case RGBA_8888:
        return kRGBA_8888_GrPixelConfig;
      case BGRA_8888:
        return kBGRA_8888_GrPixelConfig;
      case RGBA_4444:
        return kRGBA_4444_GrPixelConfig;
      default:
        break;
    }
    DCHECK(false) << "Unsupported resource format.";
    return kSkia8888_GrPixelConfig;
  }

  void Raster(PicturePileImpl* picture_pile, SkCanvas* canvas) {
    skia::RefPtr<SkDrawFilter> draw_filter;
    switch (raster_mode_) {
      case LOW_QUALITY_RASTER_MODE:
        draw_filter = skia::AdoptRef(new skia::PaintSimplifier);
        break;
      case HIGH_QUALITY_NO_LCD_RASTER_MODE:
        draw_filter = skia::AdoptRef(new DisableLCDTextFilter);
        break;
      case HIGH_QUALITY_RASTER_MODE:
        break;
      case NUM_RASTER_MODES:
      default:
        NOTREACHED();
    }
    canvas->setDrawFilter(draw_filter.get());

    base::TimeDelta prev_rasterize_time =
        rendering_stats_->impl_thread_rendering_stats().rasterize_time;

    // Only record rasterization time for highres tiles, because
    // lowres tiles are not required for activation and therefore
    // introduce noise in the measurement (sometimes they get rasterized
    // before we draw and sometimes they aren't)
    RenderingStatsInstrumentation* stats =
        tile_resolution_ == HIGH_RESOLUTION ? rendering_stats_ : NULL;
    picture_pile->RasterToBitmap(canvas, content_rect_, contents_scale_, stats);

    if (rendering_stats_->record_rendering_stats()) {
      base::TimeDelta current_rasterize_time =
          rendering_stats_->impl_thread_rendering_stats().rasterize_time;
      HISTOGRAM_CUSTOM_COUNTS(
          "Renderer4.PictureRasterTimeUS",
          (current_rasterize_time - prev_rasterize_time).InMicroseconds(),
          0,
          100000,
          100);
    }
  }

  void ChangeBitmapConfigIfNeeded(const SkBitmap& bitmap, void* buffer) {
    TRACE_EVENT0("cc", "RasterWorkerPoolTaskImpl::ChangeBitmapConfigIfNeeded");
    SkBitmap::Config config = SkBitmapConfig(resource()->format());
    if (bitmap.getConfig() != config) {
      SkBitmap bitmap_dest;
      IdentityAllocator allocator(buffer);
      bitmap.copyTo(&bitmap_dest, config, &allocator);
      // TODO(kaanb): The GL pipeline assumes a 4-byte alignment for the
      // bitmap data. This check will be removed once crbug.com/293728 is fixed.
      CHECK_EQ(0u, bitmap_dest.rowBytes() % 4);
    }
  }

  PicturePileImpl::Analysis analysis_;
  scoped_refptr<PicturePileImpl> picture_pile_;
  gfx::Rect content_rect_;
  float contents_scale_;
  RasterMode raster_mode_;
  TileResolution tile_resolution_;
  int layer_id_;
  const void* tile_id_;
  int source_frame_number_;
  RenderingStatsInstrumentation* rendering_stats_;
  const RasterWorkerPool::RasterTask::Reply reply_;
  void* buffer_;
  int stride_;

  DISALLOW_COPY_AND_ASSIGN(RasterWorkerPoolTaskImpl);
};

class ImageDecodeWorkerPoolTaskImpl : public internal::WorkerPoolTask {
 public:
  ImageDecodeWorkerPoolTaskImpl(SkPixelRef* pixel_ref,
                                int layer_id,
                                RenderingStatsInstrumentation* rendering_stats,
                                const RasterWorkerPool::Task::Reply& reply)
      : pixel_ref_(skia::SharePtr(pixel_ref)),
        layer_id_(layer_id),
        rendering_stats_(rendering_stats),
        reply_(reply) {}

  // Overridden from internal::Task:
  virtual void RunOnWorkerThread(unsigned thread_index) OVERRIDE {
    TRACE_EVENT0("cc", "ImageDecodeWorkerPoolTaskImpl::RunOnWorkerThread");
    devtools_instrumentation::ScopedImageDecodeTask image_decode_task(
        pixel_ref_.get());
    // This will cause the image referred to by pixel ref to be decoded.
    pixel_ref_->lockPixels();
    pixel_ref_->unlockPixels();
  }

  // Overridden from internal::WorkerPoolTask:
  virtual void ScheduleOnOriginThread(internal::WorkerPoolTaskClient* client)
      OVERRIDE {}
  virtual void CompleteOnOriginThread(internal::WorkerPoolTaskClient* client)
      OVERRIDE {
    client->OnImageDecodeCompleted(this);
  }
  virtual void RunReplyOnOriginThread() OVERRIDE {
    reply_.Run(!HasFinishedRunning());
  }

 protected:
  virtual ~ImageDecodeWorkerPoolTaskImpl() {}

 private:
  skia::RefPtr<SkPixelRef> pixel_ref_;
  int layer_id_;
  RenderingStatsInstrumentation* rendering_stats_;
  const RasterWorkerPool::Task::Reply reply_;

  DISALLOW_COPY_AND_ASSIGN(ImageDecodeWorkerPoolTaskImpl);
};

class RasterFinishedWorkerPoolTaskImpl : public internal::WorkerPoolTask {
 public:
  typedef base::Callback<void(const internal::WorkerPoolTask* source)> Callback;

  explicit RasterFinishedWorkerPoolTaskImpl(
      const Callback& on_raster_finished_callback)
      : origin_loop_(base::MessageLoopProxy::current().get()),
        on_raster_finished_callback_(on_raster_finished_callback) {}

  // Overridden from internal::Task:
  virtual void RunOnWorkerThread(unsigned thread_index) OVERRIDE {
    TRACE_EVENT0("cc", "RasterFinishedWorkerPoolTaskImpl::RunOnWorkerThread");
    origin_loop_->PostTask(
        FROM_HERE,
        base::Bind(&RasterFinishedWorkerPoolTaskImpl::RunOnOriginThread, this));
  }

  // Overridden from internal::WorkerPoolTask:
  virtual void ScheduleOnOriginThread(internal::WorkerPoolTaskClient* client)
      OVERRIDE {}
  virtual void CompleteOnOriginThread(internal::WorkerPoolTaskClient* client)
      OVERRIDE {}
  virtual void RunReplyOnOriginThread() OVERRIDE {}

 protected:
  virtual ~RasterFinishedWorkerPoolTaskImpl() {}

 private:
  void RunOnOriginThread() const { on_raster_finished_callback_.Run(this); }

  scoped_refptr<base::MessageLoopProxy> origin_loop_;
  const Callback on_raster_finished_callback_;

  DISALLOW_COPY_AND_ASSIGN(RasterFinishedWorkerPoolTaskImpl);
};

class RasterRequiredForActivationFinishedWorkerPoolTaskImpl
    : public RasterFinishedWorkerPoolTaskImpl {
 public:
  RasterRequiredForActivationFinishedWorkerPoolTaskImpl(
      const Callback& on_raster_finished_callback,
      size_t tasks_required_for_activation_count)
      : RasterFinishedWorkerPoolTaskImpl(on_raster_finished_callback),
        tasks_required_for_activation_count_(
            tasks_required_for_activation_count) {
    if (tasks_required_for_activation_count_) {
      g_raster_required_for_activation_delay.Get().delay->BeginParallel(
          &activation_delay_end_time_);
    }
  }

  // Overridden from RasterFinishedWorkerPoolTaskImpl:
  virtual void RunOnWorkerThread(unsigned thread_index) OVERRIDE {
    TRACE_EVENT0("cc",
                 "RasterRequiredForActivationFinishedWorkerPoolTaskImpl::"
                 "RunOnWorkerThread");
    if (tasks_required_for_activation_count_) {
      g_raster_required_for_activation_delay.Get().delay->EndParallel(
          activation_delay_end_time_);
    }
    RasterFinishedWorkerPoolTaskImpl::RunOnWorkerThread(thread_index);
  }

 private:
  virtual ~RasterRequiredForActivationFinishedWorkerPoolTaskImpl() {}

  base::TimeTicks activation_delay_end_time_;
  const size_t tasks_required_for_activation_count_;

  DISALLOW_COPY_AND_ASSIGN(
      RasterRequiredForActivationFinishedWorkerPoolTaskImpl);
};

class RasterTaskGraphRunner : public internal::TaskGraphRunner {
 public:
  RasterTaskGraphRunner()
      : internal::TaskGraphRunner(RasterWorkerPool::GetNumRasterThreads(),
                                  "CompositorRaster") {}
};
base::LazyInstance<RasterTaskGraphRunner>::Leaky g_task_graph_runner =
    LAZY_INSTANCE_INITIALIZER;

const int kDefaultNumRasterThreads = 1;

int g_num_raster_threads = 0;

}  // namespace

namespace internal {

WorkerPoolTask::WorkerPoolTask() : did_schedule_(false), did_complete_(false) {}

WorkerPoolTask::~WorkerPoolTask() {
  DCHECK(!did_schedule_);
  DCHECK(!did_run_ || did_complete_);
}

void WorkerPoolTask::WillSchedule() { DCHECK(!did_schedule_); }

void WorkerPoolTask::DidSchedule() {
  did_schedule_ = true;
  did_complete_ = false;
}

bool WorkerPoolTask::HasBeenScheduled() const { return did_schedule_; }

void WorkerPoolTask::WillComplete() { DCHECK(!did_complete_); }

void WorkerPoolTask::DidComplete() {
  DCHECK(did_schedule_);
  DCHECK(!did_complete_);
  did_schedule_ = false;
  did_complete_ = true;
}

bool WorkerPoolTask::HasCompleted() const { return did_complete_; }

RasterWorkerPoolTask::RasterWorkerPoolTask(const Resource* resource,
                                           internal::Task::Vector* dependencies,
                                           bool use_gpu_rasterization)
    : resource_(resource), use_gpu_rasterization_(use_gpu_rasterization) {
  dependencies_.swap(*dependencies);
}

RasterWorkerPoolTask::~RasterWorkerPoolTask() {}

}  // namespace internal

RasterWorkerPool::Task::Set::Set() {}

RasterWorkerPool::Task::Set::~Set() {}

void RasterWorkerPool::Task::Set::Insert(const Task& task) {
  DCHECK(!task.is_null());
  tasks_.push_back(task.internal_);
}

RasterWorkerPool::Task::Task() {}

RasterWorkerPool::Task::Task(internal::WorkerPoolTask* internal)
    : internal_(internal) {}

RasterWorkerPool::Task::~Task() {}

void RasterWorkerPool::Task::Reset() { internal_ = NULL; }

RasterWorkerPool::RasterTask::Queue::QueuedTask::QueuedTask(
    internal::RasterWorkerPoolTask* task,
    bool required_for_activation)
    : task(task), required_for_activation(required_for_activation) {}

RasterWorkerPool::RasterTask::Queue::QueuedTask::~QueuedTask() {}

RasterWorkerPool::RasterTask::Queue::Queue()
    : required_for_activation_count_(0u) {}

RasterWorkerPool::RasterTask::Queue::~Queue() {}

void RasterWorkerPool::RasterTask::Queue::Reset() {
  tasks_.clear();
  required_for_activation_count_ = 0u;
}

void RasterWorkerPool::RasterTask::Queue::Append(const RasterTask& task,
                                                 bool required_for_activation) {
  DCHECK(!task.is_null());
  tasks_.push_back(QueuedTask(task.internal_, required_for_activation));
  required_for_activation_count_ += required_for_activation;
}

void RasterWorkerPool::RasterTask::Queue::Swap(Queue* other) {
  tasks_.swap(other->tasks_);
  std::swap(required_for_activation_count_,
            other->required_for_activation_count_);
}

RasterWorkerPool::RasterTask::RasterTask() {}

RasterWorkerPool::RasterTask::RasterTask(
    internal::RasterWorkerPoolTask* internal)
    : internal_(internal) {}

void RasterWorkerPool::RasterTask::Reset() { internal_ = NULL; }

RasterWorkerPool::RasterTask::~RasterTask() {}

// This allows an external rasterize on-demand system to run raster tasks
// with highest priority using the same task graph runner instance.
unsigned RasterWorkerPool::kOnDemandRasterTaskPriority = 0u;
// Task priorities that make sure raster finished tasks run before any
// remaining raster tasks.
unsigned RasterWorkerPool::kRasterFinishedTaskPriority = 2u;
unsigned RasterWorkerPool::kRasterRequiredForActivationFinishedTaskPriority =
    1u;
unsigned RasterWorkerPool::kRasterTaskPriorityBase = 3u;

RasterWorkerPool::RasterWorkerPool(internal::TaskGraphRunner* task_graph_runner,
                                   ResourceProvider* resource_provider,
                                   ContextProvider* context_provider)
    : task_graph_runner_(task_graph_runner),
      namespace_token_(task_graph_runner_->GetNamespaceToken()),
      client_(NULL),
      resource_provider_(resource_provider),
      context_provider_(context_provider),
      weak_ptr_factory_(this) {}

RasterWorkerPool::~RasterWorkerPool() {}

// static
void RasterWorkerPool::SetNumRasterThreads(int num_threads) {
  DCHECK_LT(0, num_threads);
  DCHECK_EQ(0, g_num_raster_threads);

  g_num_raster_threads = num_threads;
}

// static
int RasterWorkerPool::GetNumRasterThreads() {
  if (!g_num_raster_threads)
    g_num_raster_threads = kDefaultNumRasterThreads;

  return g_num_raster_threads;
}

// static
internal::TaskGraphRunner* RasterWorkerPool::GetTaskGraphRunner() {
  return g_task_graph_runner.Pointer();
}

// static
RasterWorkerPool::RasterTask RasterWorkerPool::CreateRasterTask(
    const Resource* resource,
    PicturePileImpl* picture_pile,
    const gfx::Rect& content_rect,
    float contents_scale,
    RasterMode raster_mode,
    TileResolution tile_resolution,
    int layer_id,
    const void* tile_id,
    int source_frame_number,
    bool use_gpu_rasterization,
    RenderingStatsInstrumentation* rendering_stats,
    const RasterTask::Reply& reply,
    Task::Set* dependencies) {
  return RasterTask(new RasterWorkerPoolTaskImpl(resource,
                                                 picture_pile,
                                                 content_rect,
                                                 contents_scale,
                                                 raster_mode,
                                                 tile_resolution,
                                                 layer_id,
                                                 tile_id,
                                                 source_frame_number,
                                                 use_gpu_rasterization,
                                                 rendering_stats,
                                                 reply,
                                                 &dependencies->tasks_));
}

// static
RasterWorkerPool::Task RasterWorkerPool::CreateImageDecodeTask(
    SkPixelRef* pixel_ref,
    int layer_id,
    RenderingStatsInstrumentation* rendering_stats,
    const Task::Reply& reply) {
  return Task(new ImageDecodeWorkerPoolTaskImpl(
      pixel_ref, layer_id, rendering_stats, reply));
}

void RasterWorkerPool::SetClient(RasterWorkerPoolClient* client) {
  client_ = client;
}

void RasterWorkerPool::Shutdown() {
  TRACE_EVENT0("cc", "RasterWorkerPool::Shutdown");

  internal::TaskGraph empty;
  SetTaskGraph(&empty);
  task_graph_runner_->WaitForTasksToFinishRunning(namespace_token_);
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void RasterWorkerPool::SetTaskGraph(internal::TaskGraph* graph) {
  TRACE_EVENT0("cc", "RasterWorkerPool::SetTaskGraph");

  for (internal::TaskGraph::Node::Vector::iterator it = graph->nodes.begin();
       it != graph->nodes.end();
       ++it) {
    internal::TaskGraph::Node& node = *it;
    internal::WorkerPoolTask* task =
        static_cast<internal::WorkerPoolTask*>(node.task);

    if (!task->HasBeenScheduled()) {
      task->WillSchedule();
      task->ScheduleOnOriginThread(this);
      task->DidSchedule();
    }
  }

  task_graph_runner_->SetTaskGraph(namespace_token_, graph);
}

void RasterWorkerPool::CollectCompletedWorkerPoolTasks(
    internal::Task::Vector* completed_tasks) {
  task_graph_runner_->CollectCompletedTasks(namespace_token_, completed_tasks);
}

void RasterWorkerPool::RunGpuRasterTasks(const RasterTaskVector& tasks) {
  DCHECK(!tasks.empty());
  TRACE_EVENT1(
      "cc", "RasterWorkerPool::RunGpuRasterTasks", "num_tasks", tasks.size());

  GrContext* gr_context = context_provider_->GrContext();
  // TODO(alokp): Implement TestContextProvider::GrContext().
  if (gr_context)
    gr_context->resetContext();

  for (RasterTaskVector::const_iterator it = tasks.begin(); it != tasks.end();
       ++it) {
    internal::RasterWorkerPoolTask* task = it->get();
    DCHECK(task->use_gpu_rasterization());

    task->WillSchedule();
    task->ScheduleOnOriginThread(this);
    task->DidSchedule();

    task->WillRun();
    task->RunOnOriginThread(resource_provider_, context_provider_);
    task->DidRun();

    task->WillComplete();
    task->CompleteOnOriginThread(this);
    task->DidComplete();

    completed_gpu_raster_tasks_.push_back(task);
  }

  // TODO(alokp): Implement TestContextProvider::GrContext().
  if (gr_context)
    gr_context->flush();
}

void RasterWorkerPool::CheckForCompletedGpuRasterTasks() {
  // Complete gpu rasterization tasks.
  while (!completed_gpu_raster_tasks_.empty()) {
    internal::WorkerPoolTask* task = completed_gpu_raster_tasks_.front().get();

    task->RunReplyOnOriginThread();

    completed_gpu_raster_tasks_.pop_front();
  }
}

scoped_refptr<internal::WorkerPoolTask>
RasterWorkerPool::CreateRasterFinishedTask() {
  return make_scoped_refptr(new RasterFinishedWorkerPoolTaskImpl(base::Bind(
      &RasterWorkerPool::OnRasterFinished, weak_ptr_factory_.GetWeakPtr())));
}

scoped_refptr<internal::WorkerPoolTask>
RasterWorkerPool::CreateRasterRequiredForActivationFinishedTask(
    size_t tasks_required_for_activation_count) {
  return make_scoped_refptr(
      new RasterRequiredForActivationFinishedWorkerPoolTaskImpl(
          base::Bind(&RasterWorkerPool::OnRasterRequiredForActivationFinished,
                     weak_ptr_factory_.GetWeakPtr()),
          tasks_required_for_activation_count));
}

void RasterWorkerPool::OnRasterFinished(
    const internal::WorkerPoolTask* source) {
  TRACE_EVENT0("cc", "RasterWorkerPool::OnRasterFinished");

  // Early out if current |raster_finished_task_| is not the source.
  if (source != raster_finished_task_.get())
    return;

  OnRasterTasksFinished();
}

void RasterWorkerPool::OnRasterRequiredForActivationFinished(
    const internal::WorkerPoolTask* source) {
  TRACE_EVENT0("cc", "RasterWorkerPool::OnRasterRequiredForActivationFinished");

  // Early out if current |raster_required_for_activation_finished_task_|
  // is not the source.
  if (source != raster_required_for_activation_finished_task_.get())
    return;

  OnRasterTasksRequiredForActivationFinished();
}

// static
void RasterWorkerPool::InsertNodeForTask(internal::TaskGraph* graph,
                                         internal::WorkerPoolTask* task,
                                         unsigned priority,
                                         size_t dependencies) {
  DCHECK(std::find_if(graph->nodes.begin(),
                      graph->nodes.end(),
                      internal::TaskGraph::Node::TaskComparator(task)) ==
         graph->nodes.end());
  graph->nodes.push_back(
      internal::TaskGraph::Node(task, priority, dependencies));
}

// static
void RasterWorkerPool::InsertNodeForRasterTask(
    internal::TaskGraph* graph,
    internal::WorkerPoolTask* raster_task,
    const internal::Task::Vector& decode_tasks,
    unsigned priority) {
  size_t dependencies = 0u;

  // Insert image decode tasks.
  for (internal::Task::Vector::const_iterator it = decode_tasks.begin();
       it != decode_tasks.end();
       ++it) {
    internal::WorkerPoolTask* decode_task =
        static_cast<internal::WorkerPoolTask*>(it->get());

    // Skip if already decoded.
    if (decode_task->HasCompleted())
      continue;

    dependencies++;

    // Add decode task if it doesn't already exists in graph.
    internal::TaskGraph::Node::Vector::iterator decode_it =
        std::find_if(graph->nodes.begin(),
                     graph->nodes.end(),
                     internal::TaskGraph::Node::TaskComparator(decode_task));
    if (decode_it == graph->nodes.end())
      InsertNodeForTask(graph, decode_task, priority, 0u);

    graph->edges.push_back(internal::TaskGraph::Edge(decode_task, raster_task));
  }

  InsertNodeForTask(graph, raster_task, priority, dependencies);
}

}  // namespace cc
