// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/at_exit.h"
#include "base/message_loop.h"
#include "base/process_util.h"
#include "base/shared_memory.h"
#include "base/sync_socket.h"
#include "base/test/test_timeouts.h"
#include "content/common/media/audio_messages.h"
#include "content/renderer/media/audio_device.h"
#include "content/renderer/media/audio_message_filter.h"
#include "media/audio/audio_util.h"
#include "media/audio/sample_rates.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gmock_mutant.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::CancelableSyncSocket;
using base::SharedMemory;
using base::SyncSocket;
using testing::_;
using testing::DoAll;
using testing::Invoke;
using testing::Return;
using testing::WithArgs;

namespace {

// Derived from AudioDevice to gain access to the protected constructor.
class TestAudioDevice : public AudioDevice {
 public:
  explicit TestAudioDevice(const scoped_refptr<base::MessageLoopProxy>& io_loop)
      : AudioDevice(io_loop) {}

 protected:
  virtual ~TestAudioDevice() {}
};

class MockRenderCallback : public media::AudioRendererSink::RenderCallback {
 public:
  MockRenderCallback() {}
  virtual ~MockRenderCallback() {}

  MOCK_METHOD3(Render, int(const std::vector<float*>& audio_data,
                           int number_of_frames,
                           int audio_delay_milliseconds));
  MOCK_METHOD0(OnRenderError, void());
};

class MockAudioMessageFilter : public AudioMessageFilter {
 public:
  MockAudioMessageFilter() {}

  virtual bool Send(IPC::Message* message) OVERRIDE {
    bool handled = true;
    IPC_BEGIN_MESSAGE_MAP(MockAudioMessageFilter, *message)
      IPC_MESSAGE_HANDLER(AudioHostMsg_CreateStream, OnCreateStream)
      IPC_MESSAGE_HANDLER(AudioHostMsg_PlayStream, OnPlayStream)
      IPC_MESSAGE_HANDLER(AudioHostMsg_CloseStream, OnCloseStream)
      IPC_MESSAGE_HANDLER(AudioHostMsg_SetVolume, OnSetVolume)
      IPC_MESSAGE_HANDLER(AudioHostMsg_PauseStream, OnPauseStream)
      IPC_MESSAGE_HANDLER(AudioHostMsg_FlushStream, OnFlushStream)
      IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
    EXPECT_TRUE(handled);
    delete message;
    return true;
  }

  MOCK_METHOD2(OnCreateStream,
      void(int stream_id, const media::AudioParameters& params));
  MOCK_METHOD1(OnPlayStream, void(int stream_id));
  MOCK_METHOD1(OnCloseStream, void(int stream_id));
  MOCK_METHOD2(OnSetVolume, void(int stream_id, double volume));
  MOCK_METHOD1(OnPauseStream, void(int stream_id));
  MOCK_METHOD1(OnFlushStream, void(int stream_id));

 protected:
  virtual ~MockAudioMessageFilter() {}
};

// Creates a copy of a SyncSocket handle that we can give to AudioDevice.
// On Windows this means duplicating the pipe handle so that AudioDevice can
// call CloseHandle() (since ownership has been transferred), but on other
// platforms, we just copy the same socket handle since AudioDevice on those
// platforms won't actually own the socket (FileDescriptor.auto_close is false).
bool DuplicateSocketHandle(SyncSocket::Handle socket_handle,
                           SyncSocket::Handle* copy) {
#if defined(OS_WIN)
  HANDLE process = GetCurrentProcess();
  ::DuplicateHandle(process, socket_handle, process, copy,
                    0, FALSE, DUPLICATE_SAME_ACCESS);
  return *copy != NULL;
#else
  *copy = socket_handle;
  return *copy != -1;
#endif
}

ACTION_P2(SendPendingBytes, socket, pending_bytes) {
  socket->Send(&pending_bytes, sizeof(pending_bytes));
}

// Used to terminate a loop from a different thread than the loop belongs to.
// |loop| should be a MessageLoopProxy.
ACTION_P(QuitLoop, loop) {
  loop->PostTask(FROM_HERE, MessageLoop::QuitClosure());
}

}  // namespace.

class AudioDeviceTest : public testing::Test {
 public:
  AudioDeviceTest()
      : default_audio_parameters_(media::AudioParameters::AUDIO_PCM_LINEAR,
                                  CHANNEL_LAYOUT_STEREO,
                                  48000, 16, 1024),
        stream_id_(-1) {
  }

  ~AudioDeviceTest() {}

  virtual void SetUp() OVERRIDE {
    // This sets a global audio_message_filter pointer.  AudioDevice will pick
    // up a pointer to this variable via the static AudioMessageFilter::Get()
    // method.
    audio_message_filter_ = new MockAudioMessageFilter();
  }

  AudioDevice* CreateAudioDevice() {
    return new TestAudioDevice(io_loop_.message_loop_proxy());
  }

  void set_stream_id(int stream_id) { stream_id_ = stream_id; }

 protected:
  // Used to clean up TLS pointers that the test(s) will initialize.
  // Must remain the first member of this class.
  base::ShadowingAtExitManager at_exit_manager_;
  MessageLoopForIO io_loop_;
  const media::AudioParameters default_audio_parameters_;
  MockRenderCallback callback_;
  scoped_refptr<MockAudioMessageFilter> audio_message_filter_;
  int stream_id_;
};

// The simplest test for AudioDevice.  Used to test construction of AudioDevice
// and that the runtime environment is set up correctly (e.g. ChildProcess and
// AudioMessageFilter global pointers).
TEST_F(AudioDeviceTest, Initialize) {
  scoped_refptr<AudioDevice> audio_device(CreateAudioDevice());
  audio_device->Initialize(default_audio_parameters_, &callback_);
  io_loop_.RunAllPending();
}

// Calls Start() followed by an immediate Stop() and check for the basic message
// filter messages being sent in that case.
TEST_F(AudioDeviceTest, StartStop) {
  scoped_refptr<AudioDevice> audio_device(CreateAudioDevice());
  audio_device->Initialize(default_audio_parameters_, &callback_);

  audio_device->Start();
  audio_device->Stop();

  EXPECT_CALL(*audio_message_filter_, OnCreateStream(_, _));
  EXPECT_CALL(*audio_message_filter_, OnCloseStream(_));

  io_loop_.RunAllPending();
}

// Starts an audio stream, creates a shared memory section + SyncSocket pair
// that AudioDevice must use for audio data.  It then sends a request for
// a single audio packet and quits when the packet has been sent.
TEST_F(AudioDeviceTest, CreateStream) {
  scoped_refptr<AudioDevice> audio_device(CreateAudioDevice());
  audio_device->Initialize(default_audio_parameters_, &callback_);

  audio_device->Start();

  EXPECT_CALL(*audio_message_filter_, OnCreateStream(_, _))
      .WillOnce(WithArgs<0>(Invoke(this, &AudioDeviceTest::set_stream_id)));

  EXPECT_EQ(stream_id_, -1);
  io_loop_.RunAllPending();
  // OnCreateStream() must have been called and we should have a valid
  // stream id.
  ASSERT_NE(stream_id_, -1);

  // This is where it gets a bit hacky.  The shared memory contract between
  // AudioDevice and its browser side counter part includes a bit more than
  // just the audio data, so we must call TotalSharedMemorySizeInBytes() to get
  // the actual size needed to fit the audio data plus the extra data.
  int memory_size = media::TotalSharedMemorySizeInBytes(
      default_audio_parameters_.GetBytesPerBuffer());
  SharedMemory shared_memory;
  ASSERT_TRUE(shared_memory.CreateAnonymous(memory_size));

  CancelableSyncSocket browser_socket, renderer_socket;
  ASSERT_TRUE(CancelableSyncSocket::CreatePair(&browser_socket,
                                               &renderer_socket));

  // Create duplicates of the handles we pass to AudioDevice since ownership
  // will be transferred and AudioDevice is responsible for freeing.
  SyncSocket::Handle audio_device_socket = SyncSocket::kInvalidHandle;
  ASSERT_TRUE(DuplicateSocketHandle(renderer_socket.handle(),
                                    &audio_device_socket));
  base::SharedMemoryHandle duplicated_memory_handle;
  ASSERT_TRUE(shared_memory.ShareToProcess(base::GetCurrentProcessHandle(),
                                           &duplicated_memory_handle));

  // We should get a 'play' notification when we call OnStreamCreated().
  // Respond by asking for some audio data.  This should ask our callback
  // to provide some audio data that AudioDevice then writes into the shared
  // memory section.
  EXPECT_CALL(*audio_message_filter_, OnPlayStream(stream_id_))
      .WillOnce(SendPendingBytes(&browser_socket, memory_size));

  // We expect calls to our audio renderer callback, which returns the number
  // of frames written to the memory section.
  // Here's the second place where it gets hacky:  There's no way for us to
  // know (without using a sleep loop!) when the AudioDevice has finished
  // writing the interleaved audio data into the shared memory section.
  // So, for the sake of this test, we consider the call to Render a sign
  // of success and quit the loop.
  EXPECT_CALL(callback_, Render(_, _, _))
      .WillOnce(DoAll(
          QuitLoop(io_loop_.message_loop_proxy()),
          Return(1)));

  audio_device->OnStreamCreated(duplicated_memory_handle, audio_device_socket,
                                memory_size);

  io_loop_.PostDelayedTask(FROM_HERE, MessageLoop::QuitClosure(),
                           TestTimeouts::action_timeout());
  io_loop_.Run();

  // Close the stream sequence.

  EXPECT_CALL(*audio_message_filter_, OnCloseStream(stream_id_));

  audio_device->Stop();
  io_loop_.RunAllPending();
}
