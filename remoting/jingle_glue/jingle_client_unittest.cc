// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/synchronization/waitable_event.h"
#include "remoting/jingle_glue/jingle_client.h"
#include "remoting/jingle_glue/jingle_thread.h"
#include "remoting/jingle_glue/xmpp_signal_strategy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace remoting {

class MockJingleClientCallback : public JingleClient::Callback {
 public:
  ~MockJingleClientCallback() { }

  MOCK_METHOD2(OnStateChange, void(JingleClient*, JingleClient::State));
};

class JingleClientTest : public testing::Test {
 public:
  virtual ~JingleClientTest() { }

  static void OnClosed(bool* called) {
    *called = true;
  }

  // A helper that calls OnConnectionStateChanged(). Need this because we want
  // to call it on the jingle thread.
  static void ChangeState(XmppSignalStrategy* strategy,
                          buzz::XmppEngine::State state,
                          base::WaitableEvent* done_event) {
    strategy->OnConnectionStateChanged(state);
    if (done_event)
      done_event->Signal();
  }

 protected:
  virtual void SetUp() {
    thread_.Start();

    signal_strategy_.reset(new XmppSignalStrategy(&thread_, "", "", ""));
    client_ = new JingleClient(thread_.message_loop(), signal_strategy_.get(),
                               NULL, NULL, NULL, &callback_);
    // Fake initialization.
    client_->initialized_ = true;
    signal_strategy_->observer_ = client_;
  }

  JingleThread thread_;
  scoped_ptr<XmppSignalStrategy> signal_strategy_;
  scoped_refptr<JingleClient> client_;
  MockJingleClientCallback callback_;
};

TEST_F(JingleClientTest, OnStateChanged) {
  EXPECT_CALL(callback_, OnStateChange(_, JingleClient::CONNECTING))
      .Times(1);

  base::WaitableEvent state_changed_event(true, false);
  thread_.message_loop()->PostTask(FROM_HERE, NewRunnableFunction(
      &JingleClientTest::ChangeState, signal_strategy_.get(),
      buzz::XmppEngine::STATE_OPENING, &state_changed_event));
  state_changed_event.Wait();

  base::WaitableEvent closed_event(true, false);
  client_->Close(base::Bind(&base::WaitableEvent::Signal,
                            base::Unretained(&closed_event)));
  closed_event.Wait();

  thread_.Stop();
}

TEST_F(JingleClientTest, Close) {
  EXPECT_CALL(callback_, OnStateChange(_, _))
      .Times(0);
  base::WaitableEvent closed_event(true, false);
  client_->Close(base::Bind(&base::WaitableEvent::Signal,
                            base::Unretained(&closed_event)));
  closed_event.Wait();

  // Verify that the channel doesn't call callback anymore.
  thread_.message_loop()->PostTask(FROM_HERE, NewRunnableFunction(
      &JingleClientTest::ChangeState, signal_strategy_.get(),
      buzz::XmppEngine::STATE_OPENING,
      static_cast<base::WaitableEvent*>(NULL)));
  thread_.Stop();
}

TEST_F(JingleClientTest, ClosedTask) {
  bool closed = false;
  client_->Close(base::Bind(&JingleClientTest::OnClosed, &closed));
  thread_.Stop();
  EXPECT_TRUE(closed);
}

TEST_F(JingleClientTest, DoubleClose) {
  bool closed1 = false;
  client_->Close(base::Bind(&JingleClientTest::OnClosed, &closed1));
  bool closed2 = false;
  client_->Close(base::Bind(&JingleClientTest::OnClosed, &closed2));
  thread_.Stop();
  EXPECT_TRUE(closed1 && closed2);
}

}  // namespace remoting
