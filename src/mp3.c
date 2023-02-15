#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "mp3.h"
#include "memory.h"

//#define DEBUG

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
int32_t mp3_decode(MP3_DECODE_HANDLE* decode, 
                    int16_t* resample_buffer, size_t resample_buffer_len, 
                    int16_t resample_freq, size_t* resampled_len) {

  int32_t rc = -1;
  int32_t resample_ofs = 0;

  do {
    
    if (decode->current_mad_pcm == NULL) {

      int16_t result = mad_frame_decode(&(decode->mad_frame), &(decode->mad_stream));
      if (result == -1) {
        if (MAD_RECOVERABLE(decode->mad_stream.error)) {
          continue;
        } else {
          //printf("error: %s\n", mad_stream_errorstr(&(decode->mad_stream)));
          //goto exit;
          break;
        }
      }

      if (decode->mp3_bit_rate < 0) {
        MAD_HEADER* h = &(decode->mad_frame.header);
        decode->mp3_bit_rate = h->bitrate;
        decode->mp3_sample_rate = h->samplerate;
        decode->mp3_num_channels = h->mode == MAD_MODE_SINGLE_CHANNEL ? 1 : 2;
  #ifdef DEBUG
        printf("bitrate=%d, samplerate=%d, channels=%d\n",decode->mp3_bit_rate,decode->mp3_sample_rate,decode->mp3_num_channels);
  #endif
      }
      
      mad_synth_frame(&(decode->mad_synth), &(decode->mad_frame));
      mad_timer_add(&(decode->mad_timer), decode->mad_frame.header.duration);

      decode->current_mad_pcm = &(decode->mad_synth.pcm);

    } 

    MAD_PCM* pcm = decode->current_mad_pcm;
#ifdef DEBUG
    printf("stream buffer=%X,ptr=%X,buffer_end=%X,pcm length=%d,samplerate=%d,channels=%d\n",
    decode->mad_stream.buffer,decode->mad_stream.ptr.byte,decode->mad_stream.bufend,
    pcm->length,pcm->samplerate,pcm->channels);
#endif
    if (resample_ofs + pcm->length > resample_buffer_len) {
#ifdef DEBUG
      printf("insufficient MP3 decode buffer.\n");
#endif
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
#ifdef DEBUG
    printf("resample_ofs=%d\n",resample_ofs);
#endif

  } while (1);

  rc = 0;

exit:

  *resampled_len = resample_ofs;

#ifdef DEBUG
  printf("decoded %d samples\n", resample_ofs);
#endif

  return rc;
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