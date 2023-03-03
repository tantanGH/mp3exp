#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "himem.h"
#include "raw_decode.h"

//
//  init raw pcm decoder handle
//
int32_t raw_decode_init(RAW_DECODE_HANDLE* pcm, int32_t sample_rate, int16_t channels, int16_t little_endian) {

  int32_t rc = -1;

  // baseline
//  nas->decode_buffer = NULL;
//  nas->decode_buffer_len = decode_buffer_len;
//  nas->decode_buffer_ofs = 0;
  pcm->sample_rate = sample_rate;
  pcm->channels = channels;
  pcm->little_endian = little_endian;
  pcm->resample_counter = 0;
 
  // buffer allocation
//  nas->decode_buffer = himem_malloc(nas->decode_buffer_len * sizeof(int16_t), 0);
//  if (nas->decode_buffer == NULL) goto exit;

  rc = 0;

exit:
  return rc;
}

//
//  close decoder handle
//
void raw_decode_close(RAW_DECODE_HANDLE* pcm) {
//  if (nas->decode_buffer != NULL) {
//    himem_free(nas->decode_buffer, 0);
//    nas->decode_buffer = NULL;
//  }
}

//
//  resampling
//
size_t raw_decode_resample(RAW_DECODE_HANDLE* pcm, int16_t* resample_buffer, int32_t resample_freq, int16_t* source_buffer, size_t source_buffer_len, int16_t gain) {

  // resampling
  size_t source_buffer_ofs = 0;
  size_t resample_buffer_ofs = 0;
  
  if (pcm->channels == 2) {

    if (pcm->little_endian) {

      while (source_buffer_ofs < source_buffer_len) {
      
        // down sampling
        pcm->resample_counter += resample_freq;
        if (pcm->resample_counter < pcm->sample_rate) {
          source_buffer_ofs += pcm->channels;     // skip
          continue;
        }

        pcm->resample_counter -= pcm->sample_rate;
      
        // little endian
        uint8_t* source_buffer_uint8 = (uint8_t*)(&(source_buffer[ source_buffer_ofs ]));
        int16_t lch = (int16_t)(source_buffer_uint8[1] * 256 + source_buffer_uint8[0]);
        int16_t rch = (int16_t)(source_buffer_uint8[3] * 256 + source_buffer_uint8[2]);
        int16_t x = ((int32_t)(lch + rch)) / 2 / gain;
        resample_buffer[ resample_buffer_ofs++ ] = x;
        source_buffer_ofs += 2;

      }

    } else {

      while (source_buffer_ofs < source_buffer_len) {
      
        // down sampling
        pcm->resample_counter += resample_freq;
        if (pcm->resample_counter < pcm->sample_rate) {
          source_buffer_ofs += pcm->channels;     // skip
          continue;
        }

        pcm->resample_counter -= pcm->sample_rate;
      
        // big endian
        int16_t x = ( (int32_t)(source_buffer[ source_buffer_ofs ]) + (int32_t)(source_buffer[ source_buffer_ofs + 1 ]) ) / 2 / gain;
        resample_buffer[ resample_buffer_ofs++ ] = x;
        source_buffer_ofs += 2;

      }

    }

  } else {

    if (pcm->little_endian) {

      while (source_buffer_ofs < source_buffer_len) {
    
        // down sampling
        pcm->resample_counter += resample_freq;
        if (pcm->resample_counter < pcm->sample_rate) {
          source_buffer_ofs += pcm->channels;     // skip
          continue;
        }

        pcm->resample_counter -= pcm->sample_rate;

        // little endian
        uint8_t* source_buffer_uint8 = (uint8_t*)(&(source_buffer[ source_buffer_ofs ]));
        int16_t mch = (int16_t)(source_buffer_uint8[1] * 256 + source_buffer_uint8[0]);
        int16_t x = mch / 2 / gain;
        resample_buffer[ resample_buffer_ofs++ ] = mch / 2 / gain;
        source_buffer_ofs += 1;

      }

    } else {

      while (source_buffer_ofs < source_buffer_len) {

        // down sampling
        pcm->resample_counter += resample_freq;
        if (pcm->resample_counter < pcm->sample_rate) {
          source_buffer_ofs += pcm->channels;     // skip
          continue;
        }

        pcm->resample_counter -= pcm->sample_rate;

        // big endian
        resample_buffer[ resample_buffer_ofs++ ] = source_buffer[ source_buffer_ofs++ ] / gain;

      }

    }

  }

  return resample_buffer_ofs;
}
