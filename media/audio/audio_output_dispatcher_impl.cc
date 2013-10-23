// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_output_dispatcher_impl.h"

#include <algorithm>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/message_loop/message_loop.h"
#include "base/time/time.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_output_proxy.h"

namespace media {

AudioOutputDispatcherImpl::AudioOutputDispatcherImpl(
    AudioManager* audio_manager,
    const AudioParameters& params,
    const std::string& output_device_id,
    const std::string& input_device_id,
    const base::TimeDelta& close_delay)
    : AudioOutputDispatcher(audio_manager,
                            params,
                            output_device_id,
                            input_device_id),
      idle_proxies_(0),
      close_timer_(FROM_HERE,
                   close_delay,
                   this,
                   &AudioOutputDispatcherImpl::CloseIdleStreams) {}

AudioOutputDispatcherImpl::~AudioOutputDispatcherImpl() {
  DCHECK_EQ(idle_proxies_, 0u);
  DCHECK(proxy_to_physical_map_.empty());
  DCHECK(idle_streams_.empty());
}

bool AudioOutputDispatcherImpl::OpenStream() {
  DCHECK(message_loop_->BelongsToCurrentThread());

  // Ensure that there is at least one open stream.
  if (idle_streams_.empty() && !CreateAndOpenStream())
    return false;

  ++idle_proxies_;
  close_timer_.Reset();
  return true;
}

bool AudioOutputDispatcherImpl::StartStream(
    AudioOutputStream::AudioSourceCallback* callback,
    AudioOutputProxy* stream_proxy) {
  DCHECK(message_loop_->BelongsToCurrentThread());

  if (idle_streams_.empty() && !CreateAndOpenStream())
    return false;

  AudioOutputStream* physical_stream = idle_streams_.back();
  idle_streams_.pop_back();

  DCHECK_GT(idle_proxies_, 0u);
  --idle_proxies_;

  double volume = 0;
  stream_proxy->GetVolume(&volume);
  physical_stream->SetVolume(volume);
  physical_stream->Start(callback);
  proxy_to_physical_map_[stream_proxy] = physical_stream;

  close_timer_.Reset();
  return true;
}

void AudioOutputDispatcherImpl::StopStream(AudioOutputProxy* stream_proxy) {
  DCHECK(message_loop_->BelongsToCurrentThread());

  AudioStreamMap::iterator it = proxy_to_physical_map_.find(stream_proxy);
  DCHECK(it != proxy_to_physical_map_.end());
  AudioOutputStream* physical_stream = it->second;
  proxy_to_physical_map_.erase(it);

  physical_stream->Stop();
  ++idle_proxies_;
  idle_streams_.push_back(physical_stream);

  close_timer_.Reset();
}

void AudioOutputDispatcherImpl::StreamVolumeSet(AudioOutputProxy* stream_proxy,
                                                double volume) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  AudioStreamMap::iterator it = proxy_to_physical_map_.find(stream_proxy);
  if (it != proxy_to_physical_map_.end()) {
    AudioOutputStream* physical_stream = it->second;
    physical_stream->SetVolume(volume);
  }
}

void AudioOutputDispatcherImpl::CloseStream(AudioOutputProxy* stream_proxy) {
  DCHECK(message_loop_->BelongsToCurrentThread());

  DCHECK_GT(idle_proxies_, 0u);
  --idle_proxies_;

  // Leave a single stream running until the close timer fires to help cycle
  // time when streams are opened and closed repeatedly.
  static const size_t kStreamKeepAliveCount = 1;
  if (idle_streams_.size() > kStreamKeepAliveCount) {
    for (size_t i = kStreamKeepAliveCount; i < idle_streams_.size(); ++i)
      idle_streams_[i]->Close();
    idle_streams_.erase(idle_streams_.begin() + kStreamKeepAliveCount,
                        idle_streams_.end());
  }
  close_timer_.Reset();
}

void AudioOutputDispatcherImpl::Shutdown() {
  DCHECK(message_loop_->BelongsToCurrentThread());

  // Close all idle streams immediately.  The |close_timer_| will handle
  // invalidating any outstanding tasks upon its destruction.
  CloseIdleStreams();
}

bool AudioOutputDispatcherImpl::CreateAndOpenStream() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  AudioOutputStream* stream = audio_manager_->MakeAudioOutputStream(
      params_, output_device_id_, input_device_id_);
  if (!stream)
    return false;

  if (!stream->Open()) {
    stream->Close();
    return false;
  }

  idle_streams_.push_back(stream);
  return true;
}

void AudioOutputDispatcherImpl::CloseIdleStreams() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  for (size_t i = 0; i < idle_streams_.size(); ++i)
    idle_streams_[i]->Close();
  idle_streams_.clear();
}

}  // namespace media
