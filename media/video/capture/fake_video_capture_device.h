// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation of a fake VideoCaptureDevice class. Used for testing other
// video capture classes when no real hardware is available.

#ifndef MEDIA_VIDEO_CAPTURE_FAKE_VIDEO_CAPTURE_DEVICE_H_
#define MEDIA_VIDEO_CAPTURE_FAKE_VIDEO_CAPTURE_DEVICE_H_

#include <string>

#include "base/atomicops.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread.h"
#include "media/video/capture/video_capture_device.h"

namespace media {

class MEDIA_EXPORT FakeVideoCaptureDevice : public VideoCaptureDevice {
 public:
  static VideoCaptureDevice* Create(const Name& device_name);
  virtual ~FakeVideoCaptureDevice();
  // Used for testing. This will make sure the next call to Create will
  // return NULL;
  static void SetFailNextCreate();
  static void SetNumberOfFakeDevices(size_t number_of_devices);

  static void GetDeviceNames(Names* device_names);
  static void GetDeviceSupportedFormats(const Name& device,
                                        VideoCaptureCapabilities* formats);

  // VideoCaptureDevice implementation.
  virtual void AllocateAndStart(const VideoCaptureParams& params,
                                scoped_ptr<VideoCaptureDevice::Client> client)
      OVERRIDE;
  virtual void StopAndDeAllocate() OVERRIDE;

 private:
  // Flag indicating the internal state.
  enum InternalState {
    kIdle,
    kCapturing,
    kError
  };
  FakeVideoCaptureDevice();

  // Called on the capture_thread_.
  void OnCaptureTask();

  // EXPERIMENTAL, similar to allocate, but changes resolution and calls
  // client->OnFrameInfoChanged(VideoCaptureCapability&)
  void Reallocate();
  void PopulateFormatRoster();

  scoped_ptr<VideoCaptureDevice::Client> client_;
  InternalState state_;
  base::Thread capture_thread_;
  scoped_ptr<uint8[]> fake_frame_;
  int frame_count_;
  VideoCaptureFormat capture_format_;

  // When the device is configured as mutating video captures, this vector
  // holds the available ones which are used in sequence, restarting at the end.
  std::vector<VideoCaptureFormat> format_roster_;
  int format_roster_index_;

  static bool fail_next_create_;
  // |number_of_devices_| is atomic since tests can call SetNumberOfFakeDevices
  // on the IO thread to set |number_of_devices_|. The variable can be
  // read from a separate thread.
  // TODO(perkj): Make tests independent of global state. crbug/323913
  static base::subtle::Atomic32 number_of_devices_;

  DISALLOW_COPY_AND_ASSIGN(FakeVideoCaptureDevice);
};

}  // namespace media

#endif  // MEDIA_VIDEO_CAPTURE_FAKE_VIDEO_CAPTURE_DEVICE_H_
