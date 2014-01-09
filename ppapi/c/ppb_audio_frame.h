/* Copyright 2014 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From ppb_audio_frame.idl modified Thu Jan  9 14:39:24 2014. */

#ifndef PPAPI_C_PPB_AUDIO_FRAME_H_
#define PPAPI_C_PPB_AUDIO_FRAME_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_time.h"

#define PPB_AUDIOFRAME_INTERFACE_0_1 "PPB_AudioFrame;0.1" /* dev */
/**
 * @file
 * Defines the <code>PPB_AudioFrame</code> interface.
 */


/**
 * @addtogroup Interfaces
 * @{
 */
struct PPB_AudioFrame_0_1 { /* dev */
  /**
   * Determines if a resource is an AudioFrame resource.
   *
   * @param[in] resource The <code>PP_Resource</code> to test.
   *
   * @return A <code>PP_Bool</code> with <code>PP_TRUE</code> if the given
   * resource is an AudioFrame resource or <code>PP_FALSE</code> otherwise.
   */
  PP_Bool (*IsAudioFrame)(PP_Resource resource);
  /**
   * Gets the timestamp of the audio frame.
   *
   * @param[in] frame A <code>PP_Resource</code> corresponding to an audio frame
   * resource.
   *
   * @return A <code>PP_TimeDelta</code> containing the timestamp of the audio
   * frame. Given in seconds since the start of the containing audio stream.
   */
  PP_TimeDelta (*GetTimestamp)(PP_Resource frame);
  /**
   * Sets the timestamp of the audio frame.
   *
   * @param[in] frame A <code>PP_Resource</code> corresponding to an audio frame
   * resource.
   * @param[in] timestamp A <code>PP_TimeDelta</code> containing the timestamp
   * of the audio frame. Given in seconds since the start of the containing
   * audio stream.
   */
  void (*SetTimestamp)(PP_Resource frame, PP_TimeDelta timestamp);
  /**
   * Gets the sample size of the audio frame.
   *
   * @param[in] frame A <code>PP_Resource</code> corresponding to an audio frame
   * resource.
   *
   * @return The sample size of the audio frame.
   */
  uint32_t (*GetSampleSize)(PP_Resource frame);
  /**
   * Gets the number of channels in the audio frame.
   *
   * @param[in] frame A <code>PP_Resource</code> corresponding to an audio frame
   * resource.
   *
   * @return The number of channels in the audio frame.
   */
  uint32_t (*GetNumberOfChannels)(PP_Resource frame);
  /**
   * Gets the number of samples in the audio frame.
   *
   * @param[in] frame A <code>PP_Resource</code> corresponding to an audio frame
   * resource.
   *
   * @return The number of samples in the audio frame.
   * For example, at a sampling rate of 44,100 Hz in stereo audio, a frame
   * containing 4410 * 2 samples would have a duration of 100 milliseconds.
   */
  uint32_t (*GetNumberOfSamples)(PP_Resource frame);
  /**
   * Gets the data buffer containing the audio frame samples.
   *
   * @param[in] frame A <code>PP_Resource</code> corresponding to an audio frame
   * resource.
   *
   * @return A pointer to the beginning of the data buffer.
   */
  void* (*GetDataBuffer)(PP_Resource frame);
  /**
   * Gets the size of the data buffer in bytes.
   *
   * @param[in] frame A <code>PP_Resource</code> corresponding to an audio frame
   * resource.
   *
   * @return The size of the data buffer in bytes.
   */
  uint32_t (*GetDataBufferSize)(PP_Resource frame);
};
/**
 * @}
 */

#endif  /* PPAPI_C_PPB_AUDIO_FRAME_H_ */

