// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "media/base/android/media_codec_bridge.h"
#include "media/base/android/media_player_manager.h"
#include "media/base/android/media_source_player.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/gl/android/surface_texture_bridge.h"

namespace media {

// Mock of MediaPlayerManager for testing purpose
class MockMediaPlayerManager : public MediaPlayerManager {
 public:
  MockMediaPlayerManager() : num_requests_(0), last_seek_request_id_(0) {}
  virtual ~MockMediaPlayerManager() {};

  // MediaPlayerManager implementation.
  virtual void RequestMediaResources(int player_id) OVERRIDE {}
  virtual void ReleaseMediaResources(int player_id) OVERRIDE {}
  virtual MediaResourceGetter* GetMediaResourceGetter() OVERRIDE {
      return NULL;
  }
  virtual void OnTimeUpdate(int player_id,
                            base::TimeDelta current_time) OVERRIDE {}
  virtual void OnMediaMetadataChanged(
      int player_id, base::TimeDelta duration, int width, int height,
      bool success) OVERRIDE {}
  virtual void OnPlaybackComplete(int player_id) OVERRIDE {}
  virtual void OnMediaInterrupted(int player_id) OVERRIDE {}
  virtual void OnBufferingUpdate(int player_id, int percentage) OVERRIDE {}
  virtual void OnSeekComplete(int player_id,
                              base::TimeDelta current_time) OVERRIDE {}
  virtual void OnError(int player_id, int error) OVERRIDE {}
  virtual void OnVideoSizeChanged(int player_id, int width,
                                  int height) OVERRIDE {}
  virtual MediaPlayerAndroid* GetFullscreenPlayer() OVERRIDE { return NULL; }
  virtual MediaPlayerAndroid* GetPlayer(int player_id) OVERRIDE { return NULL; }
  virtual void DestroyAllMediaPlayers() OVERRIDE {}
  virtual void OnReadFromDemuxer(int player_id, media::DemuxerStream::Type type,
                                 bool seek_done) OVERRIDE {
    num_requests_++;
  }
  virtual void OnMediaSeekRequest(int player_id, base::TimeDelta time_to_seek,
                                  unsigned seek_request_id) OVERRIDE {
    last_seek_request_id_ = seek_request_id;
  }
  virtual void OnMediaConfigRequest(int player_id) OVERRIDE {}
  virtual media::MediaDrmBridge* GetDrmBridge(int media_keys_id) OVERRIDE {
    return NULL;
  }
  virtual void OnKeyAdded(int key_id,
                          const std::string& key_system,
                          const std::string& session_id) OVERRIDE {}
  virtual void OnKeyError(int key_id,
                          const std::string& key_system,
                          const std::string& session_id,
                          media::MediaKeys::KeyError error_code,
                          int system_code) OVERRIDE {}
  virtual void OnKeyMessage(int key_id,
                            const std::string& key_system,
                            const std::string& session_id,
                            const std::string& message,
                            const std::string& destination_url) OVERRIDE {}

  int num_requests() { return num_requests_; }
  unsigned last_seek_request_id() { return last_seek_request_id_; }

 private:
  // The number of request this object sents for decoding data.
  int num_requests_;
  unsigned last_seek_request_id_;
};

class MediaSourcePlayerTest : public testing::Test {
 public:
  MediaSourcePlayerTest() {
    manager_.reset(new MockMediaPlayerManager());
    player_.reset(new MediaSourcePlayer(0, manager_.get()));
  }
  virtual ~MediaSourcePlayerTest() {}

 protected:
  // Get the decoder job from the MediaSourcePlayer.
  MediaDecoderJob* GetMediaDecoderJob(bool is_audio) {
    if (is_audio)
      return player_->audio_decoder_job_.get();
    return player_->video_decoder_job_.get();
  }

  // Starts decoding the data.
  void Start(const MediaPlayerHostMsg_DemuxerReady_Params& params) {
    player_->DemuxerReady(params);
    player_->Start();
  }

  // Set the surface to the player.
  void SetVideoSurface(gfx::ScopedJavaSurface surface) {
    unsigned last_seek_request_id = manager_->last_seek_request_id();
    // Calling SetVideoSurface will trigger a seek.
    player_->SetVideoSurface(surface.Pass());
    EXPECT_EQ(last_seek_request_id + 1, manager_->last_seek_request_id());
    // Sending back the seek ACK.
    player_->OnSeekRequestAck(manager_->last_seek_request_id());
  }

 protected:
  scoped_ptr<MockMediaPlayerManager> manager_;
  scoped_ptr<MediaSourcePlayer> player_;

  DISALLOW_COPY_AND_ASSIGN(MediaSourcePlayerTest);
};


TEST_F(MediaSourcePlayerTest, StartAudioDecoderWithValidConfig) {
  if (!MediaCodecBridge::IsAvailable())
    return;

  // Test audio decoder job will be created when codec is successfully started.
  MediaPlayerHostMsg_DemuxerReady_Params params;
  params.audio_codec = kCodecVorbis;
  params.audio_channels = 2;
  params.audio_sampling_rate = 44100;
  params.is_audio_encrypted = false;
  Start(params);
  EXPECT_TRUE(NULL != GetMediaDecoderJob(true));
  EXPECT_EQ(1, manager_->num_requests());
}

TEST_F(MediaSourcePlayerTest, StartAudioDecoderWithInvalidConfig) {
  if (!MediaCodecBridge::IsAvailable())
    return;

  // Test audio decoder job will not be created when failed to start the codec.
  MediaPlayerHostMsg_DemuxerReady_Params params;
  params.audio_codec = kCodecVorbis;
  params.audio_channels = 2;
  params.audio_sampling_rate = 44100;
  params.is_audio_encrypted = false;
  uint8 invalid_codec_data[] = { 0x00, 0xff, 0xff, 0xff, 0xff };
  params.audio_extra_data.insert(params.audio_extra_data.begin(),
                                 invalid_codec_data, invalid_codec_data + 4);
  Start(params);
  EXPECT_TRUE(NULL == GetMediaDecoderJob(true));
  EXPECT_EQ(0, manager_->num_requests());
}

TEST_F(MediaSourcePlayerTest, StartVideoCodecWithValidSurface) {
  if (!MediaCodecBridge::IsAvailable())
    return;

  // Test video decoder job will be created when surface is valid.
  scoped_refptr<gfx::SurfaceTextureBridge> surface_texture(
      new gfx::SurfaceTextureBridge(0));
  gfx::ScopedJavaSurface surface(surface_texture.get());
  MediaPlayerHostMsg_DemuxerReady_Params params;
  params.video_codec = kCodecVP8;
  params.video_size = gfx::Size(320, 240);
  params.is_video_encrypted = false;
  Start(params);
  // Video decoder job will not be created until surface is available.
  EXPECT_TRUE(NULL == GetMediaDecoderJob(false));
  EXPECT_EQ(0, manager_->num_requests());

  SetVideoSurface(surface.Pass());
  // The decoder job should be ready now.
  EXPECT_TRUE(NULL != GetMediaDecoderJob(false));
  EXPECT_EQ(1, manager_->num_requests());
}

TEST_F(MediaSourcePlayerTest, StartVideoCodecWithInvalidSurface) {
  if (!MediaCodecBridge::IsAvailable())
    return;

  // Test video decoder job will be created when surface is valid.
  scoped_refptr<gfx::SurfaceTextureBridge> surface_texture(
      new gfx::SurfaceTextureBridge(0));
  gfx::ScopedJavaSurface surface(surface_texture.get());
  MediaPlayerHostMsg_DemuxerReady_Params params;
  params.video_codec = kCodecVP8;
  params.video_size = gfx::Size(320, 240);
  params.is_video_encrypted = false;
  Start(params);
  // Video decoder job will not be created until surface is available.
  EXPECT_TRUE(NULL == GetMediaDecoderJob(false));
  EXPECT_EQ(0, manager_->num_requests());

  // Release the surface texture.
  surface_texture = NULL;
  SetVideoSurface(surface.Pass());
  EXPECT_TRUE(NULL == GetMediaDecoderJob(false));
  EXPECT_EQ(0, manager_->num_requests());
}

TEST_F(MediaSourcePlayerTest, ReadFromDemuxerAfterSeek) {
  if (!MediaCodecBridge::IsAvailable())
    return;

  // Test decoder job will resend a ReadFromDemuxer request after seek.
  MediaPlayerHostMsg_DemuxerReady_Params params;
  params.audio_codec = kCodecVorbis;
  params.audio_channels = 2;
  params.audio_sampling_rate = 44100;
  params.is_audio_encrypted = false;
  Start(params);
  EXPECT_TRUE(NULL != GetMediaDecoderJob(true));
  EXPECT_EQ(1, manager_->num_requests());

  // Initiate a seek
  player_->SeekTo(base::TimeDelta());
  EXPECT_EQ(1u, manager_->last_seek_request_id());
  // Sending back the seek ACK, this should trigger the player to call
  // OnReadFromDemuxer() again.
  player_->OnSeekRequestAck(manager_->last_seek_request_id());
  EXPECT_EQ(2, manager_->num_requests());
}


}  // namespace media
