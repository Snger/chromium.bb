// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/capture/fake_video_capture_device.h"

#include <string>

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "media/audio/fake_audio_input_stream.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPaint.h"

namespace media {

static const int kFakeCaptureTimeoutMs = 50;
static const int kFakeCaptureBeepCycle = 20;  // Visual beep every 1s.
static const int kFakeCaptureCapabilityChangePeriod = 30;
enum { kNumberOfFakeDevices = 2 };

bool FakeVideoCaptureDevice::fail_next_create_ = false;
base::subtle::Atomic32 FakeVideoCaptureDevice::number_of_devices_ =
    kNumberOfFakeDevices;

void FakeVideoCaptureDevice::GetDeviceNames(Names* const device_names) {
  // Empty the name list.
  device_names->erase(device_names->begin(), device_names->end());

  int number_of_devices = base::subtle::NoBarrier_Load(&number_of_devices_);
  for (int32 n = 0; n < number_of_devices; n++) {
    Name name("fake_device_" + base::IntToString(n),
              "/dev/video" + base::IntToString(n));
    device_names->push_back(name);
  }
}

void FakeVideoCaptureDevice::GetDeviceSupportedFormats(
    const Name& device,
    VideoCaptureCapabilities* formats) {
  VideoCaptureCapability capture_format_640x480;
  capture_format_640x480.supported_format.frame_size.SetSize(640, 480);
  capture_format_640x480.supported_format.frame_rate =
      1000 / kFakeCaptureTimeoutMs;
  capture_format_640x480.supported_format.pixel_format =
      media::PIXEL_FORMAT_I420;
  formats->push_back(capture_format_640x480);
}

VideoCaptureDevice* FakeVideoCaptureDevice::Create(const Name& device_name) {
  if (fail_next_create_) {
    fail_next_create_ = false;
    return NULL;
  }
  int number_of_devices = base::subtle::NoBarrier_Load(&number_of_devices_);
  for (int32 n = 0; n < number_of_devices; ++n) {
    std::string possible_id = "/dev/video" + base::IntToString(n);
    if (device_name.id().compare(possible_id) == 0) {
      return new FakeVideoCaptureDevice();
    }
  }
  return NULL;
}

void FakeVideoCaptureDevice::SetFailNextCreate() {
  fail_next_create_ = true;
}

void FakeVideoCaptureDevice::SetNumberOfFakeDevices(size_t number_of_devices) {
  base::subtle::NoBarrier_AtomicExchange(&number_of_devices_,
                                         number_of_devices);
}

FakeVideoCaptureDevice::FakeVideoCaptureDevice()
    : state_(kIdle),
      capture_thread_("CaptureThread"),
      frame_count_(0),
      format_roster_index_(0) {}

FakeVideoCaptureDevice::~FakeVideoCaptureDevice() {
  // Check if the thread is running.
  // This means that the device have not been DeAllocated properly.
  DCHECK(!capture_thread_.IsRunning());
}

void FakeVideoCaptureDevice::AllocateAndStart(
    const VideoCaptureParams& params,
    scoped_ptr<VideoCaptureDevice::Client> client) {
  if (params.allow_resolution_change)
    PopulateFormatRoster();

  if (state_ != kIdle) {
    return;  // Wrong state.
  }

  client_ = client.Pass();
  capture_format_.pixel_format = PIXEL_FORMAT_I420;
  if (params.requested_format.frame_size.width() > 320) {  // VGA
    capture_format_.frame_size.SetSize(640, 480);
    capture_format_.frame_rate = 30;
  } else {  // QVGA
    capture_format_.frame_size.SetSize(320, 240);
    capture_format_.frame_rate = 30;
  }

  const size_t fake_frame_size =
      VideoFrame::AllocationSize(VideoFrame::I420, capture_format_.frame_size);
  fake_frame_.reset(new uint8[fake_frame_size]);

  state_ = kCapturing;
  capture_thread_.Start();
  capture_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&FakeVideoCaptureDevice::OnCaptureTask,
                 base::Unretained(this)));
}

void FakeVideoCaptureDevice::Reallocate() {
  DCHECK_EQ(state_, kCapturing);
  capture_format_ =
      format_roster_.at(++format_roster_index_ % format_roster_.size());
  DCHECK_EQ(capture_format_.pixel_format, PIXEL_FORMAT_I420);
  DVLOG(3) << "Reallocating FakeVideoCaptureDevice, new capture resolution "
           << capture_format_.frame_size.ToString();

  const size_t fake_frame_size =
      VideoFrame::AllocationSize(VideoFrame::I420, capture_format_.frame_size);
  fake_frame_.reset(new uint8[fake_frame_size]);
}

void FakeVideoCaptureDevice::StopAndDeAllocate() {
  if (state_ != kCapturing) {
    return;  // Wrong state.
  }
  capture_thread_.Stop();
  state_ = kIdle;
}

void FakeVideoCaptureDevice::OnCaptureTask() {
  if (state_ != kCapturing) {
    return;
  }

  const size_t frame_size =
      VideoFrame::AllocationSize(VideoFrame::I420, capture_format_.frame_size);
  memset(fake_frame_.get(), 0, frame_size);

  SkBitmap bitmap;
  bitmap.setConfig(SkBitmap::kA8_Config,
                   capture_format_.frame_size.width(),
                   capture_format_.frame_size.height(),
                   capture_format_.frame_size.width()),
      bitmap.setPixels(fake_frame_.get());

  SkCanvas canvas(bitmap);

  // Draw a sweeping circle to show an animation.
  int radius = std::min(capture_format_.frame_size.width(),
                        capture_format_.frame_size.height()) /
               4;
  SkRect rect =
      SkRect::MakeXYWH(capture_format_.frame_size.width() / 2 - radius,
                       capture_format_.frame_size.height() / 2 - radius,
                       2 * radius,
                       2 * radius);

  SkPaint paint;
  paint.setStyle(SkPaint::kFill_Style);

  // Only Y plane is being drawn and this gives 50% grey on the Y
  // plane. The result is a light green color in RGB space.
  paint.setAlpha(128);

  int end_angle = (frame_count_ % kFakeCaptureBeepCycle * 360) /
      kFakeCaptureBeepCycle;
  if (!end_angle)
    end_angle = 360;
  canvas.drawArc(rect, 0, end_angle, true, paint);

  // Draw current time.
  int elapsed_ms = kFakeCaptureTimeoutMs * frame_count_;
  int milliseconds = elapsed_ms % 1000;
  int seconds = (elapsed_ms / 1000) % 60;
  int minutes = (elapsed_ms / 1000 / 60) % 60;
  int hours = (elapsed_ms / 1000 / 60 / 60) % 60;

  std::string time_string =
      base::StringPrintf("%d:%02d:%02d:%03d %d", hours, minutes,
                         seconds, milliseconds, frame_count_);
  canvas.scale(3, 3);
  canvas.drawText(time_string.data(), time_string.length(), 30, 20,
                  paint);

  if (frame_count_ % kFakeCaptureBeepCycle == 0) {
    // Generate a synchronized beep sound if there is one audio input
    // stream created.
    FakeAudioInputStream::BeepOnce();
  }

  frame_count_++;

  // Give the captured frame to the client.
  client_->OnIncomingCapturedFrame(fake_frame_.get(),
                                   frame_size,
                                   base::Time::Now(),
                                   0,
                                   false,
                                   false,
                                   capture_format_);
  if (!(frame_count_ % kFakeCaptureCapabilityChangePeriod) &&
      format_roster_.size() > 0U) {
    Reallocate();
  }
  // Reschedule next CaptureTask.
  capture_thread_.message_loop()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&FakeVideoCaptureDevice::OnCaptureTask,
                 base::Unretained(this)),
      base::TimeDelta::FromMilliseconds(kFakeCaptureTimeoutMs));
}

void FakeVideoCaptureDevice::PopulateFormatRoster() {
  format_roster_.push_back(
      media::VideoCaptureFormat(gfx::Size(320, 240), 30, PIXEL_FORMAT_I420));
  format_roster_.push_back(
      media::VideoCaptureFormat(gfx::Size(640, 480), 30, PIXEL_FORMAT_I420));
  format_roster_.push_back(
      media::VideoCaptureFormat(gfx::Size(800, 600), 30, PIXEL_FORMAT_I420));

  format_roster_index_ = 0;
}

}  // namespace media
