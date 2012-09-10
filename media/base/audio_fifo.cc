// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/audio_fifo.h"

#include "base/logging.h"

namespace media {

// Given current position in the FIFO, the maximum number of elements in the
// FIFO and the size of the input; this method provides two output results:
// |size| and |wrap_size|. These two results can then be utilized for memcopy
// operations to and from the FIFO.
// Under "normal" circumstances, |size| will be equal to |in_size| and
// |wrap_size| will be zero. This case corresponding to the non-wrapping case
// where we have not yet reached the "edge" of the FIFO. If |pos| + |in_size|
// exceeds the total size of the FIFO, we must wrap around and start reusing
// a part the allocated memory. The size of this part is given by |wrap_size|.
static void GetSizes(
    int pos, int max_size, int in_size, int* size, int* wrap_size) {
  if (pos + in_size > max_size) {
    // Wrapping is required => derive size of each segment.
    *size = max_size - pos;
    *wrap_size = in_size - *size;
  } else {
    // Wrapping is not required.
    *size = in_size;
    *wrap_size = 0;
  }
}

// Updates the read/write position with |step| modulo the maximum number of
// elements in the FIFO to ensure that the position counters wraps around at
// the endpoint.
static int UpdatePos(int pos, int step, int max_size) {
  return ((pos + step) % max_size);
}

AudioFifo::AudioFifo(int channels, int frames)
    : audio_bus_(AudioBus::Create(channels, frames)),
      max_frames_in_fifo_(frames),
      frames_in_fifo_(0),
      read_pos_(0),
      write_pos_(0) {}

AudioFifo::~AudioFifo() {}

bool AudioFifo::Push(const AudioBus* source) {
  DCHECK(source);
  DCHECK_EQ(source->channels(), audio_bus_->channels());

  // Ensure that there is space for the new data in the FIFO.
  const int source_size = source->frames();
  if (frames_in_fifo_ + source_size > max_frames()) {
    DLOG(ERROR) << "FIFO overflow.";
    return false;
  }

  // Figure out if wrapping is needed and if so what segment sizes we need
  // when adding the new audio bus content to the FIFO.
  int append_size = 0;
  int wrap_size = 0;
  GetSizes(write_pos_, max_frames(), source_size, &append_size, &wrap_size);

  // Copy all channels from the source to the FIFO. Wrap around if needed.
  for (int ch = 0; ch < source->channels(); ++ch) {
    float* dest = audio_bus_->channel(ch);
    const float* src = source->channel(ch);

    // Append part of (or the complete) source to the FIFO.
    memcpy(&dest[write_pos_], &src[0], append_size * sizeof(src[0]));
    if (wrap_size > 0) {
      // Wrapping is needed: copy remaining part from the source to the FIFO.
      memcpy(&dest[0], &src[append_size], wrap_size * sizeof(src[0]));
    }
  }

  frames_in_fifo_ += source_size;
  DCHECK_LE(frames_in_fifo_, max_frames());
  write_pos_ = UpdatePos(write_pos_, source_size, max_frames());
  return true;
}

bool AudioFifo::Consume(AudioBus* destination, int frames_to_consume) {
  DCHECK(destination);
  DCHECK_EQ(destination->channels(), audio_bus_->channels());

  // It is not possible to ask for more data than what is available in the FIFO.
  if (frames_to_consume > frames_in_fifo_) {
    DLOG(ERROR) << "FIFO underrun.";
    return false;
  }

  // A copy from the FIFO to |destination| will only be performed if the
  // allocated memory in |destination| is sufficient.
  if (frames_to_consume > destination->frames()) {
    DLOG(ERROR) << "Insufficient space in destination.";
    return false;
  }

  // Figure out if wrapping is needed and if so what segment sizes we need
  // when removing audio bus content from the FIFO.
  int consume_size = 0;
  int wrap_size = 0;
  GetSizes(read_pos_, max_frames(), frames_to_consume,
           &consume_size, &wrap_size);

  // For all channels, remove the requested amount of data from the FIFO
  // and copy the content to the destination. Wrap around if needed.
  for (int ch = 0; ch < destination->channels(); ++ch) {
    float* dest = destination->channel(ch);
    const float* src = audio_bus_->channel(ch);

    // Copy a selected part of the FIFO to the destination.
    memcpy(&dest[0], &src[read_pos_], consume_size * sizeof(src[0]));
    if (wrap_size > 0) {
      // Wrapping is needed: copy remaining part to the destination.
      memcpy(&dest[consume_size], &src[0], wrap_size * sizeof(src[0]));
    }
  }

  frames_in_fifo_ -= frames_to_consume;
  read_pos_ = UpdatePos(read_pos_, frames_to_consume, max_frames());
  return true;
}

void AudioFifo::Clear() {
  frames_in_fifo_ = 0;
  read_pos_ = 0;
  write_pos_ = 0;
}

}  // namespace media
