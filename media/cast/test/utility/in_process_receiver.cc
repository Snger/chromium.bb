// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/test/utility/in_process_receiver.h"

#include "base/bind_helpers.h"
#include "base/time/time.h"
#include "media/base/video_frame.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_environment.h"
#include "media/cast/cast_receiver.h"
#include "media/cast/transport/cast_transport_config.h"
#include "media/cast/transport/transport/udp_transport.h"

using media::cast::transport::CastTransportStatus;
using media::cast::transport::UdpTransport;

namespace media {
namespace cast {

InProcessReceiver::InProcessReceiver(
    const scoped_refptr<CastEnvironment>& cast_environment,
    const net::IPEndPoint& local_end_point,
    const net::IPEndPoint& remote_end_point,
    const AudioReceiverConfig& audio_config,
    const VideoReceiverConfig& video_config)
    : cast_environment_(cast_environment),
      local_end_point_(local_end_point),
      remote_end_point_(remote_end_point),
      audio_config_(audio_config),
      video_config_(video_config),
      weak_factory_(this) {}

InProcessReceiver::~InProcessReceiver() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
}

void InProcessReceiver::Start() {
  cast_environment_->PostTask(CastEnvironment::MAIN,
                              FROM_HERE,
                              base::Bind(&InProcessReceiver::StartOnMainThread,
                                         base::Unretained(this)));
}

void InProcessReceiver::DestroySoon() {
  cast_environment_->PostTask(
      CastEnvironment::MAIN,
      FROM_HERE,
      base::Bind(&InProcessReceiver::WillDestroyReceiver, base::Owned(this)));
}

void InProcessReceiver::UpdateCastTransportStatus(CastTransportStatus status) {
  LOG_IF(ERROR, status == media::cast::transport::TRANSPORT_SOCKET_ERROR)
      << "Transport socket error occurred.  InProcessReceiver is likely dead.";
  VLOG(1) << "CastTransportStatus is now " << status;
}

void InProcessReceiver::StartOnMainThread() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));

  DCHECK(!transport_ && !cast_receiver_);
  transport_.reset(
      new UdpTransport(NULL,
                       cast_environment_->GetTaskRunner(CastEnvironment::MAIN),
                       local_end_point_,
                       remote_end_point_,
                       base::Bind(&InProcessReceiver::UpdateCastTransportStatus,
                                  base::Unretained(this))));
  cast_receiver_ = CastReceiver::Create(
      cast_environment_, audio_config_, video_config_, transport_.get());

  // TODO(hubbe): Make the cast receiver do this automatically.
  transport_->StartReceiving(cast_receiver_->packet_receiver());

  PullNextAudioFrame();
  PullNextVideoFrame();
}

void InProcessReceiver::GotAudioFrame(scoped_ptr<AudioBus> audio_frame,
                                      const base::TimeTicks& playout_time,
                                      bool is_continuous) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  if (audio_frame.get()) {
    // TODO(miu): Remove use of deprecated PcmAudioFrame and also pass
    // |is_continuous| flag.
    scoped_ptr<PcmAudioFrame> pcm_frame(new PcmAudioFrame());
    pcm_frame->channels = audio_frame->channels();
    pcm_frame->frequency = audio_config_.frequency;
    pcm_frame->samples.resize(audio_frame->channels() * audio_frame->frames());
    audio_frame->ToInterleaved(
        audio_frame->frames(), sizeof(int16), &pcm_frame->samples.front());
    OnAudioFrame(pcm_frame.Pass(), playout_time);
  }
  PullNextAudioFrame();
}

void InProcessReceiver::GotVideoFrame(
    const scoped_refptr<VideoFrame>& video_frame,
    const base::TimeTicks& render_time) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  OnVideoFrame(video_frame, render_time);
  PullNextVideoFrame();
}

void InProcessReceiver::PullNextAudioFrame() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  cast_receiver_->frame_receiver()->GetRawAudioFrame(
      base::Bind(&InProcessReceiver::GotAudioFrame,
                 weak_factory_.GetWeakPtr()));
}

void InProcessReceiver::PullNextVideoFrame() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  cast_receiver_->frame_receiver()->GetRawVideoFrame(base::Bind(
      &InProcessReceiver::GotVideoFrame, weak_factory_.GetWeakPtr()));
}

// static
void InProcessReceiver::WillDestroyReceiver(InProcessReceiver* receiver) {
  DCHECK(receiver->cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
}

}  // namespace cast
}  // namespace media
