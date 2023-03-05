#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include "himem.h"
#include "adpcm_encode.h"

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
//  initialize adpcm encoder handle
//
int32_t adpcm_encode_init(ADPCM_ENCODE_HANDLE* adpcm, int16_t volume) {

  int32_t rc = -1;

  adpcm->step_index = 0;
  adpcm->last_estimate = 0;
  adpcm->num_samples = 0;
  adpcm->resample_counter = 0;
  adpcm->volume = volume;

  rc = 0;

exit:
  return rc;
}

//
//  close adpcm encoder handle
//
void adpcm_encode_close(ADPCM_ENCODE_HANDLE* adpcm) {

}

//
//  execute adpcm encoding with resample
//
int32_t adpcm_encode_resample(ADPCM_ENCODE_HANDLE* adpcm, uint8_t* adpcm_buffer, int32_t adpcm_freq, int16_t* pcm_buffer, size_t pcm_buffer_len, int32_t pcm_freq, int16_t pcm_channels, int16_t little_endian) {

  // source and target buffer offset
  size_t pcm_buffer_ofs = 0;
  size_t adpcm_buffer_ofs = 0;

  while (pcm_buffer_ofs < pcm_buffer_len) {

    // down sampling
    adpcm->resample_counter += adpcm_freq;
    if (adpcm->resample_counter < pcm_freq) {
      pcm_buffer_ofs += pcm_channels;
      continue;
    }
    adpcm->resample_counter -= pcm_freq;

    // get 12bit PCM mono data
    int16_t xx = 0;
    uint8_t* p = (uint8_t*)(&pcm_buffer[ pcm_buffer_ofs ]);
    if (pcm_channels == 2) {
      // 16bit PCM LR to 12bit PCM mono
      int16_t lch = little_endian ? p[0] + p[1] * 256 : pcm_buffer[ pcm_buffer_ofs ];
      int16_t rch = little_endian ? p[2] + p[3] * 256 : pcm_buffer[ pcm_buffer_ofs + 1];
      xx = (lch + rch) / 2 / 16;
      pcm_buffer_ofs += 2;
    } else {
      // 16bit PCM mono to 12bit PCM mono
      int16_t mch = little_endian ? p[0] + p[1] * 256 : pcm_buffer[ pcm_buffer_ofs ];
      xx = mch / 16;
      pcm_buffer_ofs += 1;
    }
    if (adpcm->volume != 8) {
      xx = (int16_t)(xx * adpcm->volume / 8.0);
    }

    // encode to 4bit ADPCM data
    int16_t new_estimate;
    uint8_t code = encode(xx, adpcm->last_estimate, &adpcm->step_index, &new_estimate);
    adpcm->last_estimate = new_estimate;

    if (adpcm_buffer_ofs >= 0xff00) {
      printf("error: ADPCM encoding error - too long data for a chunk.\n");
      return 0;
    }

    // fill a byte in this order: lower 4 bit -> upper 4 bit
    if ((adpcm->num_samples % 2) == 0) {
      adpcm_buffer[ adpcm_buffer_ofs ] = code;
    } else {
      adpcm_buffer[ adpcm_buffer_ofs ] |= code << 4;
      adpcm_buffer_ofs++;
    }
    adpcm->num_samples++;

  }

//  if ((adpcm->num_samples % 2) != 0) {
//    printf("error: ADPCM encoding error - incomplete ADPCM output byte (pcm_len=%d,adpcm_samples=%d).\n",pcm_buffer_len,adpcm->num_samples);
//    return 0;
//  }

  return adpcm_buffer_ofs;
}
