// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_RECTANGLE_UPDATE_DECODER_H_
#define REMOTING_CLIENT_RECTANGLE_UPDATE_DECODER_H_

#include <list>

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "remoting/base/decoder.h"
#include "remoting/client/frame_producer.h"

namespace base {
class MessageLoopProxy;
}  // namespace base

namespace pp {
class ImageData;
};

namespace remoting {

class FrameConsumer;
class VideoPacket;

namespace protocol {
class SessionConfig;
}  // namespace protocol

// TODO(ajwong): Re-examine this API, especially with regards to how error
// conditions on each step are reported.  Should they be CHECKs? Logs? Other?
// TODO(sergeyu): Rename this class.
class RectangleUpdateDecoder :
    public base::RefCountedThreadSafe<RectangleUpdateDecoder>,
    public FrameProducer {
 public:
  RectangleUpdateDecoder(base::MessageLoopProxy* message_loop,
                         FrameConsumer* consumer);

  // Initializes decoder with the infromation from the protocol config.
  void Initialize(const protocol::SessionConfig& config);

  // Decodes the contents of |packet|. DecodePacket may keep a reference to
  // |packet| so the |packet| must remain alive and valid until |done| is
  // executed.
  void DecodePacket(const VideoPacket* packet, const base::Closure& done);

  // FrameProducer implementation.
  virtual void DrawBuffer(pp::ImageData* buffer) OVERRIDE;
  virtual void InvalidateRegion(const SkRegion& region) OVERRIDE;
  virtual void RequestReturnBuffers(const base::Closure& done) OVERRIDE;
  virtual void SetOutputSizeAndClip(const SkISize& view_size,
                                    const SkIRect& clip_area) OVERRIDE;

 private:
  friend class base::RefCountedThreadSafe<RectangleUpdateDecoder>;

  virtual ~RectangleUpdateDecoder();

  // Paint the invalidated region to the next available buffer and return it
  // to the consumer.
  void DoPaint();

  scoped_refptr<base::MessageLoopProxy> message_loop_;
  FrameConsumer* consumer_;

  scoped_ptr<Decoder> decoder_;

  // Remote screen size in pixels.
  SkISize source_size_;

  // The current dimentions of the frame consumer view.
  SkISize view_size_;
  SkIRect clip_area_;

  // The drawing buffers supplied by the frame consumer.
  std::list<pp::ImageData*> buffers_;
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_RECTANGLE_UPDATE_DECODER_H_
