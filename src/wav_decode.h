#ifndef __H_WAV__
#define __H_WAV__

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

typedef struct {

  int32_t sample_rate;
  int16_t channels;

  size_t resample_counter;

//  size_t decode_buffer_len;
//  size_t decode_buffer_ofs;
//  int16_t* decode_buffer;

} WAV_DECODE_HANDLE;

int32_t wav_decode_init(WAV_DECODE_HANDLE* wav);
void wav_decode_close(WAV_DECODE_HANDLE* wav);
int32_t wav_decode_parse_header(WAV_DECODE_HANDLE* wav, FILE* fp);
size_t wav_decode_resample(WAV_DECODE_HANDLE* wav, int16_t* resample_buffer, int32_t resample_freq, int16_t* source_buffer, size_t source_buffer_len, int16_t gain);

#endif