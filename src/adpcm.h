#ifndef __H_ADPCM__
#define __H_ADPCM__

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

#define ADPCM_BUFFER_SIZE  (0xff00)
#define ADPCM_MAX_BUFFER_COUNT (132)

typedef struct {
  int16_t step_index;
  int16_t last_estimate;
  size_t num_samples;
  size_t resample_counter;
  int16_t use_high_memory;
  int16_t current_buffer_id;
  int16_t buffer_count;
  size_t buffer_bytes;
  size_t buffer_ofs;
  uint8_t* buffers[ ADPCM_MAX_BUFFER_COUNT ];
} ADPCM_HANDLE;

int32_t adpcm_init(ADPCM_HANDLE* adpcm, int16_t buffer_count, int16_t use_high_memory);
void adpcm_close(ADPCM_HANDLE* adpcm);
int32_t adpcm_encode(ADPCM_HANDLE* adpcm, void* pcm_buffer, size_t pcm_buffer_len, int16_t pcm_bit_depth, int16_t pcm_channels);
size_t adpcm_resample(ADPCM_HANDLE* adpcm, int16_t* convert_buffer, int16_t* source_buffer, size_t source_buffer_len, int32_t source_pcm_freq, int16_t source_pcm_channels, int16_t gain);

#endif