// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_CHUNK_DEMUXER_H_
#define MEDIA_FILTERS_CHUNK_DEMUXER_H_

#include <list>
#include <string>
#include <utility>
#include <vector>

#include "base/synchronization/lock.h"
#include "media/base/byte_queue.h"
#include "media/base/demuxer.h"
#include "media/filters/source_buffer.h"

namespace media {

class ChunkDemuxerClient;
class ChunkDemuxerStream;
class FFmpegURLProtocol;

// Demuxer implementation that allows chunks of media data to be passed
// from JavaScript to the media stack.
class MEDIA_EXPORT ChunkDemuxer : public Demuxer {
 public:
  enum Status {
    kOk,              // ID added w/o error.
    kNotSupported,    // Type specified is not supported.
    kReachedIdLimit,  // Reached ID limit. We can't handle any more IDs.
  };

  typedef std::vector<std::pair<base::TimeDelta, base::TimeDelta> > Ranges;

  explicit ChunkDemuxer(ChunkDemuxerClient* client);

  // Demuxer implementation.
  virtual void Initialize(DemuxerHost* host,
                          const PipelineStatusCB& cb) OVERRIDE;
  virtual void Stop(const base::Closure& callback) OVERRIDE;
  virtual void Seek(base::TimeDelta time, const PipelineStatusCB&  cb) OVERRIDE;
  virtual void OnAudioRendererDisabled() OVERRIDE;
  virtual scoped_refptr<DemuxerStream> GetStream(
      DemuxerStream::Type type) OVERRIDE;
  virtual base::TimeDelta GetStartTime() const OVERRIDE;
  virtual int GetBitrate() OVERRIDE;

  // Methods used by an external object to control this demuxer.
  void StartWaitingForSeek();

  // Registers a new |id| to use for AppendData() calls. |type| indicates
  // the MIME type for the data that we intend to append for this ID.
  // kOk is returned if the demuxer has enough resources to support another ID
  //    and supports the format indicated by |type|.
  // kNotSupported is returned if |type| is not a supported format.
  // kReachedIdLimit is returned if the demuxer cannot handle another ID right
  //    now.
  Status AddId(const std::string& id, const std::string& type,
               std::vector<std::string>& codecs);

  // Removed an ID & associated resources that were previously added with
  // AddId().
  void RemoveId(const std::string& id);

  // Gets the currently buffered ranges for the specified ID.
  // Returns true if data is buffered & |ranges_out| is set to the
  // time ranges currently buffered.
  // Returns false if no data is buffered.
  bool GetBufferedRanges(const std::string& id, Ranges* ranges_out) const;

  // Appends media data to the source buffer associated with |id|. Returns
  // false if this method is called in an invalid state.
  bool AppendData(const std::string& id, const uint8* data, size_t length);

  // Aborts parsing the current segment and reset the parser to a state where
  // it can accept a new segment.
  void Abort(const std::string& id);

  // Accesses the configs associated with the current playback buffers.
  const AudioDecoderConfig& GetCurrentAudioDecoderConfig();
  const VideoDecoderConfig& GetCurrentVideoDecoderConfig();

  // Fills |out_buffer| with a new buffer from the current SourceBufferStream
  // indicated by |type|.
  // Returns true if |out_buffer| is filled with a valid buffer.
  // Returns false if SourceBuffer can not fulfill the request or if |type| is
  // not AUDIO or VIDEO.
  // Two versions needed (Locked and not Locked) because this method is called
  // during a Read() when ChunkDemuxer IS NOT locked and from
  // ChunkDemuxerStream::CreateReadDoneClosures_Locked() when
  // OnBuffersAvailable() is called and ChunkDemuxer IS locked.
  // TODO(acolwell): Investigate a cleaner solution to SourceBufferRead locking
  // requirements. crbug.com/129849
  bool SourceBufferRead_Locked(DemuxerStream::Type type,
                               scoped_refptr<StreamParserBuffer>* out_buffer);
  bool SourceBufferRead(DemuxerStream::Type type,
                        scoped_refptr<StreamParserBuffer>* out_buffer);

  // Signals an EndOfStream request.
  // Returns false if called in an unexpected state or if there is a gap between
  // the current position and the end of the buffered data.
  bool EndOfStream(PipelineStatus status);
  void Shutdown();

 protected:
  virtual ~ChunkDemuxer();

 private:
  enum State {
    WAITING_FOR_INIT,
    INITIALIZING,
    INITIALIZED,
    ENDED,
    PARSE_ERROR,
    SHUTDOWN,
  };

  void ChangeState_Locked(State new_state);

  // Reports an error and puts the demuxer in a state where it won't accept more
  // data.
  void ReportError_Locked(PipelineStatus error);

  // SourceBuffer callbacks.
  void OnSourceBufferInitDone(bool success, base::TimeDelta duration);
  bool OnNewConfigs(const AudioDecoderConfig& audio_config,
                    const VideoDecoderConfig& video_config);
  bool OnAudioBuffers();
  bool OnVideoBuffers();
  bool OnKeyNeeded(scoped_array<uint8> init_data, int init_data_size);

  mutable base::Lock lock_;
  State state_;

  DemuxerHost* host_;
  ChunkDemuxerClient* client_;
  PipelineStatusCB init_cb_;
  PipelineStatusCB seek_cb_;

  scoped_refptr<ChunkDemuxerStream> audio_;
  scoped_refptr<ChunkDemuxerStream> video_;

  int64 buffered_bytes_;

  base::TimeDelta duration_;

  scoped_ptr<SourceBuffer> source_buffer_;

  // TODO(acolwell): Remove this when fixing http://crbug.com/122909
  std::string source_id_;

  DISALLOW_COPY_AND_ASSIGN(ChunkDemuxer);
};

}  // namespace media

#endif  // MEDIA_FILTERS_CHUNK_DEMUXER_H_
