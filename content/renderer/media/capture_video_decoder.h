// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_CAPTURE_VIDEO_DECODER_H_
#define CONTENT_RENDERER_MEDIA_CAPTURE_VIDEO_DECODER_H_

#include <deque>

#include "base/time.h"
#include "media/base/filters.h"
#include "media/base/video_frame.h"
#include "media/video/capture/video_capture.h"

namespace base {
class MessageLoopProxy;
}
class VideoCaptureImplManager;

// A filter takes raw frames from video capture engine and passes them to media
// engine as a video decoder filter.
class CaptureVideoDecoder
    : public media::VideoDecoder,
      public media::VideoCapture::EventHandler {
 public:
  CaptureVideoDecoder(
      base::MessageLoopProxy* message_loop_proxy,
      media::VideoCaptureSessionId video_stream_id,
      VideoCaptureImplManager* vc_manager,
      const media::VideoCapture::VideoCaptureCapability& capability);
  virtual ~CaptureVideoDecoder();

  // Filter implementation.
  virtual void Play(media::FilterCallback* callback);
  virtual void Seek(base::TimeDelta time, const media::FilterStatusCB& cb);
  virtual void Pause(media::FilterCallback* callback);
  virtual void Stop(media::FilterCallback* callback);

  // Decoder implementation.
  virtual void Initialize(media::DemuxerStream* demuxer_stream,
                          media::FilterCallback* filter_callback,
                          media::StatisticsCallback* stat_callback);
  virtual const media::MediaFormat& media_format();
  virtual void ProduceVideoFrame(scoped_refptr<media::VideoFrame> video_frame);
  virtual bool ProvidesBuffer();

  // VideoCapture::EventHandler implementation.
  virtual void OnStarted(media::VideoCapture* capture);
  virtual void OnStopped(media::VideoCapture* capture);
  virtual void OnPaused(media::VideoCapture* capture);
  virtual void OnError(media::VideoCapture* capture, int error_code);
  virtual void OnBufferReady(
      media::VideoCapture* capture,
      scoped_refptr<media::VideoCapture::VideoFrameBuffer> buf);
  virtual void OnDeviceInfoReceived(
      media::VideoCapture* capture,
      const media::VideoCaptureParams& device_info);

 private:
  friend class CaptureVideoDecoderTest;

  enum DecoderState {
    kUnInitialized,
    kNormal,
    kSeeking,
    kStopped,
    kPaused
  };

  void PlayOnDecoderThread(media::FilterCallback* callback);
  void SeekOnDecoderThread(base::TimeDelta time,
                           const media::FilterStatusCB& cb);
  void PauseOnDecoderThread(media::FilterCallback* callback);
  void StopOnDecoderThread(media::FilterCallback* callback);

  void InitializeOnDecoderThread(media::DemuxerStream* demuxer_stream,
                                 media::FilterCallback* filter_callback,
                                 media::StatisticsCallback* stat_callback);
  void ProduceVideoFrameOnDecoderThread(
      scoped_refptr<media::VideoFrame> video_frame);

  void OnStoppedOnDecoderThread(media::VideoCapture* capture);
  void OnBufferReadyOnDecoderThread(
      media::VideoCapture* capture,
      scoped_refptr<media::VideoCapture::VideoFrameBuffer> buf);

  scoped_refptr<base::MessageLoopProxy> message_loop_proxy_;
  scoped_refptr<VideoCaptureImplManager> vc_manager_;
  media::VideoCapture::VideoCaptureCapability capability_;
  DecoderState state_;
  media::MediaFormat media_format_;
  std::deque<scoped_refptr<media::VideoFrame> > available_frames_;
  media::FilterCallback* pending_stop_cb_;
  scoped_ptr<media::StatisticsCallback> statistics_callback_;

  media::VideoCaptureSessionId video_stream_id_;
  media::VideoCapture* capture_engine_;
  base::Time start_time_;

  DISALLOW_COPY_AND_ASSIGN(CaptureVideoDecoder);
};

#endif  // CONTENT_RENDERER_MEDIA_CAPTURE_VIDEO_DECODER_H_
