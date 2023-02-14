#ifndef __H_MP3__
#define __H_MP3__

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include "mad.h"

//#define DECODE_BUFFER_LEN (48000*2*2*4)

typedef struct {
  uint8_t* start;
  size_t length;
} MAD_BUFFER;

typedef struct mad_stream MAD_STREAM;
typedef struct mad_synth MAD_SYNTH;
typedef struct mad_header MAD_HEADER;
typedef struct mad_pcm MAD_PCM;
typedef struct mad_frame MAD_FRAME;
typedef struct mad_decoder MAD_DECODER;
typedef mad_timer_t MAD_TIMER;

typedef struct {

  MAD_STREAM mad_stream;
  MAD_FRAME mad_frame;
  MAD_SYNTH mad_synth;
//  MAD_HEADER mad_header;
  MAD_TIMER mad_timer;

//  int16_t use_high_memory;

  int32_t mp3_bit_rate;
  int32_t mp3_sample_rate;
  int32_t mp3_num_channels;

  size_t resample_counter;

  size_t decoded_samples;
//  size_t decode_buffer_len;
//  size_t decode_buffer_ofs;
//  void* decode_buffer;

} MP3_DECODE_HANDLE;

int32_t mp3_init(MP3_DECODE_HANDLE* decode);
void mp3_close(MP3_DECODE_HANDLE* decode);
int32_t mp3_decode(MP3_DECODE_HANDLE* decode, void* mp3_data, size_t mp3_data_len, 
                   int16_t* resample_buffer, size_t resample_buffer_len, 
                   int16_t resample_freq, int16_t resample_gain, size_t* resampled_len);

#endif