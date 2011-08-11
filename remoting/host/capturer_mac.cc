// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/capturer.h"

#include <ApplicationServices/ApplicationServices.h>
#include <OpenGL/CGLMacro.h>
#include <OpenGL/OpenGL.h>

#include <stddef.h>

#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/memory/scoped_ptr.h"
#include "remoting/base/util.h"
#include "remoting/host/capturer_helper.h"
#include "skia/ext/skia_utils_mac.h"

namespace remoting {

namespace {

class scoped_pixel_buffer_object {
 public:
  scoped_pixel_buffer_object();
  ~scoped_pixel_buffer_object();

  bool Init(CGLContextObj cgl_context, int size_in_bytes);
  void Release();

  GLuint get() const { return pixel_buffer_object_; }

 private:
  CGLContextObj cgl_context_;
  GLuint pixel_buffer_object_;

  DISALLOW_COPY_AND_ASSIGN(scoped_pixel_buffer_object);
};

scoped_pixel_buffer_object::scoped_pixel_buffer_object()
    : cgl_context_(NULL),
      pixel_buffer_object_(0) {
}

scoped_pixel_buffer_object::~scoped_pixel_buffer_object() {
  Release();
}

bool scoped_pixel_buffer_object::Init(CGLContextObj cgl_context,
                                      int size_in_bytes) {
  // The PBO path is only done on 10.6 (SnowLeopard) and above due to
  // a driver issue that was found on 10.5
  // (specifically on a NVIDIA GeForce 7300 GT).
  // http://crbug.com/87283
  if (base::mac::IsOSLeopardOrEarlier()) {
    return false;
  }
  cgl_context_ = cgl_context;
  CGLContextObj CGL_MACRO_CONTEXT = cgl_context_;
  glGenBuffersARB(1, &pixel_buffer_object_);
  if (glGetError() == GL_NO_ERROR) {
    glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, pixel_buffer_object_);
    glBufferDataARB(GL_PIXEL_PACK_BUFFER_ARB, size_in_bytes, NULL,
                    GL_STREAM_READ_ARB);
    glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, 0);
    if (glGetError() != GL_NO_ERROR) {
      Release();
    }
  } else {
    cgl_context_ = NULL;
    pixel_buffer_object_ = 0;
  }
  return pixel_buffer_object_ != 0;
}

void scoped_pixel_buffer_object::Release() {
  if (pixel_buffer_object_) {
    CGLContextObj CGL_MACRO_CONTEXT = cgl_context_;
    glDeleteBuffersARB(1, &pixel_buffer_object_);
    cgl_context_ = NULL;
    pixel_buffer_object_ = 0;
  }
}

// A class representing a full-frame pixel buffer.
class VideoFrameBuffer {
 public:
  VideoFrameBuffer() : bytes_per_row_(0), needs_update_(true) { }

  // If the buffer is marked as needing to be updated (for example after the
  // screen mode changes) and is the wrong size, then release the old buffer
  // and create a new one.
  void Update() {
    if (needs_update_) {
      needs_update_ = false;
      CGDirectDisplayID mainDevice = CGMainDisplayID();
      int width = CGDisplayPixelsWide(mainDevice);
      int height = CGDisplayPixelsHigh(mainDevice);
      if (width != size_.width() || height != size_.height()) {
        size_.SetSize(width, height);
        bytes_per_row_ = width * sizeof(uint32_t);
        size_t buffer_size = width * height * sizeof(uint32_t);
        ptr_.reset(new uint8[buffer_size]);
      }
    }
  }

  gfx::Size size() const { return size_; }
  int bytes_per_row() const { return bytes_per_row_; }
  uint8* ptr() const { return ptr_.get(); }

  void set_needs_update() { needs_update_ = true; }

 private:
  gfx::Size size_;
  int bytes_per_row_;
  scoped_array<uint8> ptr_;
  bool needs_update_;

  DISALLOW_COPY_AND_ASSIGN(VideoFrameBuffer);
};

// A class to perform capturing for mac.
class CapturerMac : public Capturer {
 public:
  CapturerMac();
  virtual ~CapturerMac();

  // Enable or disable capturing. Capturing should be disabled while a screen
  // reconfiguration is in progress, otherwise reading from the screen base
  // address is likely to segfault.
  void EnableCapture(bool enable);

  // Capturer interface.
  virtual void ScreenConfigurationChanged() OVERRIDE;
  virtual media::VideoFrame::Format pixel_format() const OVERRIDE;
  virtual void ClearInvalidRegion() OVERRIDE;
  virtual void InvalidateRegion(const SkRegion& invalid_region) OVERRIDE;
  virtual void InvalidateScreen(const gfx::Size& size) OVERRIDE;
  virtual void InvalidateFullScreen() OVERRIDE;
  virtual void CaptureInvalidRegion(CaptureCompletedCallback* callback)
      OVERRIDE;
  virtual const gfx::Size& size_most_recent() const OVERRIDE;

 private:
  void GlBlitFast(const VideoFrameBuffer& buffer, const SkRegion& region);
  void GlBlitSlow(const VideoFrameBuffer& buffer);
  void CgBlit(const VideoFrameBuffer& buffer, const SkRegion& region);
  void CaptureRegion(const SkRegion& region,
                     CaptureCompletedCallback* callback);

  void ScreenRefresh(CGRectCount count, const CGRect *rect_array);
  void ScreenUpdateMove(CGScreenUpdateMoveDelta delta,
                                size_t count,
                                const CGRect *rect_array);
  static void ScreenRefreshCallback(CGRectCount count,
                                    const CGRect *rect_array,
                                    void *user_parameter);
  static void ScreenUpdateMoveCallback(CGScreenUpdateMoveDelta delta,
                                       size_t count,
                                       const CGRect *rect_array,
                                       void *user_parameter);
  static void DisplaysReconfiguredCallback(CGDirectDisplayID display,
                                           CGDisplayChangeSummaryFlags flags,
                                           void *user_parameter);

  void ReleaseBuffers();
  CGLContextObj cgl_context_;
  static const int kNumBuffers = 2;
  scoped_pixel_buffer_object pixel_buffer_object_;
  VideoFrameBuffer buffers_[kNumBuffers];

  // A thread-safe list of invalid rectangles, and the size of the most
  // recently captured screen.
  CapturerHelper helper_;

  // The current buffer with valid data for reading.
  int current_buffer_;

  // The previous buffer into which we captured, or NULL for the first capture
  // for a particular screen resolution.
  uint8* last_buffer_;

  // Contains an invalid region from the previous capture.
  SkRegion last_invalid_region_;

  // Format of pixels returned in buffer.
  media::VideoFrame::Format pixel_format_;

  bool capturing_;

  DISALLOW_COPY_AND_ASSIGN(CapturerMac);
};

CapturerMac::CapturerMac()
    : cgl_context_(NULL),
      current_buffer_(0),
      last_buffer_(NULL),
      pixel_format_(media::VideoFrame::RGB32),
      capturing_(true) {
  // TODO(dmaclach): move this initialization out into session_manager,
  // or at least have session_manager call into here to initialize it.
  CGError err =
      CGRegisterScreenRefreshCallback(CapturerMac::ScreenRefreshCallback,
                                      this);
  DCHECK_EQ(err, kCGErrorSuccess);
  err = CGScreenRegisterMoveCallback(CapturerMac::ScreenUpdateMoveCallback,
                                     this);
  DCHECK_EQ(err, kCGErrorSuccess);
  err = CGDisplayRegisterReconfigurationCallback(
      CapturerMac::DisplaysReconfiguredCallback, this);
  DCHECK_EQ(err, kCGErrorSuccess);
  ScreenConfigurationChanged();
}

CapturerMac::~CapturerMac() {
  ReleaseBuffers();
  CGUnregisterScreenRefreshCallback(CapturerMac::ScreenRefreshCallback, this);
  CGScreenUnregisterMoveCallback(CapturerMac::ScreenUpdateMoveCallback, this);
  CGDisplayRemoveReconfigurationCallback(
      CapturerMac::DisplaysReconfiguredCallback, this);
}

void CapturerMac::EnableCapture(bool enable) {
  capturing_ = enable;
}

void CapturerMac::ReleaseBuffers() {
  if (cgl_context_) {
    pixel_buffer_object_.Release();
    CGLDestroyContext(cgl_context_);
    cgl_context_ = NULL;
  }
  // The buffers might be in use by the encoder, so don't delete them here.
  // Instead, mark them as "needs update"; next time the buffers are used by
  // the capturer, they will be recreated if necessary.
  for (int i = 0; i < kNumBuffers; ++i) {
    buffers_[i].set_needs_update();
  }
}

void CapturerMac::ScreenConfigurationChanged() {
  ReleaseBuffers();
  helper_.ClearInvalidRegion();
  last_buffer_ = NULL;

  CGDirectDisplayID mainDevice = CGMainDisplayID();
  int width = CGDisplayPixelsWide(mainDevice);
  int height = CGDisplayPixelsHigh(mainDevice);
  InvalidateScreen(gfx::Size(width, height));

  if (!CGDisplayUsesOpenGLAcceleration(mainDevice)) {
    VLOG(3) << "OpenGL support not available.";
    return;
  }

  CGLPixelFormatAttribute attributes[] = {
    kCGLPFAFullScreen,
    kCGLPFADisplayMask,
    (CGLPixelFormatAttribute)CGDisplayIDToOpenGLDisplayMask(mainDevice),
    (CGLPixelFormatAttribute)0
  };
  CGLPixelFormatObj pixel_format = NULL;
  GLint matching_pixel_format_count = 0;
  CGLError err = CGLChoosePixelFormat(attributes,
                                      &pixel_format,
                                      &matching_pixel_format_count);
  DCHECK_EQ(err, kCGLNoError);
  err = CGLCreateContext(pixel_format, NULL, &cgl_context_);
  DCHECK_EQ(err, kCGLNoError);
  CGLDestroyPixelFormat(pixel_format);
  CGLSetFullScreen(cgl_context_);
  CGLSetCurrentContext(cgl_context_);

  size_t buffer_size = width * height * sizeof(uint32_t);
  pixel_buffer_object_.Init(cgl_context_, buffer_size);
}

media::VideoFrame::Format CapturerMac::pixel_format() const {
  return pixel_format_;
}

void CapturerMac::ClearInvalidRegion() {
  helper_.ClearInvalidRegion();
}

void CapturerMac::InvalidateRegion(const SkRegion& invalid_region) {
  helper_.InvalidateRegion(invalid_region);
}

void CapturerMac::InvalidateScreen(const gfx::Size& size) {
  helper_.InvalidateScreen(size);
}

void CapturerMac::InvalidateFullScreen() {
  helper_.InvalidateFullScreen();
}

void CapturerMac::CaptureInvalidRegion(CaptureCompletedCallback* callback) {
  scoped_refptr<CaptureData> data;
  if (capturing_) {
    SkRegion region;
    helper_.SwapInvalidRegion(&region);
    VideoFrameBuffer& current_buffer = buffers_[current_buffer_];
    current_buffer.Update();

    bool flip = true;  // GL capturers need flipping.
    if (cgl_context_) {
      if (pixel_buffer_object_.get() != 0) {
        GlBlitFast(current_buffer, region);
      } else {
        // See comment in scoped_pixel_buffer_object::Init about why the slow
        // path is always used on 10.5.
        GlBlitSlow(current_buffer);
      }
    } else {
      CgBlit(current_buffer, region);
      flip = false;
    }

    DataPlanes planes;
    planes.data[0] = current_buffer.ptr();
    planes.strides[0] = current_buffer.bytes_per_row();
    if (flip) {
      planes.strides[0] = -planes.strides[0];
      planes.data[0] +=
          (current_buffer.size().height() - 1) * current_buffer.bytes_per_row();
    }

    data = new CaptureData(planes, gfx::Size(current_buffer.size()),
                           pixel_format());
    data->mutable_dirty_region() = region;

    current_buffer_ = (current_buffer_ + 1) % kNumBuffers;
    helper_.set_size_most_recent(data->size());
  }

  callback->Run(data);
  delete callback;
}

void CapturerMac::GlBlitFast(const VideoFrameBuffer& buffer,
           const SkRegion& region) {
  if (last_buffer_) {
    // We are doing double buffer for the capture data so we just need to copy
    // the invalid region from the previous capture in the current buffer.
    // TODO(hclam): We can reduce the amount of copying here by subtracting
    // |capturer_helper_|s region from |last_invalid_region_|.
    // http://crbug.com/92354

    // Since the image obtained from OpenGL is upside-down, need to do some
    // magic here to copy the correct rectangle.
    const int y_offset = (buffer.size().height() - 1) * buffer.bytes_per_row();
    for(SkRegion::Iterator i(last_invalid_region_); !i.done(); i.next()) {
      CopyRect(last_buffer_ + y_offset,
               -buffer.bytes_per_row(),
               buffer.ptr() + y_offset,
               -buffer.bytes_per_row(),
               4,  // Bytes for pixel for RGBA.
               i.rect());
    }
  }
  last_buffer_ = buffer.ptr();
  last_invalid_region_ = region;

  CGLContextObj CGL_MACRO_CONTEXT = cgl_context_;
  glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, pixel_buffer_object_.get());
  glReadPixels(0, 0, buffer.size().width(), buffer.size().height(),
               GL_BGRA, GL_UNSIGNED_BYTE, 0);
  GLubyte* ptr = static_cast<GLubyte*>(
      glMapBufferARB(GL_PIXEL_PACK_BUFFER_ARB, GL_READ_ONLY_ARB));
  if (ptr == NULL) {
    // If the buffer can't be mapped, assume that it's no longer valid and
    // release it.
    pixel_buffer_object_.Release();
  } else {
    // Copy only from the dirty rects. Since the image obtained from OpenGL is
    // upside-down we need to do some magic here to copy the correct rectangle.
    const int y_offset = (buffer.size().height() - 1) * buffer.bytes_per_row();
    for(SkRegion::Iterator i(region); !i.done(); i.next()) {
      CopyRect(ptr + y_offset,
         -buffer.bytes_per_row(),
         buffer.ptr() + y_offset,
         -buffer.bytes_per_row(),
         4,  // Bytes for pixel for RGBA.
         i.rect());
    }
  }
  if (!glUnmapBufferARB(GL_PIXEL_PACK_BUFFER_ARB)) {
    // If glUnmapBuffer returns false, then the contents of the data store are
    // undefined. This might be because the screen mode has changed, in which
    // case it will be recreated in ScreenConfigurationChanged, but releasing
    // the object here is the best option. Capturing will fall back on
    // GlBlitSlow until such time as the pixel buffer object is recreated.
    pixel_buffer_object_.Release();
  }
  glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, 0);
}

void CapturerMac::GlBlitSlow(const VideoFrameBuffer& buffer) {
  CGLContextObj CGL_MACRO_CONTEXT = cgl_context_;
  glReadBuffer(GL_FRONT);
  glPushClientAttrib(GL_CLIENT_PIXEL_STORE_BIT);
  glPixelStorei(GL_PACK_ALIGNMENT, 4);  // Force 4-byte alignment.
  glPixelStorei(GL_PACK_ROW_LENGTH, 0);
  glPixelStorei(GL_PACK_SKIP_ROWS, 0);
  glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
  // Read a block of pixels from the frame buffer.
  glReadPixels(0, 0, buffer.size().width(), buffer.size().height(),
               GL_BGRA, GL_UNSIGNED_BYTE, buffer.ptr());
  glPopClientAttrib();
}

void CapturerMac::CgBlit(const VideoFrameBuffer& buffer,
                         const SkRegion& region) {
  if (last_buffer_)
    memcpy(buffer.ptr(), last_buffer_,
           buffer.bytes_per_row() * buffer.size().height());
  last_buffer_ = buffer.ptr();
  CGDirectDisplayID main_display = CGMainDisplayID();
  uint8* display_base_address =
      reinterpret_cast<uint8*>(CGDisplayBaseAddress(main_display));
  int src_bytes_per_row = CGDisplayBytesPerRow(main_display);
  int src_bytes_per_pixel = CGDisplayBitsPerPixel(main_display) / 8;
  // TODO(hclam): We can reduce the amount of copying here by subtracting
  // |capturer_helper_|s region from |last_invalid_region_|.
  // http://crbug.com/92354
  for(SkRegion::Iterator i(region); !i.done(); i.next()) {
    CopyRect(display_base_address,
             src_bytes_per_row,
             buffer.ptr(),
             buffer.bytes_per_row(),
             src_bytes_per_pixel,
             i.rect());
  }
}

const gfx::Size& CapturerMac::size_most_recent() const {
  return helper_.size_most_recent();
}

void CapturerMac::ScreenRefresh(CGRectCount count, const CGRect *rect_array) {
  SkIRect skirect_array[count];
  for (CGRectCount i = 0; i < count; ++i) {
    skirect_array[i] = gfx::CGRectToSkIRect(rect_array[i]);
  }
  SkRegion region;
  region.setRects(skirect_array, count);
  InvalidateRegion(region);
}

void CapturerMac::ScreenUpdateMove(CGScreenUpdateMoveDelta delta,
                                   size_t count,
                                   const CGRect *rect_array) {
  SkIRect skirect_new_array[count];
  for (CGRectCount i = 0; i < count; ++i) {
    CGRect rect = rect_array[i];
    rect = CGRectOffset(rect, delta.dX, delta.dY);
    skirect_new_array[i] = gfx::CGRectToSkIRect(rect);
  }
  SkRegion region;
  region.setRects(skirect_new_array, count);
  InvalidateRegion(region);
}

void CapturerMac::ScreenRefreshCallback(CGRectCount count,
                                        const CGRect *rect_array,
                                        void *user_parameter) {
  CapturerMac *capturer = reinterpret_cast<CapturerMac *>(user_parameter);
  capturer->ScreenRefresh(count, rect_array);
}

void CapturerMac::ScreenUpdateMoveCallback(CGScreenUpdateMoveDelta delta,
                                           size_t count,
                                           const CGRect *rect_array,
                                           void *user_parameter) {
  CapturerMac *capturer = reinterpret_cast<CapturerMac *>(user_parameter);
  capturer->ScreenUpdateMove(delta, count, rect_array);
}

void CapturerMac::DisplaysReconfiguredCallback(
    CGDirectDisplayID display,
    CGDisplayChangeSummaryFlags flags,
    void *user_parameter) {
  if (display == CGMainDisplayID()) {
    CapturerMac *capturer = reinterpret_cast<CapturerMac *>(user_parameter);
    if (flags & kCGDisplayBeginConfigurationFlag) {
      capturer->EnableCapture(false);
    } else {
      capturer->EnableCapture(true);
      capturer->ScreenConfigurationChanged();
    }
  }
}

}  // namespace

// static
Capturer* Capturer::Create() {
  return new CapturerMac();
}

}  // namespace remoting
