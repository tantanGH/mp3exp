#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "mp3.h"
#include "memory.h"

//
//  inline helper: 24bit signed int to 16bit signed int
//
static inline int16_t scale_16bit(mad_fixed_t sample) {
  // round
  sample += (1L << (MAD_F_FRACBITS - 16));

  // clip
  if (sample >= MAD_F_ONE)
    sample = MAD_F_ONE - 1;
  else if (sample < -MAD_F_ONE)
    sample = -MAD_F_ONE;

  // quantize
  return sample >> (MAD_F_FRACBITS + 1 - 16);
}

//
//  inline helper: 24bit signed int to 12bit signed int
//
static inline int16_t scale_12bit(mad_fixed_t sample) {
  // round
  sample += (1L << (MAD_F_FRACBITS - 12));

  // clip
  if (sample >= MAD_F_ONE)
    sample = MAD_F_ONE - 1;
  else if (sample < -MAD_F_ONE)
    sample = -MAD_F_ONE;

  // quantize
  return sample >> (MAD_F_FRACBITS + 1 - 12);
}

//
//  init decoder handle
//
int32_t mp3_init(MP3_DECODE_HANDLE* decode, void* mp3_data, size_t mp3_data_len) {

  decode->mp3_data = mp3_data;
  decode->mp3_data_len = mp3_data_len;
  decode->mp3_bit_rate = -1;
  decode->mp3_sample_rate = -1;
  decode->mp3_num_channels = -1;
  decode->resample_counter = 0;

  memset(&(decode->mad_stream), 0, sizeof(MAD_STREAM));
  memset(&(decode->mad_frame), 0, sizeof(MAD_FRAME));
  memset(&(decode->mad_synth), 0, sizeof(MAD_SYNTH));
  memset(&(decode->mad_timer), 0, sizeof(MAD_TIMER));

  mad_stream_init(&(decode->mad_stream));
  mad_frame_init(&(decode->mad_frame));
  mad_synth_init(&(decode->mad_synth));
  mad_timer_reset(&(decode->mad_timer));

  mad_stream_buffer(&(decode->mad_stream), mp3_data, mp3_data_len);

  decode->current_mad_pcm = NULL;

  return 0;
}

//
//  close decoder handle
//
void mp3_close(MP3_DECODE_HANDLE* decode) {
  mad_synth_finish(&(decode->mad_synth));
  mad_frame_finish(&(decode->mad_frame));
  mad_stream_finish(&(decode->mad_stream));
}

//
//  decode MP3 stream
//
int32_t mp3_decode(MP3_DECODE_HANDLE* decode, int16_t* resample_buffer, size_t resample_buffer_len, int16_t resample_freq, size_t* resampled_len) {

  // default return code
  int32_t rc = -1;

  // down sampling counter
  int32_t resample_ofs = 0;

  for (;;) {
    
    if (decode->current_mad_pcm == NULL) {

      int16_t result = mad_frame_decode(&(decode->mad_frame), &(decode->mad_stream));
      if (result == -1) {
        if (decode->mad_stream.error == MAD_ERROR_BUFLEN) {
          // MP3 EOF
          break;
        } else if (MAD_RECOVERABLE(decode->mad_stream.error)) {
          continue;
        } else {
          printf("error: %s\n", mad_stream_errorstr(&(decode->mad_stream)));
          goto exit;
        }
      }

      if (decode->mp3_bit_rate < 0) {
        MAD_HEADER* h = &(decode->mad_frame.header);
        decode->mp3_bit_rate = h->bitrate;
        decode->mp3_sample_rate = h->samplerate;
        decode->mp3_num_channels = h->mode == MAD_MODE_SINGLE_CHANNEL ? 1 : 2;
      }
      
      mad_synth_frame(&(decode->mad_synth), &(decode->mad_frame));
      mad_timer_add(&(decode->mad_timer), decode->mad_frame.header.duration);

      decode->current_mad_pcm = &(decode->mad_synth.pcm);

    } 

    MAD_PCM* pcm = decode->current_mad_pcm;
    if (resample_ofs + pcm->length > resample_buffer_len) {
      // no more buffer space to write
      break;
    }

    if (pcm->channels == 2) {

      for (int32_t i = 0; i < pcm->length; i++) {

        // down sampling
        decode->resample_counter += resample_freq;
        if (decode->resample_counter < pcm->samplerate) {
          continue;
        }

        decode->resample_counter -= pcm->samplerate;
    
        // stereo to mono
        int16_t x = ( scale_12bit(pcm->samples[0][i]) + scale_12bit(pcm->samples[1][i]) ) / 2;
        resample_buffer[ resample_ofs++ ] = x;

      }

    } else {

      for (int32_t i = 0; i < pcm->length; i++) {

        // down sampling
        decode->resample_counter += resample_freq;
        if (decode->resample_counter < pcm->samplerate) {
          continue;
        }

        decode->resample_counter -= pcm->samplerate;
    
        int16_t x = scale_12bit(pcm->samples[0][i]);
        resample_buffer[ resample_ofs++ ] = x;

      }

    }

    decode->current_mad_pcm = NULL;

  }

  // success
  rc = 0;

exit:

  // push resampled count
  *resampled_len = resample_ofs;

  return rc;
}
