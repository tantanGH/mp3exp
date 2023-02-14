#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "mp3.h"
#include "memory.h"

//
//  inline helper: 24bit signed int to 16bit signed int
//
static inline int16_t scale(mad_fixed_t sample) {
  /* round */
  sample += (1L << (MAD_F_FRACBITS - 16));

  /* clip */
  if (sample >= MAD_F_ONE)
    sample = MAD_F_ONE - 1;
  else if (sample < -MAD_F_ONE)
    sample = -MAD_F_ONE;

  /* quantize */
  return sample >> (MAD_F_FRACBITS + 1 - 16);
}

//
//  init decoder handle
//
//int32_t decode_init(MP3_DECODE_HANDLE* decode, PCM_HANDLE* pcm, ADPCM_HANDLE* adpcm, int16_t out_format, FILE* fp) {
int32_t mp3_init(MP3_DECODE_HANDLE* decode) {

  decode->mp3_bit_rate = -1;
  decode->mp3_sample_rate = -1;
  decode->mp3_num_channels = -1;
  decode->decoded_samples = 0;

  memset(&(decode->mad_stream), 0, sizeof(MAD_STREAM));
  memset(&(decode->mad_frame), 0, sizeof(MAD_FRAME));
  memset(&(decode->mad_synth), 0, sizeof(MAD_SYNTH));
  memset(&(decode->mad_header), 0, sizeof(MAD_HEADER));
  memset(&(decode->mad_timer), 0, sizeof(MAD_TIMER));

  mad_stream_init(&(decode->mad_stream));
  mad_frame_init(&(decode->mad_frame));
  mad_synth_init(&(decode->mad_synth));
  mad_timer_reset(&(decode->mad_timer));

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
size_t mp3_decode(MP3_DECODE_HANDLE* decode, void* mp3_data, size_t mp3_data_len, int16_t* resample_buffer, size_t resample_buffer_len, int16_t resample_freq) {
}

// decoder handle global reference for callback functions
//static MP3_DECODE_HANDLE* g_decode;

//
//  libmad high level API callback: error
//
/*
static enum mad_flow mad_callback_error(void* data, MAD_STREAM* stream, MAD_FRAME* frame) {

  MAD_BUFFER* buffer = data;

  printf("error: decoding error 0x%04x (%s) at byte offset %u\n",
	  stream->error, mad_stream_errorstr(stream),
	  stream->this_frame - buffer->start);

  // return MAD_FLOW_BREAK here to stop decoding (and propagate an error)

  return MAD_FLOW_CONTINUE;
}
*/

//
//  libmad high level API callback: input
//
/*
static enum mad_flow mad_callback_input(void* data, MAD_STREAM* stream) {

  MAD_BUFFER* buffer = data;

  if (!buffer->length)
    return MAD_FLOW_STOP;

  mad_stream_buffer(stream, buffer->start, buffer->length);

  buffer->length = 0;

  return MAD_FLOW_CONTINUE;
}
*/

//
//  libmad high level API callback: header
//
/*
static enum mad_flow mad_callback_header(void* data, const MAD_HEADER* header) {

  if (g_decode->mp3_bit_rate < 0) {
    g_decode->mp3_bit_rate = header->bitrate;
  }
  
  if (g_decode->mp3_sample_rate < 0) {
    g_decode->mp3_sample_rate = header->samplerate;
    printf("MP3 sampling rate: %d [Hz]\n", header->samplerate);
  }
  
  return MAD_FLOW_CONTINUE;
}
*/

//
//  libmad high level API callback: output
//
/*
static enum mad_flow mad_callback_output(void* data, const MAD_HEADER* header, MAD_PCM* pcm) {

  uint16_t nchannels, nsamples;
  mad_fixed_t const *left_ch, *right_ch;

  // pcm->samplerate contains the sampling frequency

  nchannels = pcm->channels;
  nsamples  = pcm->length;
  left_ch   = pcm->samples[0];
  right_ch  = pcm->samples[1];

  if (g_decode->mp3_num_channels < 0) {
    g_decode->mp3_num_channels = nchannels;
    printf("MP3 channels: %s\n", nchannels == 2 ? "stereo" : "mono");
  }

  int16_t* pcm_buffer = g_decode->decode_buffer;
  size_t ofs = g_decode->decode_buffer_ofs / 2;     // 8bit offset to 16bit offset

  while (nsamples--) {

    // output sample(s) in 16-bit signed big-endian PCM
    pcm_buffer[ ofs++ ] = scale(*left_ch++);

    if (nchannels == 2) {
      pcm_buffer[ ofs++ ] = scale(*right_ch++);
    }

    g_decode->decode_samples++;
    g_decode->decode_buffer_ofs = ofs * 2;    // 16bit offset to 8bit offset

  }

  return MAD_FLOW_CONTINUE;
}
*/

//
//  decode mp3 (high level API)
//
/*
int32_t mp3_decode(MP3_DECODE_HANDLE* decode, void* mp3_data, size_t mp3_data_len) {

  MAD_BUFFER buffer;
  MAD_DECODER decoder;
  int16_t result;

  // initialize our private message structure

  g_decode = decode;

  buffer.start  = mp3_data;
  buffer.length = mp3_data_len;

  // configure input, output, and error functions

  mad_decoder_init(&decoder, &buffer, mad_callback_input, mad_callback_header, NULL, mad_callback_output, mad_callback_error, NULL); 

  // start decoding

  result = mad_decoder_run(&decoder, MAD_DECODER_MODE_SYNC);

  // release the decoder

  mad_decoder_finish(&decoder);

  return result;
}
*/