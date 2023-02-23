#ifndef __H_NAS_ADPCM__
#define __H_NAS_ADPCM__

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

#define NAS_ADPCMLIB_CONV_TABLE_SIZE (141312)

typedef struct {

  int32_t sample_rate;
  int16_t channels;

  size_t resample_counter;

  size_t decode_buffer_len;
  size_t decode_buffer_ofs;
  int16_t* decode_buffer;

  uint8_t* conv_table;

} NAS_ADPCM_DECODE_HANDLE;

int32_t nas_adpcm_init(NAS_ADPCM_DECODE_HANDLE* nas, size_t decode_buffer_bytes, int32_t sample_rate, int16_t channels);
void nas_adpcm_close(NAS_ADPCM_DECODE_HANDLE* nas);
size_t nas_adpcm_decode_buffer(NAS_ADPCM_DECODE_HANDLE* nas, uint8_t* adpcm_data, size_t adpcm_data_bytes, int16_t* decode_buffer, size_t decode_buffer_len);
size_t nas_adpcm_decode(NAS_ADPCM_DECODE_HANDLE* nas, uint8_t* adpcm_data, size_t adpcm_data_bytes);
size_t nas_adpcm_resample(NAS_ADPCM_DECODE_HANDLE* nas, int16_t* resample_buffer, int16_t gain);
#endif