// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/video_scheduler.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/codec/video_encoder.h"
#include "remoting/host/host_mock_objects.h"
#include "remoting/proto/control.pb.h"
#include "remoting/proto/video.pb.h"
#include "remoting/protocol/protocol_mock_objects.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor.h"
#include "third_party/webrtc/modules/desktop_capture/screen_capturer_mock_objects.h"

using ::remoting::protocol::MockClientStub;
using ::remoting::protocol::MockVideoStub;

using ::testing::_;
using ::testing::AtLeast;
using ::testing::AnyNumber;
using ::testing::DeleteArg;
using ::testing::DoAll;
using ::testing::Expectation;
using ::testing::InSequence;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::SaveArg;

namespace remoting {

namespace {

ACTION(FinishEncode) {
  scoped_ptr<VideoPacket> packet(new VideoPacket());
  return packet.release();
}

ACTION(FinishSend) {
  arg1.Run();
}

}  // namespace

static const int kWidth = 640;
static const int kHeight = 480;
static const int kCursorWidth = 64;
static const int kCursorHeight = 32;
static const int kHotspotX = 11;
static const int kHotspotY = 12;

class MockVideoEncoder : public VideoEncoder {
 public:
  MockVideoEncoder();
  virtual ~MockVideoEncoder();

  scoped_ptr<VideoPacket> Encode(
      const webrtc::DesktopFrame& frame) {
    return scoped_ptr<VideoPacket>(EncodePtr(frame));
  }
  MOCK_METHOD1(EncodePtr, VideoPacket*(const webrtc::DesktopFrame& frame));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockVideoEncoder);
};

MockVideoEncoder::MockVideoEncoder() {}

MockVideoEncoder::~MockVideoEncoder() {}

class VideoSchedulerTest : public testing::Test {
 public:
  VideoSchedulerTest();

  virtual void SetUp() OVERRIDE;

  void StartVideoScheduler(
      scoped_ptr<webrtc::ScreenCapturer> capturer,
      scoped_ptr<webrtc::MouseCursorMonitor> mouse_monitor);
  void StopVideoScheduler();

  // webrtc::ScreenCapturer mocks.
  void OnCapturerStart(webrtc::ScreenCapturer::Callback* callback);
  void OnCaptureFrame(const webrtc::DesktopRegion& region);
  void OnMouseCursorMonitorInit(
      webrtc::MouseCursorMonitor::Callback* callback,
      webrtc::MouseCursorMonitor::Mode mode);
void OnCaptureMouse();
void SetCursorShape(const protocol::CursorShapeInfo& cursor_shape);

 protected:
  base::MessageLoop message_loop_;
  base::RunLoop run_loop_;
  scoped_refptr<AutoThreadTaskRunner> task_runner_;
  scoped_refptr<VideoScheduler> scheduler_;

  MockClientStub client_stub_;
  MockVideoStub video_stub_;

  // The following mock objects are owned by VideoScheduler.
  MockVideoEncoder* encoder_;

  scoped_ptr<webrtc::DesktopFrame> frame_;
  scoped_ptr<webrtc::MouseCursor> mouse_cursor_;

  // Points to the callback passed to webrtc::ScreenCapturer::Start().
  webrtc::ScreenCapturer::Callback* capturer_callback_;

  // Points to the callback passed to webrtc::MouseCursor::Init().
  webrtc::MouseCursorMonitor::Callback* mouse_monitor_callback_;

 private:
  DISALLOW_COPY_AND_ASSIGN(VideoSchedulerTest);
};

VideoSchedulerTest::VideoSchedulerTest()
    : encoder_(NULL),
      capturer_callback_(NULL),
      mouse_monitor_callback_(NULL) {
}

void VideoSchedulerTest::SetUp() {
  task_runner_ = new AutoThreadTaskRunner(
      message_loop_.message_loop_proxy(), run_loop_.QuitClosure());

  encoder_ = new MockVideoEncoder();
}

void VideoSchedulerTest::StartVideoScheduler(
    scoped_ptr<webrtc::ScreenCapturer> capturer,
    scoped_ptr<webrtc::MouseCursorMonitor> mouse_monitor) {
  scheduler_ = new VideoScheduler(
      task_runner_, // Capture
      task_runner_, // Encode
      task_runner_, // Network
      capturer.Pass(),
      mouse_monitor.Pass(),
      scoped_ptr<VideoEncoder>(encoder_),
      &client_stub_,
      &video_stub_);
  scheduler_->Start();
}

void VideoSchedulerTest::StopVideoScheduler() {
  scheduler_->Stop();
  scheduler_ = NULL;
}

void VideoSchedulerTest::OnCapturerStart(
    webrtc::ScreenCapturer::Callback* callback) {
  EXPECT_FALSE(capturer_callback_);
  EXPECT_TRUE(callback);

  capturer_callback_ = callback;
}

void VideoSchedulerTest::OnCaptureFrame(const webrtc::DesktopRegion& region) {
  frame_->mutable_updated_region()->SetRect(
      webrtc::DesktopRect::MakeXYWH(0, 0, 10, 10));
  capturer_callback_->OnCaptureCompleted(frame_.release());
}

void VideoSchedulerTest::OnCaptureMouse() {
  EXPECT_TRUE(mouse_monitor_callback_);
  mouse_monitor_callback_->OnMouseCursor(mouse_cursor_.release());
}

void VideoSchedulerTest::OnMouseCursorMonitorInit(
    webrtc::MouseCursorMonitor::Callback* callback,
    webrtc::MouseCursorMonitor::Mode mode) {
  EXPECT_FALSE(mouse_monitor_callback_);
  EXPECT_TRUE(callback);

  mouse_monitor_callback_ = callback;
}

void VideoSchedulerTest::SetCursorShape(
    const protocol::CursorShapeInfo& cursor_shape) {
  EXPECT_TRUE(cursor_shape.has_width());
  EXPECT_EQ(kCursorWidth, cursor_shape.width());
  EXPECT_TRUE(cursor_shape.has_height());
  EXPECT_EQ(kCursorHeight, cursor_shape.height());
  EXPECT_TRUE(cursor_shape.has_hotspot_x());
  EXPECT_EQ(kHotspotX, cursor_shape.hotspot_x());
  EXPECT_TRUE(cursor_shape.has_hotspot_y());
  EXPECT_EQ(kHotspotY, cursor_shape.hotspot_y());
  EXPECT_TRUE(cursor_shape.has_data());
  EXPECT_EQ(kCursorWidth * kCursorHeight * webrtc::DesktopFrame::kBytesPerPixel,
            static_cast<int>(cursor_shape.data().size()));
}

// This test mocks capturer, encoder and network layer to simulate one capture
// cycle. When the first encoded packet is submitted to the network
// VideoScheduler is instructed to come to a complete stop. We expect the stop
// sequence to be executed successfully.
TEST_F(VideoSchedulerTest, StartAndStop) {
  scoped_ptr<webrtc::MockScreenCapturer> capturer(
      new webrtc::MockScreenCapturer());
  scoped_ptr<MockMouseCursorMonitor> cursor_monitor(
      new MockMouseCursorMonitor());

  {
    InSequence s;

    EXPECT_CALL(*cursor_monitor, Init(_, _))
        .WillOnce(
            Invoke(this, &VideoSchedulerTest::OnMouseCursorMonitorInit));

    EXPECT_CALL(*cursor_monitor, Capture())
        .WillRepeatedly(Invoke(this, &VideoSchedulerTest::OnCaptureMouse));
  }

  Expectation capturer_start =
      EXPECT_CALL(*capturer, Start(_))
          .WillOnce(Invoke(this, &VideoSchedulerTest::OnCapturerStart));

  frame_.reset(new webrtc::BasicDesktopFrame(
      webrtc::DesktopSize(kWidth, kHeight)));

  mouse_cursor_.reset(new webrtc::MouseCursor(
      new webrtc::BasicDesktopFrame(webrtc::DesktopSize(kCursorWidth,
                                                        kCursorHeight)),
      webrtc::DesktopVector(kHotspotX, kHotspotY)));

  // First the capturer is called.
  Expectation capturer_capture = EXPECT_CALL(*capturer, Capture(_))
      .After(capturer_start)
      .WillRepeatedly(Invoke(this, &VideoSchedulerTest::OnCaptureFrame));

  // Expect the encoder be called.
  EXPECT_CALL(*encoder_, EncodePtr(_))
      .WillRepeatedly(FinishEncode());

  // By default delete the arguments when ProcessVideoPacket is received.
  EXPECT_CALL(video_stub_, ProcessVideoPacketPtr(_, _))
      .WillRepeatedly(FinishSend());

  EXPECT_CALL(client_stub_, SetCursorShape(_))
      .WillOnce(Invoke(this, &VideoSchedulerTest::SetCursorShape));

  // For the first time when ProcessVideoPacket is received we stop the
  // VideoScheduler.
  EXPECT_CALL(video_stub_, ProcessVideoPacketPtr(_, _))
      .WillOnce(DoAll(
          FinishSend(),
          InvokeWithoutArgs(this, &VideoSchedulerTest::StopVideoScheduler)))
      .RetiresOnSaturation();

  // Start video frame capture.
  StartVideoScheduler(capturer.PassAs<webrtc::ScreenCapturer>(),
                      cursor_monitor.PassAs<webrtc::MouseCursorMonitor>());

  task_runner_ = NULL;
  run_loop_.Run();
}

}  // namespace remoting
