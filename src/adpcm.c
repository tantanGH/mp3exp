#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include "adpcm.h"
#include "himem.h"

//
//  MSM6258V ADPCM constant tables
//
static const int16_t step_adjust[] = { -1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8 };

static const int16_t step_size[] = { 
        16,  17,  19,  21,  23,  25,  28,  31,  34,  37,  41,  45,   50,   55,   60,   66,
        73,  80,  88,  97, 107, 118, 130, 143, 157, 173, 190, 209,  230,  253,  279,  307,
       337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552 };

//
//  MSM6258V ADPCM decode
//
static int16_t decode(uint8_t code, int16_t* step_index, int16_t last_data) {

  int16_t si = *step_index;
  int16_t ss = step_size[ si ];

  int16_t delta = ( ss >> 3 );
  if (code & 0x01) {
    delta += (ss >> 2);
  }
  if (code & 0x02) {
    delta += (ss >> 1);
  }
  if (code & 0x04) {
    delta += ss;
  }
  if (code & 0x08) {
    delta = -delta;
  }
    
  int16_t estimate = last_data + delta;
  if (estimate > 2047) {
    estimate = 2047;
  }

  if (estimate < -2048) {
    estimate = -2048;
  }

  si += step_adjust[ code ];
  if (si < 0) {
    si = 0;
  }
  if (si > 48) {
    si = 48;
  }
  *step_index = si;

  return estimate;
}

//
//  MSM6258V ADPCM encode
//
static uint8_t encode(int16_t current_data, int16_t last_estimate, int16_t* step_index, int16_t* new_estimate) {

  int16_t ss = step_size[ *step_index ];

  int16_t delta = current_data - last_estimate;

  uint8_t code = 0x00;
  if (delta < 0) {
    code = 0x08;          // bit3 = 1
    delta = -delta;
  }
  if (delta >= ss) {
    code += 0x04;         // bit2 = 1
    delta -= ss;
  }
  if (delta >= (ss>>1)) {
    code += 0x02;         // bit1 = 1
    delta -= ss>>1;
  }
  if (delta >= (ss>>2)) {
    code += 0x01;         // bit0 = 1
  } 

  // need to use decoder to estimate
  *new_estimate = decode(code, step_index, last_estimate);

  return code;
}

//
//  initialize adpcm handle
//
int32_t adpcm_init(ADPCM_HANDLE* adpcm, int16_t buffer_count, int16_t use_high_memory) {

  int32_t rc = -1;

  adpcm->step_index = 0;
  adpcm->last_estimate = 0;
  adpcm->num_samples = 0;
  adpcm->resample_counter = 0;

  adpcm->use_high_memory = use_high_memory;
  adpcm->current_buffer_id = 0;
  adpcm->buffer_count = buffer_count;
  adpcm->buffer_bytes = ADPCM_BUFFER_SIZE;
  adpcm->buffer_ofs = 0;

  for (int16_t i = 0; i < adpcm->buffer_count; i++) {
    adpcm->buffers[i] = himem_malloc(adpcm->buffer_bytes, adpcm->use_high_memory);
    if (adpcm->buffers[i] == NULL) {
      goto exit;
    }
  }

  rc = 0;

exit:
  return rc;
}

//
//  close adpcm handle
//
void adpcm_close(ADPCM_HANDLE* adpcm) {

  // reclaim buffers
  for (int16_t i = 0; i < adpcm->buffer_count; i++) {
    if (adpcm->buffers[i] != NULL) {
      himem_free(adpcm->buffers[i], adpcm->use_high_memory);
      adpcm->buffers[i] = NULL;
    }
  }
}

//
//  execute adpcm encoding
//
int32_t adpcm_encode(ADPCM_HANDLE* adpcm, void* pcm_buffer, size_t pcm_buffer_len, int16_t pcm_bit_depth, int16_t pcm_channels) {

  // default return code
  int32_t rc = -1;

  // number of source PCM data samples and offset
  size_t pcm_samples = pcm_buffer_len * 8 / pcm_bit_depth;
  size_t pcm_ofs = 0;

  while (pcm_ofs < pcm_samples) {

    // get 12bit PCM mono data
    int16_t xx = 0;
    if (pcm_bit_depth == 16) {
      int16_t* pcm = (int16_t*)pcm_buffer;
      if (pcm_channels == 2) {
        // 16bit PCM LR to 12bit PCM mono
        xx = (pcm[ pcm_ofs ] + pcm[ pcm_ofs + 1 ]) / 2 / 16;
        pcm_ofs += 2;
      } else {
        // 16bit PCM mono to 12bit PCM mono
        xx = pcm[ pcm_ofs ] / 16;
        pcm_ofs += 1;
      }
    } else if (pcm_bit_depth == 8) {
      int8_t* pcm = (int8_t*)pcm_buffer;
      if (pcm_channels == 2) {
        // 8bit PCM LR to 12bit PCM mono
        xx = (pcm[ pcm_ofs ] + pcm[ pcm_ofs + 1 ]) * 16 / 2;
        pcm_ofs += 2;
      } else {
        // 8bit PCM mono to 12bit PCM mono
        xx = pcm[ pcm_ofs ] * 16;
        pcm_ofs += 1;
      }
    } else {
      printf("error: unsupported pcm bit depth.\n");
      goto exit;
    }

    // encode to 4bit ADPCM data
    int16_t new_estimate;
    uint8_t code = encode(xx, adpcm->last_estimate, &adpcm->step_index, &new_estimate);
    adpcm->last_estimate = new_estimate;

    // current buffer is full?
    if (adpcm->buffer_ofs >= adpcm->buffer_bytes) {
      int16_t orig = adpcm->current_buffer_id;
      adpcm->current_buffer_id = (orig + 1) % adpcm->buffer_count;
      adpcm->buffer_ofs = 0;
    }

    // fill a byte in this order: lower 4 bit -> upper 4 bit
    uint8_t* buffer = adpcm->buffers[ adpcm->current_buffer_id ];
    if ((adpcm->num_samples % 2) == 0) {
      buffer[ adpcm->buffer_ofs ] = code;
    } else {
      buffer[ adpcm->buffer_ofs ] |= code << 4;
      adpcm->buffer_ofs++;
    }
    adpcm->num_samples++;

  }

  rc = 0;

exit:
  return rc;
}

//
//  resampling
//
size_t adpcm_resample(ADPCM_HANDLE* adpcm, int16_t* convert_buffer, int16_t* source_buffer, size_t source_buffer_len, int32_t source_pcm_freq, int16_t source_pcm_channels, int16_t gain) {

  size_t convert_buffer_ofs = 0;
  size_t source_buffer_ofs = 0;

  if (source_pcm_channels == 2) {

    while (source_buffer_ofs < source_buffer_len) {
    
      // down sampling
      adpcm->resample_counter += 15625;                 // need to preserve processed count for better accuracy
      if (adpcm->resample_counter < source_pcm_freq) {
        source_buffer_ofs += source_pcm_channels;     // skip
        continue;
      }

      adpcm->resample_counter -= source_pcm_freq;
    
      int16_t x = ( (int32_t)(source_buffer[ source_buffer_ofs ]) + (int32_t)(source_buffer[ source_buffer_ofs + 1 ]) ) / 2 / gain;
      convert_buffer[ convert_buffer_ofs++ ] = x;
      source_buffer_ofs += 2;

    }

  } else {

    while (source_buffer_ofs < source_buffer_len) {
    
      // down sampling
      adpcm->resample_counter += 15625;                 // need to preserve processed count for better accuracy
      if (adpcm->resample_counter < source_pcm_freq) {
        source_buffer_ofs += source_pcm_channels;     // skip
        continue;
      }

      adpcm->resample_counter -= source_pcm_freq;

      convert_buffer[ convert_buffer_ofs++ ] = source_buffer[ source_buffer_ofs++ ] / gain;

    }

  }

  return convert_buffer_ofs;
}
