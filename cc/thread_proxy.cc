// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/thread_proxy.h"

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "cc/context_provider.h"
#include "cc/delay_based_time_source.h"
#include "cc/draw_quad.h"
#include "cc/frame_rate_controller.h"
#include "cc/input_handler.h"
#include "cc/layer_tree_host.h"
#include "cc/layer_tree_impl.h"
#include "cc/output_surface.h"
#include "cc/prioritized_resource_manager.h"
#include "cc/scheduler.h"
#include "cc/thread.h"

namespace {

// Measured in seconds.
const double contextRecreationTickRate = 0.03;

// Measured in seconds.
const double smoothnessTakesPriorityExpirationDelay = 0.25;

}  // namespace

namespace cc {

scoped_ptr<Proxy> ThreadProxy::create(LayerTreeHost* layerTreeHost, scoped_ptr<Thread> implThread)
{
    return make_scoped_ptr(new ThreadProxy(layerTreeHost, implThread.Pass())).PassAs<Proxy>();
}

ThreadProxy::ThreadProxy(LayerTreeHost* layerTreeHost, scoped_ptr<Thread> implThread)
    : Proxy(implThread.Pass())
    , m_animateRequested(false)
    , m_commitRequested(false)
    , m_commitRequestSentToImplThread(false)
    , m_createdOffscreenContextProvider(false)
    , m_layerTreeHost(layerTreeHost)
    , m_rendererInitialized(false)
    , m_started(false)
    , m_texturesAcquired(true)
    , m_inCompositeAndReadback(false)
    , m_manageTilesPending(false)
    , m_weakFactoryOnImplThread(ALLOW_THIS_IN_INITIALIZER_LIST(this))
    , m_weakFactory(ALLOW_THIS_IN_INITIALIZER_LIST(this))
    , m_beginFrameCompletionEventOnImplThread(0)
    , m_readbackRequestOnImplThread(0)
    , m_commitCompletionEventOnImplThread(0)
    , m_completionEventForCommitHeldOnTreeActivation(0)
    , m_textureAcquisitionCompletionEventOnImplThread(0)
    , m_nextFrameIsNewlyCommittedFrameOnImplThread(false)
    , m_renderVSyncEnabled(layerTreeHost->settings().renderVSyncEnabled)
    , m_insideDraw(false)
    , m_totalCommitCount(0)
    , m_deferCommits(false)
    , m_renewTreePriorityOnImplThreadPending(false)
{
    TRACE_EVENT0("cc", "ThreadProxy::ThreadProxy");
    DCHECK(IsMainThread());
}

ThreadProxy::~ThreadProxy()
{
    TRACE_EVENT0("cc", "ThreadProxy::~ThreadProxy");
    DCHECK(IsMainThread());
    DCHECK(!m_started);
}

bool ThreadProxy::CompositeAndReadback(void* pixels, gfx::Rect rect)
{
    TRACE_EVENT0("cc", "ThreadProxy::compositeAndReadback");
    DCHECK(IsMainThread());
    DCHECK(m_layerTreeHost);
    DCHECK(!m_deferCommits);

    if (!m_layerTreeHost->InitializeRendererIfNeeded()) {
        TRACE_EVENT0("cc", "compositeAndReadback_EarlyOut_LR_Uninitialized");
        return false;
    }


    // Perform a synchronous commit.
    {
        DebugScopedSetMainThreadBlocked mainThreadBlocked(this);
        CompletionEvent beginFrameCompletion;
        Proxy::ImplThread()->postTask(base::Bind(&ThreadProxy::forceBeginFrameOnImplThread, m_implThreadWeakPtr, &beginFrameCompletion));
        beginFrameCompletion.wait();
    }
    m_inCompositeAndReadback = true;
    beginFrame(scoped_ptr<BeginFrameAndCommitState>());
    m_inCompositeAndReadback = false;

    // Perform a synchronous readback.
    ReadbackRequest request;
    request.rect = rect;
    request.pixels = pixels;
    {
        DebugScopedSetMainThreadBlocked mainThreadBlocked(this);
        Proxy::ImplThread()->postTask(base::Bind(&ThreadProxy::requestReadbackOnImplThread, m_implThreadWeakPtr, &request));
        request.completion.wait();
    }
    return request.success;
}

void ThreadProxy::requestReadbackOnImplThread(ReadbackRequest* request)
{
    DCHECK(Proxy::IsImplThread());
    DCHECK(!m_readbackRequestOnImplThread);
    if (!m_layerTreeHostImpl.get()) {
        request->success = false;
        request->completion.signal();
        return;
    }

    m_readbackRequestOnImplThread = request;
    m_schedulerOnImplThread->setNeedsRedraw();
    m_schedulerOnImplThread->setNeedsForcedRedraw();
}

void ThreadProxy::StartPageScaleAnimation(gfx::Vector2d targetOffset, bool useAnchor, float scale, base::TimeDelta duration)
{
    DCHECK(Proxy::IsMainThread());
    Proxy::ImplThread()->postTask(base::Bind(&ThreadProxy::requestStartPageScaleAnimationOnImplThread, m_implThreadWeakPtr, targetOffset, useAnchor, scale, duration));
}

void ThreadProxy::requestStartPageScaleAnimationOnImplThread(gfx::Vector2d targetOffset, bool useAnchor, float scale, base::TimeDelta duration)
{
    DCHECK(Proxy::IsImplThread());
    if (m_layerTreeHostImpl.get())
        m_layerTreeHostImpl->StartPageScaleAnimation(targetOffset, useAnchor, scale, base::TimeTicks::Now(), duration);
}

void ThreadProxy::FinishAllRendering()
{
    DCHECK(Proxy::IsMainThread());
    DCHECK(!m_deferCommits);

    // Make sure all GL drawing is finished on the impl thread.
    DebugScopedSetMainThreadBlocked mainThreadBlocked(this);
    CompletionEvent completion;
    Proxy::ImplThread()->postTask(base::Bind(&ThreadProxy::finishAllRenderingOnImplThread, m_implThreadWeakPtr, &completion));
    completion.wait();
}

bool ThreadProxy::IsStarted() const
{
    DCHECK(Proxy::IsMainThread());
    return m_started;
}

bool ThreadProxy::InitializeOutputSurface()
{
    TRACE_EVENT0("cc", "ThreadProxy::initializeOutputSurface");
    scoped_ptr<OutputSurface> context = m_layerTreeHost->CreateOutputSurface();
    if (!context.get())
        return false;

    Proxy::ImplThread()->postTask(base::Bind(&ThreadProxy::initializeOutputSurfaceOnImplThread, m_implThreadWeakPtr, base::Passed(&context)));
    return true;
}

void ThreadProxy::SetSurfaceReady()
{
    TRACE_EVENT0("cc", "ThreadProxy::setSurfaceReady");
    Proxy::ImplThread()->postTask(base::Bind(&ThreadProxy::setSurfaceReadyOnImplThread, m_implThreadWeakPtr));
}

void ThreadProxy::setSurfaceReadyOnImplThread()
{
    TRACE_EVENT0("cc", "ThreadProxy::setSurfaceReadyOnImplThread");
    m_schedulerOnImplThread->setCanBeginFrame(true);
}

void ThreadProxy::SetVisible(bool visible)
{
    TRACE_EVENT0("cc", "ThreadProxy::setVisible");
    DebugScopedSetMainThreadBlocked mainThreadBlocked(this);
    CompletionEvent completion;
    Proxy::ImplThread()->postTask(base::Bind(&ThreadProxy::setVisibleOnImplThread, m_implThreadWeakPtr, &completion, visible));
    completion.wait();
}

void ThreadProxy::setVisibleOnImplThread(CompletionEvent* completion, bool visible)
{
    TRACE_EVENT0("cc", "ThreadProxy::setVisibleOnImplThread");
    m_layerTreeHostImpl->SetVisible(visible);
    m_schedulerOnImplThread->setVisible(visible);
    completion->signal();
}

bool ThreadProxy::InitializeRenderer()
{
    TRACE_EVENT0("cc", "ThreadProxy::initializeRenderer");
    // Make a blocking call to initializeRendererOnImplThread. The results of that call
    // are pushed into the initializeSucceeded and capabilities local variables.
    CompletionEvent completion;
    bool initializeSucceeded = false;
    RendererCapabilities capabilities;
    DebugScopedSetMainThreadBlocked mainThreadBlocked(this);
    Proxy::ImplThread()->postTask(base::Bind(&ThreadProxy::initializeRendererOnImplThread,
                                             m_implThreadWeakPtr,
                                             &completion,
                                             &initializeSucceeded,
                                             &capabilities));
    completion.wait();

    if (initializeSucceeded) {
        m_rendererInitialized = true;
        m_RendererCapabilitiesMainThreadCopy = capabilities;
    }
    return initializeSucceeded;
}

bool ThreadProxy::RecreateOutputSurface()
{
    TRACE_EVENT0("cc", "ThreadProxy::recreateOutputSurface");
    DCHECK(IsMainThread());

    // Try to create the surface.
    scoped_ptr<OutputSurface> outputSurface = m_layerTreeHost->CreateOutputSurface();
    if (!outputSurface.get())
        return false;
    scoped_refptr<cc::ContextProvider> offscreenContextProvider;
    if (m_createdOffscreenContextProvider) {
        offscreenContextProvider = m_layerTreeHost->client()->OffscreenContextProviderForCompositorThread();
        if (!offscreenContextProvider->InitializeOnMainThread())
            return false;
    }

    // Make a blocking call to recreateOutputSurfaceOnImplThread. The results of that
    // call are pushed into the recreateSucceeded and capabilities local
    // variables.
    CompletionEvent completion;
    bool recreateSucceeded = false;
    RendererCapabilities capabilities;
    DebugScopedSetMainThreadBlocked mainThreadBlocked(this);
    Proxy::ImplThread()->postTask(base::Bind(&ThreadProxy::recreateOutputSurfaceOnImplThread,
                                             m_implThreadWeakPtr,
                                             &completion,
                                             base::Passed(&outputSurface),
                                             offscreenContextProvider,
                                             &recreateSucceeded,
                                             &capabilities));
    completion.wait();

    if (recreateSucceeded)
        m_RendererCapabilitiesMainThreadCopy = capabilities;
    return recreateSucceeded;
}

void ThreadProxy::CollectRenderingStats(RenderingStats* stats)
{
    DCHECK(IsMainThread());

    DebugScopedSetMainThreadBlocked mainThreadBlocked(this);
    CompletionEvent completion;
    Proxy::ImplThread()->postTask(base::Bind(&ThreadProxy::renderingStatsOnImplThread,
                                             m_implThreadWeakPtr,  &completion, stats));
    stats->totalCommitTime = m_totalCommitTime;
    stats->totalCommitCount = m_totalCommitCount;

    completion.wait();
}

const RendererCapabilities& ThreadProxy::GetRendererCapabilities() const
{
    DCHECK(m_rendererInitialized);
    return m_RendererCapabilitiesMainThreadCopy;
}

void ThreadProxy::SetNeedsAnimate()
{
    DCHECK(IsMainThread());
    if (m_animateRequested)
        return;

    TRACE_EVENT0("cc", "ThreadProxy::setNeedsAnimate");
    m_animateRequested = true;

    if (m_commitRequestSentToImplThread)
        return;
    m_commitRequestSentToImplThread = true;
    Proxy::ImplThread()->postTask(base::Bind(&ThreadProxy::SetNeedsCommitOnImplThread, m_implThreadWeakPtr));
}

void ThreadProxy::SetNeedsCommit()
{
    DCHECK(IsMainThread());
    if (m_commitRequested)
        return;
    TRACE_EVENT0("cc", "ThreadProxy::setNeedsCommit");
    m_commitRequested = true;

    if (m_commitRequestSentToImplThread)
        return;
    m_commitRequestSentToImplThread = true;
    Proxy::ImplThread()->postTask(base::Bind(&ThreadProxy::SetNeedsCommitOnImplThread, m_implThreadWeakPtr));
}

void ThreadProxy::DidLoseOutputSurfaceOnImplThread()
{
    DCHECK(IsImplThread());
    TRACE_EVENT0("cc", "ThreadProxy::didLoseOutputSurfaceOnImplThread");
    Proxy::ImplThread()->postTask(base::Bind(&ThreadProxy::checkOutputSurfaceStatusOnImplThread, m_implThreadWeakPtr));
}

void ThreadProxy::checkOutputSurfaceStatusOnImplThread()
{
    DCHECK(IsImplThread());
    TRACE_EVENT0("cc", "ThreadProxy::checkOutputSurfaceStatusOnImplThread");
    if (!m_layerTreeHostImpl->IsContextLost())
        return;
    if (cc::ContextProvider* offscreenContexts = m_layerTreeHostImpl->resource_provider()->offscreen_context_provider())
        offscreenContexts->VerifyContexts();
    m_schedulerOnImplThread->didLoseOutputSurface();
}

void ThreadProxy::OnSwapBuffersCompleteOnImplThread()
{
    DCHECK(IsImplThread());
    TRACE_EVENT0("cc", "ThreadProxy::onSwapBuffersCompleteOnImplThread");
    m_schedulerOnImplThread->didSwapBuffersComplete();
    Proxy::MainThread()->postTask(base::Bind(&ThreadProxy::didCompleteSwapBuffers, m_mainThreadWeakPtr));
}

void ThreadProxy::OnVSyncParametersChanged(base::TimeTicks timebase, base::TimeDelta interval)
{
    DCHECK(IsImplThread());
    TRACE_EVENT2("cc", "ThreadProxy::onVSyncParametersChanged", "timebase", (timebase - base::TimeTicks()).InMilliseconds(), "interval", interval.InMilliseconds());
    m_schedulerOnImplThread->setTimebaseAndInterval(timebase, interval);
}

void ThreadProxy::OnCanDrawStateChanged(bool canDraw)
{
    DCHECK(IsImplThread());
    TRACE_EVENT1("cc", "ThreadProxy::onCanDrawStateChanged", "canDraw", canDraw);
    m_schedulerOnImplThread->setCanDraw(canDraw);
}

void ThreadProxy::OnHasPendingTreeStateChanged(bool hasPendingTree)
{
    DCHECK(IsImplThread());
    TRACE_EVENT1("cc", "ThreadProxy::onHasPendingTreeStateChanged", "hasPendingTree", hasPendingTree);
    m_schedulerOnImplThread->setHasPendingTree(hasPendingTree);
}

void ThreadProxy::SetNeedsCommitOnImplThread()
{
    DCHECK(IsImplThread());
    TRACE_EVENT0("cc", "ThreadProxy::setNeedsCommitOnImplThread");
    m_schedulerOnImplThread->setNeedsCommit();
}

void ThreadProxy::SetNeedsManageTilesOnImplThread()
{
    if (m_manageTilesPending)
      return;
    Proxy::ImplThread()->postTask(base::Bind(&ThreadProxy::manageTilesOnImplThread, m_implThreadWeakPtr));
    m_manageTilesPending = true;
}

void ThreadProxy::manageTilesOnImplThread()
{
    // TODO(nduca): If needed, move this into CCSchedulerStateMachine.
    m_manageTilesPending = false;
    if (m_layerTreeHostImpl)
        m_layerTreeHostImpl->ManageTiles();
}

void ThreadProxy::setNeedsForcedCommitOnImplThread()
{
    DCHECK(IsImplThread());
    TRACE_EVENT0("cc", "ThreadProxy::setNeedsForcedCommitOnImplThread");
    m_schedulerOnImplThread->setNeedsForcedCommit();
}

void ThreadProxy::PostAnimationEventsToMainThreadOnImplThread(scoped_ptr<AnimationEventsVector> events, base::Time wallClockTime)
{
    DCHECK(IsImplThread());
    TRACE_EVENT0("cc", "ThreadProxy::postAnimationEventsToMainThreadOnImplThread");
    Proxy::MainThread()->postTask(base::Bind(&ThreadProxy::setAnimationEvents, m_mainThreadWeakPtr, base::Passed(&events), wallClockTime));
}

bool ThreadProxy::ReduceContentsTextureMemoryOnImplThread(size_t limitBytes, int priorityCutoff)
{
    DCHECK(IsImplThread());

    if (!m_layerTreeHost->contents_texture_manager())
        return false;

    bool reduceResult = m_layerTreeHost->contents_texture_manager()->reduceMemoryOnImplThread(limitBytes, priorityCutoff, m_layerTreeHostImpl->resource_provider());
    if (!reduceResult)
        return false;

    // The texture upload queue may reference textures that were just purged, clear
    // them from the queue.
    if (m_currentResourceUpdateControllerOnImplThread.get())
        m_currentResourceUpdateControllerOnImplThread->DiscardUploadsToEvictedResources();
    return true;
}

void ThreadProxy::ReduceWastedContentsTextureMemoryOnImplThread()
{
    DCHECK(IsImplThread());

    if (!m_layerTreeHost->contents_texture_manager())
        return;

    m_layerTreeHost->contents_texture_manager()->reduceWastedMemoryOnImplThread(m_layerTreeHostImpl->resource_provider());
}

void ThreadProxy::SendManagedMemoryStats()
{
    DCHECK(IsImplThread());
    if (!m_layerTreeHostImpl.get())
        return;
    if (!m_layerTreeHost->contents_texture_manager())
        return;

    // If we are using impl-side painting, then sendManagedMemoryStats is called
    // directly after the tile manager's manage function, and doesn't need to
    // interact with main thread's layer tree.
    if (m_layerTreeHost->settings().implSidePainting)
        return;

    m_layerTreeHostImpl->SendManagedMemoryStats(
        m_layerTreeHost->contents_texture_manager()->memoryVisibleBytes(),
        m_layerTreeHost->contents_texture_manager()->memoryVisibleAndNearbyBytes(),
        m_layerTreeHost->contents_texture_manager()->memoryUseBytes());
}

bool ThreadProxy::IsInsideDraw()
{
    return m_insideDraw;
}

void ThreadProxy::SetNeedsRedraw()
{
    DCHECK(IsMainThread());
    TRACE_EVENT0("cc", "ThreadProxy::setNeedsRedraw");
    Proxy::ImplThread()->postTask(base::Bind(&ThreadProxy::setFullRootLayerDamageOnImplThread, m_implThreadWeakPtr));
    Proxy::ImplThread()->postTask(base::Bind(&ThreadProxy::SetNeedsRedrawOnImplThread, m_implThreadWeakPtr));
}

void ThreadProxy::SetDeferCommits(bool deferCommits)
{
    DCHECK(IsMainThread());
    DCHECK_NE(m_deferCommits, deferCommits);
    m_deferCommits = deferCommits;

    if (m_deferCommits)
        TRACE_EVENT_ASYNC_BEGIN0("cc", "ThreadProxy::setDeferCommits", this);
    else
        TRACE_EVENT_ASYNC_END0("cc", "ThreadProxy::setDeferCommits", this);

    if (!m_deferCommits && m_pendingDeferredCommit)
        Proxy::MainThread()->postTask(base::Bind(&ThreadProxy::beginFrame, m_mainThreadWeakPtr, base::Passed(&m_pendingDeferredCommit)));
}

bool ThreadProxy::CommitRequested() const
{
    DCHECK(IsMainThread());
    return m_commitRequested;
}

void ThreadProxy::SetNeedsRedrawOnImplThread()
{
    DCHECK(IsImplThread());
    TRACE_EVENT0("cc", "ThreadProxy::setNeedsRedrawOnImplThread");
    m_schedulerOnImplThread->setNeedsRedraw();
}

void ThreadProxy::didSwapUseIncompleteTileOnImplThread()
{
   DCHECK(IsImplThread());
   TRACE_EVENT0("cc", "ThreadProxy::didSwapUseIncompleteTileOnImplThread");
   m_schedulerOnImplThread->didSwapUseIncompleteTile();
}

void ThreadProxy::DidUploadVisibleHighResolutionTileOnImplThread()
{
   DCHECK(IsImplThread());
   TRACE_EVENT0("cc", "ThreadProxy::didUploadVisibleHighResolutionTileOnImplThread");
   m_schedulerOnImplThread->setNeedsRedraw();
}

void ThreadProxy::MainThreadHasStoppedFlinging()
{
    if (m_inputHandlerOnImplThread)
        m_inputHandlerOnImplThread->MainThreadHasStoppedFlinging();
}

void ThreadProxy::Start()
{
    DCHECK(IsMainThread());
    DCHECK(Proxy::ImplThread());
    // Create LayerTreeHostImpl.
    DebugScopedSetMainThreadBlocked mainThreadBlocked(this);
    CompletionEvent completion;
    scoped_ptr<InputHandler> handler = m_layerTreeHost->CreateInputHandler();
    Proxy::ImplThread()->postTask(base::Bind(&ThreadProxy::initializeImplOnImplThread, base::Unretained(this), &completion, handler.release()));
    completion.wait();

    m_mainThreadWeakPtr = m_weakFactory.GetWeakPtr();

    m_started = true;
}

void ThreadProxy::Stop()
{
    TRACE_EVENT0("cc", "ThreadProxy::stop");
    DCHECK(IsMainThread());
    DCHECK(m_started);

    // Synchronously deletes the impl.
    {
        DebugScopedSetMainThreadBlocked mainThreadBlocked(this);

        CompletionEvent completion;
        Proxy::ImplThread()->postTask(base::Bind(&ThreadProxy::layerTreeHostClosedOnImplThread, m_implThreadWeakPtr, &completion));
        completion.wait();
    }

    m_weakFactory.InvalidateWeakPtrs();

    DCHECK(!m_layerTreeHostImpl.get()); // verify that the impl deleted.
    m_layerTreeHost = 0;
    m_started = false;
}

void ThreadProxy::ForceSerializeOnSwapBuffers()
{
    DebugScopedSetMainThreadBlocked mainThreadBlocked(this);
    CompletionEvent completion;
    Proxy::ImplThread()->postTask(base::Bind(&ThreadProxy::forceSerializeOnSwapBuffersOnImplThread, m_implThreadWeakPtr, &completion));
    completion.wait();
}

void ThreadProxy::forceSerializeOnSwapBuffersOnImplThread(CompletionEvent* completion)
{
    if (m_rendererInitialized)
        m_layerTreeHostImpl->renderer()->DoNoOp();
    completion->signal();
}


void ThreadProxy::finishAllRenderingOnImplThread(CompletionEvent* completion)
{
    TRACE_EVENT0("cc", "ThreadProxy::finishAllRenderingOnImplThread");
    DCHECK(IsImplThread());
    m_layerTreeHostImpl->FinishAllRendering();
    completion->signal();
}

void ThreadProxy::forceBeginFrameOnImplThread(CompletionEvent* completion)
{
    TRACE_EVENT0("cc", "ThreadProxy::forceBeginFrameOnImplThread");
    DCHECK(!m_beginFrameCompletionEventOnImplThread);

    setNeedsForcedCommitOnImplThread();
    if (m_schedulerOnImplThread->commitPending()) {
        completion->signal();
        return;
    }

    m_beginFrameCompletionEventOnImplThread = completion;
}

void ThreadProxy::scheduledActionBeginFrame()
{
    TRACE_EVENT0("cc", "ThreadProxy::scheduledActionBeginFrame");
    scoped_ptr<BeginFrameAndCommitState> beginFrameState(new BeginFrameAndCommitState);
    beginFrameState->monotonicFrameBeginTime = base::TimeTicks::Now();
    beginFrameState->scrollInfo = m_layerTreeHostImpl->ProcessScrollDeltas();
    beginFrameState->implTransform = m_layerTreeHostImpl->active_tree()->ImplTransform();
    DCHECK_GT(m_layerTreeHostImpl->memory_allocation_limit_bytes(), 0u);
    beginFrameState->memoryAllocationLimitBytes = m_layerTreeHostImpl->memory_allocation_limit_bytes();
    Proxy::MainThread()->postTask(base::Bind(&ThreadProxy::beginFrame, m_mainThreadWeakPtr, base::Passed(&beginFrameState)));

    if (m_beginFrameCompletionEventOnImplThread) {
        m_beginFrameCompletionEventOnImplThread->signal();
        m_beginFrameCompletionEventOnImplThread = 0;
    }
}

void ThreadProxy::beginFrame(scoped_ptr<BeginFrameAndCommitState> beginFrameState)
{
    TRACE_EVENT0("cc", "ThreadProxy::beginFrame");
    DCHECK(IsMainThread());
    if (!m_layerTreeHost)
        return;

    if (m_deferCommits) {
        m_pendingDeferredCommit = beginFrameState.Pass();
        m_layerTreeHost->DidDeferCommit();
        TRACE_EVENT0("cc", "EarlyOut_DeferCommits");
        return;
    }

    // Do not notify the impl thread of commit requests that occur during
    // the apply/animate/layout part of the beginFrameAndCommit process since
    // those commit requests will get painted immediately. Once we have done
    // the paint, m_commitRequested will be set to false to allow new commit
    // requests to be scheduled.
    m_commitRequested = true;
    m_commitRequestSentToImplThread = true;

    // On the other hand, the animationRequested flag needs to be cleared
    // here so that any animation requests generated by the apply or animate
    // callbacks will trigger another frame.
    m_animateRequested = false;

    if (beginFrameState) {
        m_layerTreeHost->ApplyScrollAndScale(*beginFrameState->scrollInfo);
        m_layerTreeHost->SetImplTransform(beginFrameState->implTransform);
    }

    if (!m_inCompositeAndReadback && !m_layerTreeHost->visible()) {
        m_commitRequested = false;
        m_commitRequestSentToImplThread = false;

        TRACE_EVENT0("cc", "EarlyOut_NotVisible");
        Proxy::ImplThread()->postTask(base::Bind(&ThreadProxy::beginFrameAbortedOnImplThread, m_implThreadWeakPtr));
        return;
    }

    m_layerTreeHost->WillBeginFrame();

    if (beginFrameState)
        m_layerTreeHost->UpdateAnimations(beginFrameState->monotonicFrameBeginTime);

    // Unlink any backings that the impl thread has evicted, so that we know to re-paint
    // them in updateLayers.
    if (m_layerTreeHost->contents_texture_manager())
        m_layerTreeHost->contents_texture_manager()->unlinkAndClearEvictedBackings();

    m_layerTreeHost->Layout();

    // Clear the commit flag after updating animations and layout here --- objects that only
    // layout when painted will trigger another setNeedsCommit inside
    // updateLayers.
    m_commitRequested = false;
    m_commitRequestSentToImplThread = false;

    if (!m_layerTreeHost->InitializeRendererIfNeeded()) {
        TRACE_EVENT0("cc", "EarlyOut_InitializeFailed");
        return;
    }

    scoped_ptr<ResourceUpdateQueue> queue = make_scoped_ptr(new ResourceUpdateQueue);
    m_layerTreeHost->UpdateLayers(queue.get(), beginFrameState ? beginFrameState->memoryAllocationLimitBytes : 0);

    // Once single buffered layers are committed, they cannot be modified until
    // they are drawn by the impl thread.
    m_texturesAcquired = false;

    m_layerTreeHost->WillCommit();
    // Before applying scrolls and calling animate, we set m_animateRequested to
    // false. If it is true now, it means setNeedAnimate was called again, but
    // during a state when m_commitRequestSentToImplThread = true. We need to
    // force that call to happen again now so that the commit request is sent to
    // the impl thread.
    if (m_animateRequested) {
        // Forces setNeedsAnimate to consider posting a commit task.
        m_animateRequested = false;
        SetNeedsAnimate();
    }

    scoped_refptr<cc::ContextProvider> offscreenContextProvider;
    if (m_RendererCapabilitiesMainThreadCopy.using_offscreen_context3d && m_layerTreeHost->needs_offscreen_context()) {
        offscreenContextProvider = m_layerTreeHost->client()->OffscreenContextProviderForCompositorThread();
        if (offscreenContextProvider->InitializeOnMainThread())
            m_createdOffscreenContextProvider = true;
        else
            offscreenContextProvider = NULL;
    }

    // Notify the impl thread that the beginFrame has completed. This will
    // begin the commit process, which is blocking from the main thread's
    // point of view, but asynchronously performed on the impl thread,
    // coordinated by the Scheduler.
    {
        TRACE_EVENT0("cc", "commit");

        DebugScopedSetMainThreadBlocked mainThreadBlocked(this);

        base::TimeTicks startTime = base::TimeTicks::HighResNow();
        CompletionEvent completion;
        Proxy::ImplThread()->postTask(base::Bind(&ThreadProxy::beginFrameCompleteOnImplThread, m_implThreadWeakPtr, &completion, queue.release(), offscreenContextProvider));
        completion.wait();
        base::TimeTicks endTime = base::TimeTicks::HighResNow();

        m_totalCommitTime += endTime - startTime;
        m_totalCommitCount++;
    }

    m_layerTreeHost->CommitComplete();
    m_layerTreeHost->DidBeginFrame();
}

void ThreadProxy::beginFrameCompleteOnImplThread(CompletionEvent* completion, ResourceUpdateQueue* rawQueue, scoped_refptr<cc::ContextProvider> offscreenContextProvider)
{
    scoped_ptr<ResourceUpdateQueue> queue(rawQueue);

    TRACE_EVENT0("cc", "ThreadProxy::beginFrameCompleteOnImplThread");
    DCHECK(!m_commitCompletionEventOnImplThread);
    DCHECK(IsImplThread() && IsMainThreadBlocked());
    DCHECK(m_schedulerOnImplThread);
    DCHECK(m_schedulerOnImplThread->commitPending());

    if (!m_layerTreeHostImpl.get()) {
        TRACE_EVENT0("cc", "EarlyOut_NoLayerTree");
        completion->signal();
        return;
    }

    m_layerTreeHostImpl->resource_provider()->SetOffscreenContextProvider(offscreenContextProvider);

    if (m_layerTreeHost->contents_texture_manager()->linkedEvictedBackingsExist()) {
        // Clear any uploads we were making to textures linked to evicted
        // resources
        queue->clearUploadsToEvictedResources();
        // Some textures in the layer tree are invalid. Kick off another commit
        // to fill them again.
        SetNeedsCommitOnImplThread();
    }

    m_layerTreeHost->contents_texture_manager()->pushTexturePrioritiesToBackings();

    m_currentResourceUpdateControllerOnImplThread = ResourceUpdateController::Create(this, Proxy::ImplThread(), queue.Pass(), m_layerTreeHostImpl->resource_provider());
    m_currentResourceUpdateControllerOnImplThread->PerformMoreUpdates(
        m_schedulerOnImplThread->anticipatedDrawTime());

    m_commitCompletionEventOnImplThread = completion;
}

void ThreadProxy::beginFrameAbortedOnImplThread()
{
    TRACE_EVENT0("cc", "ThreadProxy::beginFrameAbortedOnImplThread");
    DCHECK(IsImplThread());
    DCHECK(m_schedulerOnImplThread);
    DCHECK(m_schedulerOnImplThread->commitPending());

    m_schedulerOnImplThread->beginFrameAborted();
}

void ThreadProxy::scheduledActionCommit()
{
    TRACE_EVENT0("cc", "ThreadProxy::scheduledActionCommit");
    DCHECK(IsImplThread());
    DCHECK(m_commitCompletionEventOnImplThread);
    DCHECK(m_currentResourceUpdateControllerOnImplThread);

    // Complete all remaining texture updates.
    m_currentResourceUpdateControllerOnImplThread->Finalize();
    m_currentResourceUpdateControllerOnImplThread.reset();

    m_layerTreeHostImpl->BeginCommit();
    m_layerTreeHost->BeginCommitOnImplThread(m_layerTreeHostImpl.get());
    m_layerTreeHost->FinishCommitOnImplThread(m_layerTreeHostImpl.get());
    m_layerTreeHostImpl->CommitComplete();

    m_nextFrameIsNewlyCommittedFrameOnImplThread = true;

    if (m_layerTreeHost->settings().implSidePainting && m_layerTreeHost->BlocksPendingCommit())
    {
        // For some layer types in impl-side painting, the commit is held until
        // the pending tree is activated.
        TRACE_EVENT_INSTANT0("cc", "HoldCommit");
        m_completionEventForCommitHeldOnTreeActivation = m_commitCompletionEventOnImplThread;
        m_commitCompletionEventOnImplThread = 0;
    }
    else
    {
        m_commitCompletionEventOnImplThread->signal();
        m_commitCompletionEventOnImplThread = 0;
    }

    // SetVisible kicks off the next scheduler action, so this must be last.
    m_schedulerOnImplThread->setVisible(m_layerTreeHostImpl->visible());
}

void ThreadProxy::scheduledActionCheckForCompletedTileUploads()
{
    DCHECK(IsImplThread());
    TRACE_EVENT0("cc", "ThreadProxy::scheduledActionCheckForCompletedTileUploads");
    m_layerTreeHostImpl->CheckForCompletedTileUploads();
}

void ThreadProxy::scheduledActionActivatePendingTreeIfNeeded()
{
    DCHECK(IsImplThread());
    TRACE_EVENT0("cc", "ThreadProxy::scheduledActionActivatePendingTreeIfNeeded");
    m_layerTreeHostImpl->ActivatePendingTreeIfNeeded();
}

void ThreadProxy::scheduledActionBeginContextRecreation()
{
    DCHECK(IsImplThread());
    Proxy::MainThread()->postTask(base::Bind(&ThreadProxy::beginContextRecreation, m_mainThreadWeakPtr));
}

ScheduledActionDrawAndSwapResult ThreadProxy::scheduledActionDrawAndSwapInternal(bool forcedDraw)
{
    TRACE_EVENT0("cc", "ThreadProxy::scheduledActionDrawAndSwap");

    base::AutoReset<bool> markInside(&m_insideDraw, true);

    ScheduledActionDrawAndSwapResult result;
    result.didDraw = false;
    result.didSwap = false;
    DCHECK(IsImplThread());
    DCHECK(m_layerTreeHostImpl.get());
    if (!m_layerTreeHostImpl.get())
        return result;

    DCHECK(m_layerTreeHostImpl->renderer());
    if (!m_layerTreeHostImpl->renderer())
        return result;

    // FIXME: compute the frame display time more intelligently
    base::TimeTicks monotonicTime = base::TimeTicks::Now();
    base::Time wallClockTime = base::Time::Now();

    if (m_inputHandlerOnImplThread.get())
        m_inputHandlerOnImplThread->Animate(monotonicTime);

    m_layerTreeHostImpl->ActivatePendingTreeIfNeeded();
    m_layerTreeHostImpl->Animate(monotonicTime, wallClockTime);

    // This method is called on a forced draw, regardless of whether we are able to produce a frame,
    // as the calling site on main thread is blocked until its request completes, and we signal
    // completion here. If canDraw() is false, we will indicate success=false to the caller, but we
    // must still signal completion to avoid deadlock.

    // We guard prepareToDraw() with canDraw() because it always returns a valid frame, so can only
    // be used when such a frame is possible. Since drawLayers() depends on the result of
    // prepareToDraw(), it is guarded on canDraw() as well.

    LayerTreeHostImpl::FrameData frame;
    bool drawFrame = m_layerTreeHostImpl->CanDraw() && (m_layerTreeHostImpl->PrepareToDraw(&frame) || forcedDraw);
    if (drawFrame) {
        m_layerTreeHostImpl->DrawLayers(&frame);
        result.didDraw = true;
    }
    m_layerTreeHostImpl->DidDrawAllLayers(frame);

    // Check for tree activation.
    if (m_completionEventForCommitHeldOnTreeActivation && !m_layerTreeHostImpl->pending_tree())
    {
        TRACE_EVENT_INSTANT0("cc", "ReleaseCommitbyActivation");
        DCHECK(m_layerTreeHostImpl->settings().implSidePainting);
        m_completionEventForCommitHeldOnTreeActivation->signal();
        m_completionEventForCommitHeldOnTreeActivation = 0;
    }

    // Check for a pending compositeAndReadback.
    if (m_readbackRequestOnImplThread) {
        m_readbackRequestOnImplThread->success = false;
        if (drawFrame) {
            m_layerTreeHostImpl->Readback(m_readbackRequestOnImplThread->pixels, m_readbackRequestOnImplThread->rect);
            m_readbackRequestOnImplThread->success = !m_layerTreeHostImpl->IsContextLost();
        }
        m_readbackRequestOnImplThread->completion.signal();
        m_readbackRequestOnImplThread = 0;
    } else if (drawFrame) {
        result.didSwap = m_layerTreeHostImpl->SwapBuffers();

        if (frame.contains_incomplete_tile)
          didSwapUseIncompleteTileOnImplThread();
    }

    // Tell the main thread that the the newly-commited frame was drawn.
    if (m_nextFrameIsNewlyCommittedFrameOnImplThread) {
        m_nextFrameIsNewlyCommittedFrameOnImplThread = false;
        Proxy::MainThread()->postTask(base::Bind(&ThreadProxy::didCommitAndDrawFrame, m_mainThreadWeakPtr));
    }

    if (drawFrame)
        checkOutputSurfaceStatusOnImplThread();

    m_layerTreeHostImpl->BeginNextFrame();

    return result;
}

void ThreadProxy::AcquireLayerTextures()
{
    // Called when the main thread needs to modify a layer texture that is used
    // directly by the compositor.
    // This method will block until the next compositor draw if there is a
    // previously committed frame that is still undrawn. This is necessary to
    // ensure that the main thread does not monopolize access to the textures.
    DCHECK(IsMainThread());

    if (m_texturesAcquired)
        return;

    TRACE_EVENT0("cc", "ThreadProxy::acquireLayerTextures");
    DebugScopedSetMainThreadBlocked mainThreadBlocked(this);
    CompletionEvent completion;
    Proxy::ImplThread()->postTask(base::Bind(&ThreadProxy::acquireLayerTexturesForMainThreadOnImplThread, m_implThreadWeakPtr, &completion));
    completion.wait(); // Block until it is safe to write to layer textures from the main thread.

    m_texturesAcquired = true;
}

void ThreadProxy::acquireLayerTexturesForMainThreadOnImplThread(CompletionEvent* completion)
{
    DCHECK(IsImplThread());
    DCHECK(!m_textureAcquisitionCompletionEventOnImplThread);

    m_textureAcquisitionCompletionEventOnImplThread = completion;
    m_schedulerOnImplThread->setMainThreadNeedsLayerTextures();
}

void ThreadProxy::scheduledActionAcquireLayerTexturesForMainThread()
{
    DCHECK(m_textureAcquisitionCompletionEventOnImplThread);
    m_textureAcquisitionCompletionEventOnImplThread->signal();
    m_textureAcquisitionCompletionEventOnImplThread = 0;
}

ScheduledActionDrawAndSwapResult ThreadProxy::scheduledActionDrawAndSwapIfPossible()
{
    return scheduledActionDrawAndSwapInternal(false);
}

ScheduledActionDrawAndSwapResult ThreadProxy::scheduledActionDrawAndSwapForced()
{
    return scheduledActionDrawAndSwapInternal(true);
}

void ThreadProxy::didAnticipatedDrawTimeChange(base::TimeTicks time)
{
    if (!m_currentResourceUpdateControllerOnImplThread)
        return;

    m_currentResourceUpdateControllerOnImplThread->PerformMoreUpdates(time);
}

void ThreadProxy::ReadyToFinalizeTextureUpdates()
{
    DCHECK(IsImplThread());
    m_schedulerOnImplThread->beginFrameComplete();
}

void ThreadProxy::didCommitAndDrawFrame()
{
    DCHECK(IsMainThread());
    if (!m_layerTreeHost)
        return;
    m_layerTreeHost->DidCommitAndDrawFrame();
}

void ThreadProxy::didCompleteSwapBuffers()
{
    DCHECK(IsMainThread());
    if (!m_layerTreeHost)
        return;
    m_layerTreeHost->DidCompleteSwapBuffers();
}

void ThreadProxy::setAnimationEvents(scoped_ptr<AnimationEventsVector> events, base::Time wallClockTime)
{
    TRACE_EVENT0("cc", "ThreadProxy::setAnimationEvents");
    DCHECK(IsMainThread());
    if (!m_layerTreeHost)
        return;
    m_layerTreeHost->SetAnimationEvents(events.Pass(), wallClockTime);
}

void ThreadProxy::beginContextRecreation()
{
    TRACE_EVENT0("cc", "ThreadProxy::beginContextRecreation");
    DCHECK(IsMainThread());
    m_layerTreeHost->DidLoseOutputSurface();
    m_outputSurfaceRecreationCallback.Reset(base::Bind(&ThreadProxy::tryToRecreateOutputSurface, base::Unretained(this)));
    Proxy::MainThread()->postTask(m_outputSurfaceRecreationCallback.callback());
}

void ThreadProxy::tryToRecreateOutputSurface()
{
    DCHECK(IsMainThread());
    DCHECK(m_layerTreeHost);
    LayerTreeHost::RecreateResult result = m_layerTreeHost->RecreateOutputSurface();
    if (result == LayerTreeHost::RecreateFailedButTryAgain)
        Proxy::MainThread()->postTask(m_outputSurfaceRecreationCallback.callback());
    else if (result == LayerTreeHost::RecreateSucceeded)
        m_outputSurfaceRecreationCallback.Cancel();
}

void ThreadProxy::initializeImplOnImplThread(CompletionEvent* completion, InputHandler* handler)
{
    TRACE_EVENT0("cc", "ThreadProxy::initializeImplOnImplThread");
    DCHECK(IsImplThread());
    m_layerTreeHostImpl = m_layerTreeHost->CreateLayerTreeHostImpl(this);
    const base::TimeDelta displayRefreshInterval = base::TimeDelta::FromMicroseconds(base::Time::kMicrosecondsPerSecond / 60);
    scoped_ptr<FrameRateController> frameRateController;
    if (m_renderVSyncEnabled)
        frameRateController.reset(new FrameRateController(DelayBasedTimeSource::create(displayRefreshInterval, Proxy::ImplThread())));
    else
        frameRateController.reset(new FrameRateController(Proxy::ImplThread()));
    SchedulerSettings schedulerSettings;
    schedulerSettings.implSidePainting = m_layerTreeHost->settings().implSidePainting;
    m_schedulerOnImplThread = Scheduler::create(this, frameRateController.Pass(),
                                                schedulerSettings);
    m_schedulerOnImplThread->setVisible(m_layerTreeHostImpl->visible());

    m_inputHandlerOnImplThread = scoped_ptr<InputHandler>(handler);
    if (m_inputHandlerOnImplThread.get())
        m_inputHandlerOnImplThread->BindToClient(m_layerTreeHostImpl.get());

    m_implThreadWeakPtr = m_weakFactoryOnImplThread.GetWeakPtr();
    completion->signal();
}

void ThreadProxy::initializeOutputSurfaceOnImplThread(scoped_ptr<OutputSurface> outputSurface)
{
    TRACE_EVENT0("cc", "ThreadProxy::initializeContextOnImplThread");
    DCHECK(IsImplThread());
    m_outputSurfaceBeforeInitializationOnImplThread = outputSurface.Pass();
}

void ThreadProxy::initializeRendererOnImplThread(CompletionEvent* completion, bool* initializeSucceeded, RendererCapabilities* capabilities)
{
    TRACE_EVENT0("cc", "ThreadProxy::initializeRendererOnImplThread");
    DCHECK(IsImplThread());
    DCHECK(m_outputSurfaceBeforeInitializationOnImplThread.get());
    *initializeSucceeded = m_layerTreeHostImpl->InitializeRenderer(m_outputSurfaceBeforeInitializationOnImplThread.Pass());
    if (*initializeSucceeded) {
        *capabilities = m_layerTreeHostImpl->GetRendererCapabilities();
        m_schedulerOnImplThread->setSwapBuffersCompleteSupported(
                capabilities->using_swap_complete_callback);

        int maxFramesPending = FrameRateController::kDefaultMaxFramesPending;
        if (m_layerTreeHostImpl->output_surface()->capabilities().has_parent_compositor)
            maxFramesPending = 1;
        m_schedulerOnImplThread->setMaxFramesPending(maxFramesPending);
    }

    completion->signal();
}

void ThreadProxy::layerTreeHostClosedOnImplThread(CompletionEvent* completion)
{
    TRACE_EVENT0("cc", "ThreadProxy::layerTreeHostClosedOnImplThread");
    DCHECK(IsImplThread());
    m_layerTreeHost->DeleteContentsTexturesOnImplThread(m_layerTreeHostImpl->resource_provider());
    m_inputHandlerOnImplThread.reset();
    m_layerTreeHostImpl.reset();
    m_schedulerOnImplThread.reset();
    m_weakFactoryOnImplThread.InvalidateWeakPtrs();
    completion->signal();
}

void ThreadProxy::setFullRootLayerDamageOnImplThread()
{
    DCHECK(IsImplThread());
    m_layerTreeHostImpl->SetFullRootLayerDamage();
}

size_t ThreadProxy::MaxPartialTextureUpdates() const
{
    return ResourceUpdateController::MaxPartialTextureUpdates();
}

void ThreadProxy::recreateOutputSurfaceOnImplThread(CompletionEvent* completion, scoped_ptr<OutputSurface> outputSurface, scoped_refptr<cc::ContextProvider> offscreenContextProvider, bool* recreateSucceeded, RendererCapabilities* capabilities)
{
    TRACE_EVENT0("cc", "ThreadProxy::recreateOutputSurfaceOnImplThread");
    DCHECK(IsImplThread());
    m_layerTreeHost->DeleteContentsTexturesOnImplThread(m_layerTreeHostImpl->resource_provider());
    *recreateSucceeded = m_layerTreeHostImpl->InitializeRenderer(outputSurface.Pass());
    if (*recreateSucceeded) {
        *capabilities = m_layerTreeHostImpl->GetRendererCapabilities();
        m_layerTreeHostImpl->resource_provider()->SetOffscreenContextProvider(offscreenContextProvider);
        m_schedulerOnImplThread->didRecreateOutputSurface();
    } else if (offscreenContextProvider) {
        offscreenContextProvider->VerifyContexts();
    }
    completion->signal();
}

void ThreadProxy::renderingStatsOnImplThread(CompletionEvent* completion, RenderingStats* stats)
{
    DCHECK(IsImplThread());
    m_layerTreeHostImpl->CollectRenderingStats(stats);
    completion->signal();
}

ThreadProxy::BeginFrameAndCommitState::BeginFrameAndCommitState()
    : memoryAllocationLimitBytes(0)
{
}

ThreadProxy::BeginFrameAndCommitState::~BeginFrameAndCommitState()
{
}

scoped_ptr<base::Value> ThreadProxy::AsValue() const
{
    scoped_ptr<base::DictionaryValue> state(new base::DictionaryValue());

    CompletionEvent completion;
    {
        DebugScopedSetMainThreadBlocked mainThreadBlocked(
            const_cast<ThreadProxy*>(this));
        Proxy::ImplThread()->postTask(base::Bind(&ThreadProxy::asValueOnImplThread,
                                                 m_implThreadWeakPtr,
                                                 &completion,
                                                 state.get()));
        completion.wait();
    }
    return state.PassAs<base::Value>();
}

void ThreadProxy::asValueOnImplThread(CompletionEvent* completion, base::DictionaryValue* state) const
{
    state->Set("layer_tree_host_impl", m_layerTreeHostImpl->AsValue().release());
    completion->signal();
}

bool ThreadProxy::CommitPendingForTesting()
{
    DCHECK(IsMainThread());
    CommitPendingRequest commitPendingRequest;
    {
        DebugScopedSetMainThreadBlocked mainThreadBlocked(this);
        Proxy::ImplThread()->postTask(base::Bind(&ThreadProxy::commitPendingOnImplThreadForTesting, m_implThreadWeakPtr, &commitPendingRequest));
        commitPendingRequest.completion.wait();
    }
    return commitPendingRequest.commitPending;
}

void ThreadProxy::commitPendingOnImplThreadForTesting(CommitPendingRequest* request)
{
    DCHECK(IsImplThread());
    if (m_layerTreeHostImpl->output_surface())
        request->commitPending = m_schedulerOnImplThread->commitPending();
    else
        request->commitPending = false;
    request->completion.signal();
}

skia::RefPtr<SkPicture> ThreadProxy::CapturePicture()
{
    DCHECK(IsMainThread());
    CompletionEvent completion;
    skia::RefPtr<SkPicture> picture;
    {
        DebugScopedSetMainThreadBlocked mainThreadBlocked(this);
        Proxy::ImplThread()->postTask(base::Bind(&ThreadProxy::capturePictureOnImplThread,
                                                 m_implThreadWeakPtr,
                                                 &completion,
                                                 &picture));
        completion.wait();
    }
    return picture;
}

void ThreadProxy::capturePictureOnImplThread(CompletionEvent* completion, skia::RefPtr<SkPicture>* picture)
{
    DCHECK(IsImplThread());
    *picture = m_layerTreeHostImpl->CapturePicture();
    completion->signal();
}

void ThreadProxy::RenewTreePriority()
{
    bool smoothnessTakesPriority =
        m_layerTreeHostImpl->pinch_gesture_active() ||
        m_layerTreeHostImpl->CurrentlyScrollingLayer() ||
        m_layerTreeHostImpl->page_scale_animation_active();

    // Update expiration time if smoothness currently takes priority.
    if (smoothnessTakesPriority) {
        m_smoothnessTakesPriorityExpirationTime = base::TimeTicks::Now() +
            base::TimeDelta::FromMilliseconds(
                smoothnessTakesPriorityExpirationDelay * 1000);
    }

    // We use the same priority for both trees by default.
    TreePriority priority = SAME_PRIORITY_FOR_BOTH_TREES;

    // Smoothness takes priority if expiration time is in the future.
    if (m_smoothnessTakesPriorityExpirationTime > base::TimeTicks::Now())
        priority = SMOOTHNESS_TAKES_PRIORITY;

    // New content always takes priority when the active tree has
    // evicted resources or there is an invalid viewport size.
    if (m_layerTreeHostImpl->active_tree()->ContentsTexturesPurged() ||
        m_layerTreeHostImpl->active_tree()->ViewportSizeInvalid())
        priority = NEW_CONTENT_TAKES_PRIORITY;

    m_layerTreeHostImpl->SetTreePriority(priority);

    // Notify the the client of this compositor via the output surface.
    // TODO(epenner): Route this to compositor-thread instead of output-surface
    // after GTFO refactor of compositor-thread (http://crbug/170828).
    if (m_layerTreeHostImpl->output_surface()) {
        m_layerTreeHostImpl->output_surface()->UpdateSmoothnessTakesPriority(
            priority == SMOOTHNESS_TAKES_PRIORITY);
    }

    base::TimeDelta delay = m_smoothnessTakesPriorityExpirationTime -
        base::TimeTicks::Now();

    // Need to make sure a delayed task is posted when we have smoothness
    // takes priority expiration time in the future.
    if (delay <= base::TimeDelta())
        return;
    if (m_renewTreePriorityOnImplThreadPending)
        return;

    Proxy::ImplThread()->postDelayedTask(
        base::Bind(&ThreadProxy::renewTreePriorityOnImplThread,
                   m_weakFactoryOnImplThread.GetWeakPtr()),
        delay.InMilliseconds());

    m_renewTreePriorityOnImplThreadPending = true;
}

void ThreadProxy::renewTreePriorityOnImplThread()
{
    DCHECK(m_renewTreePriorityOnImplThreadPending);
    m_renewTreePriorityOnImplThreadPending = false;

    RenewTreePriority();
}

}  // namespace cc
