#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "nas_adpcm.h"
#include "himem.h"

//
//  init decoder handle
//
int32_t nas_adpcm_init(NAS_ADPCM_DECODE_HANDLE* nas, size_t decode_buffer_len, int32_t sample_rate, int16_t channels) {

  int32_t rc = -1;

  // baseline
  nas->decode_buffer = NULL;
  nas->decode_buffer_len = decode_buffer_len;
  nas->decode_buffer_ofs = 0;
  nas->sample_rate = sample_rate;
  nas->channels = channels;
  nas->resample_counter = 0;
  nas->conv_table = NULL;
 
  // buffer allocation
  nas->decode_buffer = himem_malloc(nas->decode_buffer_len * sizeof(int16_t), 0);
  if (nas->decode_buffer == NULL) goto exit;

  // conversion table allocation and initialization
  nas->conv_table = himem_malloc(NAS_ADPCMLIB_CONV_TABLE_SIZE, 0);
  if (nas->conv_table == NULL) goto exit;

  register uint32_t reg_a0 asm ("a0") = (uint32_t)(nas->conv_table);
  asm volatile (
    "jbsr  atop_make_buffer\n"
    :                   // output operand
    : "r" (reg_a0)      // input operand
    :                   // clobbered register
  );

  register uint32_t reg_d0 asm ("d0") = (uint32_t)(nas->channels);
  asm volatile (
    "jbsr  atop_init\n"
    :                   // output operand
    : "r" (reg_d0)      // input operand
    :                   // clobbered register
  );

  rc = 0;

exit:
  return rc;
}

//
//  close decoder handle
//
void nas_adpcm_close(NAS_ADPCM_DECODE_HANDLE* nas) {
  if (nas->decode_buffer != NULL) {
    himem_free(nas->decode_buffer, 0);
    nas->decode_buffer = NULL;
  }
  if (nas->conv_table != NULL) {
    himem_free(nas->conv_table, 0);
    nas->conv_table = NULL;
  }
}

//
//  decode NAS ADPCM stream into the specified buffer
//
size_t nas_adpcm_decode_buffer(NAS_ADPCM_DECODE_HANDLE* nas, uint8_t* adpcm_data, size_t adpcm_data_bytes, int16_t* decode_buffer, size_t decode_buffer_len) {

  // check decode buffer size
  if (adpcm_data_bytes * 4 / sizeof(int16_t) > decode_buffer_len) return 0;

  // decode NAS ADPCM
  register uint32_t reg_d0 asm ("d0") = (uint32_t)(adpcm_data_bytes);
  register uint32_t reg_a0 asm ("a0") = (uint32_t)(adpcm_data);
  register uint32_t reg_a1 asm ("a1") = (uint32_t)(decode_buffer);
  asm volatile (
    "jbsr  atop_exec\n"
    :                   // output operand
    : "r" (reg_d0),     // input operand
      "r" (reg_a0),     // input operand
      "r" (reg_a1)      // input operand
    :                   // clobbered register
  );

  return adpcm_data_bytes * 4 / sizeof(int16_t);
}

//
//  decode NAS ADPCM stream into the decoder instance buffer
//
size_t nas_adpcm_decode(NAS_ADPCM_DECODE_HANDLE* nas, uint8_t* adpcm_data, size_t adpcm_data_bytes) {
  nas->decode_buffer_ofs = 
    nas_adpcm_decode_buffer(nas, adpcm_data, adpcm_data_bytes, nas->decode_buffer, nas->decode_buffer_len);
  return nas->decode_buffer_ofs;
}

//
//  resampling
//
size_t nas_adpcm_resample(NAS_ADPCM_DECODE_HANDLE* nas, int16_t* resample_buffer, int16_t gain) {

  // resampling
  size_t source_buffer_ofs = 0;
  size_t resample_buffer_ofs = 0;
  
  if (nas->channels == 2) {

    while (source_buffer_ofs < nas->decode_buffer_ofs) {
    
      // down sampling
      nas->resample_counter += 15625;
      if (nas->resample_counter < nas->sample_rate) {
        source_buffer_ofs += nas->channels;     // skip
        continue;
      }

      nas->resample_counter -= nas->sample_rate;
    
      int16_t x = ( (int32_t)(nas->decode_buffer[ source_buffer_ofs ]) + (int32_t)(nas->decode_buffer[ source_buffer_ofs + 1 ]) ) / 2 / gain;
      resample_buffer[ resample_buffer_ofs++ ] = x;
      source_buffer_ofs += 2;

    }

  } else {

    while (source_buffer_ofs < nas->decode_buffer_ofs) {
    
      // down sampling
      nas->resample_counter += 15625;
      if (nas->resample_counter < nas->sample_rate) {
        source_buffer_ofs += nas->channels;     // skip
        continue;
      }

      nas->resample_counter -= nas->sample_rate;

      resample_buffer[ resample_buffer_ofs++ ] = nas->decode_buffer[ source_buffer_ofs++ ] / gain;

    }

  }

  return resample_buffer_ofs;
}
