// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/desktop_environment.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "remoting/host/capturer.h"
#include "remoting/host/chromoting_host.h"
#include "remoting/host/chromoting_host_context.h"
#include "remoting/host/continue_window.h"
#include "remoting/host/curtain.h"
#include "remoting/host/disconnect_window.h"
#include "remoting/host/event_executor.h"
#include "remoting/host/local_input_monitor.h"

static const int kContinueWindowTimeoutMs = 10 * 60 * 1000;

namespace remoting {

// UIThreadProxy proxies DesktopEnvironment method calls to the UI
// thread. This is neccessary so that DesktopEnvironment can be
// deleted synchronously even while there are pending tasks on the
// message queue.
class UIThreadProxy : public base::RefCountedThreadSafe<UIThreadProxy> {
 public:
  UIThreadProxy(ChromotingHostContext* context)
      : context_(context) {
  }

  void Detach() {
    DCHECK(context_->IsUIThread());
    context_ = NULL;
  }

  void CallOnUIThread(const tracked_objects::Location& from_here,
                      const base::Closure& closure) {
    if (context_) {
      context_->PostTaskToUIThread(from_here, NewRunnableMethod(
          this, &UIThreadProxy::CallClosure, closure));
    }
  }

  void CallOnUIThreadDelayed(const tracked_objects::Location& from_here,
                             const base::Closure& closure,
                             int delay_ms) {
    if (context_) {
      context_->PostDelayedTaskToUIThread(from_here, NewRunnableMethod(
          this, &UIThreadProxy::CallClosure, closure), delay_ms);
    }
  }

 private:
  friend class base::RefCountedThreadSafe<UIThreadProxy>;

  virtual ~UIThreadProxy() { }

  void CallClosure(const base::Closure& closure) {
    if (context_)
      closure.Run();
  }

  ChromotingHostContext* context_;

  DISALLOW_COPY_AND_ASSIGN(UIThreadProxy);
};

// static
DesktopEnvironment* DesktopEnvironment::Create(ChromotingHostContext* context) {
  Capturer* capturer = Capturer::Create();
  EventExecutor* event_executor =
      EventExecutor::Create(context->desktop_message_loop(), capturer);
  Curtain* curtain = Curtain::Create();
  DisconnectWindow* disconnect_window = DisconnectWindow::Create();
  ContinueWindow* continue_window = ContinueWindow::Create();
  LocalInputMonitor* local_input_monitor = LocalInputMonitor::Create();

  return new DesktopEnvironment(context, capturer, event_executor, curtain,
                                disconnect_window, continue_window,
                                local_input_monitor);
}

DesktopEnvironment::DesktopEnvironment(ChromotingHostContext* context,
                                       Capturer* capturer,
                                       EventExecutor* event_executor,
                                       Curtain* curtain,
                                       DisconnectWindow* disconnect_window,
                                       ContinueWindow* continue_window,
                                       LocalInputMonitor* local_input_monitor)
    : host_(NULL),
      context_(context),
      capturer_(capturer),
      event_executor_(event_executor),
      curtain_(curtain),
      disconnect_window_(disconnect_window),
      continue_window_(continue_window),
      local_input_monitor_(local_input_monitor),
      is_monitoring_local_inputs_(false),
      continue_timer_started_(false),
      proxy_(new UIThreadProxy(context)) {
}

DesktopEnvironment::~DesktopEnvironment() {
}

void DesktopEnvironment::Shutdown() {
  DCHECK(context_->IsUIThread());

  MonitorLocalInputs(false);
  ShowDisconnectWindow(false, std::string());
  ShowContinueWindow(false);
  StartContinueWindowTimer(false);

  proxy_->Detach();
}

void DesktopEnvironment::OnConnect(const std::string& username) {
  proxy_->CallOnUIThread(FROM_HERE, base::Bind(
      &DesktopEnvironment::ProcessOnConnect, base::Unretained(this), username));
}

void DesktopEnvironment::OnLastDisconnect() {
  proxy_->CallOnUIThread(FROM_HERE, base::Bind(
      &DesktopEnvironment::ProcessOnLastDisconnect, base::Unretained(this)));
}

void DesktopEnvironment::OnPause(bool pause) {
  proxy_->CallOnUIThread(FROM_HERE, base::Bind(
      &DesktopEnvironment::ProcessOnPause, base::Unretained(this), pause));
}

void DesktopEnvironment::ProcessOnConnect(const std::string& username) {
  DCHECK(context_->IsUIThread());

  MonitorLocalInputs(true);
  ShowDisconnectWindow(true, username);
  StartContinueWindowTimer(true);
}

void DesktopEnvironment::ProcessOnLastDisconnect() {
  DCHECK(context_->IsUIThread());

  MonitorLocalInputs(false);
  ShowDisconnectWindow(false, std::string());
  ShowContinueWindow(false);
  StartContinueWindowTimer(false);
}

void DesktopEnvironment::ProcessOnPause(bool pause) {
  StartContinueWindowTimer(!pause);
}

void DesktopEnvironment::MonitorLocalInputs(bool enable) {
  DCHECK(context_->IsUIThread());

  if (enable == is_monitoring_local_inputs_)
    return;
  if (enable) {
    local_input_monitor_->Start(host_);
  } else {
    local_input_monitor_->Stop();
  }
  is_monitoring_local_inputs_ = enable;
}

void DesktopEnvironment::ShowDisconnectWindow(bool show,
                                              const std::string& username) {
  DCHECK(context_->IsUIThread());

  if (show) {
    disconnect_window_->Show(host_, username);
  } else {
    disconnect_window_->Hide();
  }
}

void DesktopEnvironment::ShowContinueWindow(bool show) {
  DCHECK(context_->IsUIThread());

  if (show) {
    continue_window_->Show(host_);
  } else {
    continue_window_->Hide();
  }
}

void DesktopEnvironment::StartContinueWindowTimer(bool start) {
  DCHECK(context_->IsUIThread());

  if (start && ! continue_timer_started_) {
    continue_timer_target_time_ = base::Time::Now() +
        base::TimeDelta::FromMilliseconds(kContinueWindowTimeoutMs);
    proxy_->CallOnUIThreadDelayed(
        FROM_HERE, base::Bind(&DesktopEnvironment::ContinueWindowTimerFunc,
                              base::Unretained(this)),
        kContinueWindowTimeoutMs);
  }

  continue_timer_started_ = start;
}

void DesktopEnvironment::ContinueWindowTimerFunc() {
  DCHECK(context_->IsUIThread());

  // This function may be called prematurely if timer was stopped and
  // then started again. In that case we just ignore this call.
  if (continue_timer_target_time_ > base::Time::Now())
    return;

  host_->PauseSession(true);
  ShowContinueWindow(true);
}


}  // namespace remoting
