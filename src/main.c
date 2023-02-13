#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <doslib.h>
#include <iocslib.h>
#include "memory.h"
#include "keyboard.h"
#include "adpcm.h"
#include "pcm8.h"
#include "pcm8a.h"

#define VERSION "0.1.0 (2023/02/13)"

#define ENCODE_MODE_NONE  (0)
#define ENCODE_MODE_SELF  (1)
#define ENCODE_MODE_PCM8A (2)

#define PCM8_MODE_NONE  (0)
#define PCM8_MODE_PCM8  (1)
#define PCM8_MODE_PCM8A (2)

#define REG_DMAC_CH3_BAR ((volatile uint32_t*)(0xE840C0 + 0x1C))

static void abort_application() {
  ADPCMMOD(0);
  printf("aborted.\n");
  exit(1);
}

static void show_help_message() {
  printf("  usage: mp3exp [options] <input-file[.pcm|.s32|.s44|.s48|.m32|.m44|.m48|.mp3]>\n");
  printf("options:\n");
  printf("     -a ... use PCM8A.X for ADPCM encoding\n");
}

int32_t main(int32_t argc, uint8_t* argv[]) {

  // default return code
  int32_t rc = 1;

  // credit
  printf("MP3EXP.X - ADPCM/PCM/MP3 player for X680x0 version " VERSION " by tantan\n");

  // argument count check
  if (argc < 2) {
    show_help_message();
    goto exit;
  }

  // use PCM8A.X for encoding?
  int16_t encode_with_pcm8a = 0;
  int16_t aofs = 1;
  if (strcmp(argv[aofs],"-a") == 0) {
    encode_with_pcm8a = 1;
    if (argc < 3) {
      show_help_message();
      goto exit;
    }
    aofs++;
  }

  // input pcm file name and extension
  uint8_t* pcm_file_name = argv[aofs];
  uint8_t* pcm_file_exp = pcm_file_name + strlen(pcm_file_name) - 4;

  // input format check
  int32_t pcm_freq = 15625;
  int16_t pcm_channels = 1;
  int16_t pcm_gain = encode_with_pcm8a ? 16 : 1; 
  int32_t encode_mode = ENCODE_MODE_NONE;
  if (stricmp(".s32", pcm_file_exp) == 0) {
    pcm_freq = 32000;
    pcm_channels = 2;
    encode_mode = ENCODE_MODE_SELF;
  } else if (stricmp(".s44", pcm_file_exp) == 0) {
    pcm_freq = 44100;
    pcm_channels = 2;
    encode_mode = ENCODE_MODE_SELF;
  } else if (stricmp(".s48", pcm_file_exp) == 0) {
    pcm_freq = 48000;
    pcm_channels = 2;
    encode_mode = ENCODE_MODE_SELF;
  } else if (stricmp(".m32", pcm_file_exp) == 0) {
    pcm_freq = 32000;
    pcm_channels = 1;
    encode_mode = ENCODE_MODE_SELF;
  } else if (stricmp(".m44", pcm_file_exp) == 0) {
    pcm_freq = 44100;
    pcm_channels = 1;
    encode_mode = ENCODE_MODE_SELF;
  } else if (stricmp(".m48", pcm_file_exp) == 0) {
    pcm_freq = 48000;
    pcm_channels = 1;
    encode_mode = ENCODE_MODE_SELF;
  } else if (stricmp(".pcm", pcm_file_exp) == 0) {
    pcm_freq = 15625;
    pcm_channels = 1;
    encode_mode = ENCODE_MODE_NONE;
  } else {
    printf("error: unknown format.\n");
    goto exit;
  }

  // cursor off
  C_CUROFF();

  // abort vectors
  uint32_t abort_vector1 = INTVCS(0xFFF1, (int8_t*)abort_application);
  uint32_t abort_vector2 = INTVCS(0xFFF2, (int8_t*)abort_application);  

  // enter supervisor mode
  B_SUPER(0);

  // PCM8 mode
  int16_t pcm8_mode = PCM8_MODE_NONE;

  // PCM8 running check
  if (pcm8_keepchk()) {
    pcm8_mode = PCM8_MODE_PCM8;
  }

  // PCM8A running check
  if (pcm8a_keepchk()) {
    pcm8_mode = PCM8_MODE_PCM8A;
  }

  // can we use PCM8A?
  if (encode_mode != ENCODE_MODE_NONE && encode_with_pcm8a) {
    if (pcm8_mode != PCM8_MODE_PCM8A) {
      printf("error: PCM8A is not running.\n");
      goto exit;
    } else {
      encode_mode = ENCODE_MODE_PCM8A;
    }
  }

  // reset PCM8 / PCM8A
  if (pcm8_mode != PCM8_MODE_NONE) {
    pcm8_pause();
    pcm8_stop();
  } else {
    ADPCMMOD(0);
  }

  // chain tables
  CHAIN_TABLE ch_tbl1 = { 0 };
  CHAIN_TABLE ch_tbl2 = { 0 };

  // file read buffer
  void* fread_buffer = NULL;
  void* resample_buffer = NULL;
  FILE* fp = NULL;

  // init adpcm encoder
  ADPCM_HANDLE adpcm_encoder = { 0 };
  if (adpcm_init(&adpcm_encoder) != 0) {
    printf("error: ADPCM encoder initialization error.\n");
    goto catch;
  }

  // init chain tables
  ch_tbl1.buffer = adpcm_encoder.buffers[0];
  ch_tbl1.buffer_bytes = 0;
  ch_tbl1.next = &ch_tbl2;

  ch_tbl2.buffer = adpcm_encoder.buffers[1];
  ch_tbl2.buffer_bytes = 0;
  ch_tbl2.next = &ch_tbl1;

  // init file read buffer
  size_t fread_buffer_len = pcm_freq * pcm_channels * 2;      // max 2 sec to read
  if (encode_mode) {
    fread_buffer = malloc_himem(fread_buffer_len * sizeof(int16_t), 0);
    if (fread_buffer == NULL) {
      printf("error: file read buffer memory allocation error.\n");
      goto catch;
    }
  }

  // resample buffer
  size_t resample_buffer_len = 15625 * 1 * 2;
  if (encode_mode) {
    resample_buffer = malloc_himem(resample_buffer_len * sizeof(int16_t), 0);
    if (resample_buffer == NULL) {
      printf("error: resampling buffer memory allocation error.\n");
      goto catch;
    }
  }

  // open pcm file
  fp = fopen(pcm_file_name,"rb");
  if (fp == NULL) {
    printf("error: cannot open pcm file (%s).\n", pcm_file_name);
    goto catch;
  }

  // check file size
  fseek(fp, 0, SEEK_END);
  uint32_t pcm_file_size = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  float pcm_1sec_size = pcm_freq * pcm_channels * (encode_mode == 1 ? 2 : 0.5);

  // show file information
  printf("\n");
  printf("PCM file name: %s\n", pcm_file_name);
  printf("PCM file size: %d bytes\n", pcm_file_size);
  printf("PCM format: %s\n", encode_mode == 1 ? "16bit signed PCM (big)" : "ADPCM(MSM6258V)");
  printf("PCM frequency: %d [Hz]\n", pcm_freq);
  printf("PCM channels: %s\n", pcm_channels == 1 ? "mono" : "stereo");
  printf("PCM length: %4.2f seconds\n", (float)pcm_file_size / pcm_1sec_size);
  printf("\n");

  // show PCM driver name
  if (pcm8_mode == PCM8_MODE_NONE) {
    //printf("ADPCM control: IOCS\n");
  } else if (pcm8_mode == PCM8_MODE_PCM8) {
    printf("detected PCM8\n");
  } else if (pcm8_mode == PCM8_MODE_PCM8A) {
    printf("detected PCM8A\n");
  }
  if (encode_mode == ENCODE_MODE_PCM8A) {
    printf("use PCM8A for ADPCM encoding.\n");
  }

  // buffer1 initial fill
  printf("buffering...\n");
  int16_t end_flag = 0;
  if (encode_mode == ENCODE_MODE_PCM8A) {

    // ADPCM encode with PCM8A
    size_t fread_len = fread(fread_buffer, 2, fread_buffer_len, fp);  
    if (fread_len < fread_buffer_len) {
      ch_tbl1.next = NULL;
      end_flag = 1;
    }
    size_t resample_len = adpcm_resample(&adpcm_encoder, ch_tbl1.buffer, fread_buffer, fread_len, pcm_freq, pcm_channels, pcm_gain);
    ch_tbl1.buffer_bytes = resample_len * sizeof(int16_t);

  } else if (encode_mode == ENCODE_MODE_NONE) {

    // ADPCM through (no encoding)
    size_t fread_len = fread(ch_tbl1.buffer, 1, adpcm_encoder.buffer_len, fp);
    if (fread_len < adpcm_encoder.buffer_len) {
      ch_tbl1.next = NULL;
      end_flag = 1;
    }
    ch_tbl1.buffer_bytes = fread_len;

  } else {

    // ADPCM self encoding
    int16_t orig_id = adpcm_encoder.current_buffer_id;
    do {
      size_t fread_len = fread(fread_buffer, 2, fread_buffer_len, fp);
      size_t resample_len = adpcm_resample(&adpcm_encoder, resample_buffer, fread_buffer, fread_len, pcm_freq, pcm_channels, pcm_gain);
      adpcm_encode(&adpcm_encoder, resample_buffer, resample_len * sizeof(int16_t), 16, 1);
      if (fread_len < fread_buffer_len) {
        ch_tbl1.next = NULL;
        end_flag = 1;
        break;
      }
    } while (adpcm_encoder.current_buffer_id == orig_id);

    ch_tbl1.buffer = adpcm_encoder.buffers[ orig_id ];
    ch_tbl1.buffer_bytes = (ch_tbl1.next == NULL) ? adpcm_encoder.buffer_ofs : adpcm_encoder.buffer_len; 

  }

  // buffer2 initial fill
  if (end_flag == 0) {

    if (encode_mode == ENCODE_MODE_PCM8A) {

      // ADPCM encoding with PCM8A
      size_t fread_len = fread(fread_buffer, 2, fread_buffer_len, fp);  
      if (fread_len < fread_buffer_len) {
        ch_tbl2.next = NULL;
        end_flag = 1;
      }
      size_t resample_len = adpcm_resample(&adpcm_encoder, ch_tbl2.buffer, fread_buffer, fread_len, pcm_freq, pcm_channels, pcm_gain);
      ch_tbl2.buffer_bytes = resample_len * sizeof(int16_t);

    } else if (encode_mode == ENCODE_MODE_NONE) {

      // ADPCM through (no encoding)
      size_t fread_len = fread(ch_tbl2.buffer, 1, adpcm_encoder.buffer_len, fp);    // 1 unit = 8bit byte
      if (fread_len < adpcm_encoder.buffer_len) {
        ch_tbl2.next = NULL;
        end_flag = 1;
      }
      ch_tbl2.buffer_bytes = fread_len;

    } else {

      // ADPCM self encoding
      int16_t orig_id = adpcm_encoder.current_buffer_id;
      do {
        size_t fread_len = fread(fread_buffer, 2, fread_buffer_len, fp);
        size_t resample_len = adpcm_resample(&adpcm_encoder, resample_buffer, fread_buffer, fread_len, pcm_freq, pcm_channels, pcm_gain);
        adpcm_encode(&adpcm_encoder, resample_buffer, resample_len * sizeof(int16_t), 16, 1);
        if (fread_len < fread_buffer_len) {
          ch_tbl2.next = NULL;
          end_flag = 1;
          break;
        }
      } while (adpcm_encoder.current_buffer_id == orig_id);

      ch_tbl2.buffer = adpcm_encoder.buffers[ orig_id ];
      ch_tbl2.buffer_bytes = (ch_tbl2.next == NULL) ? adpcm_encoder.buffer_ofs : adpcm_encoder.buffer_len; 

    }
  }

  // start play
  if (encode_mode == ENCODE_MODE_PCM8A) {
    // PCM8A encoding mode
    int16_t pcm8a_volume = 0x08;
    int16_t pcm8a_freq = 0x14;
    int16_t pcm8a_pan = 0x03;
    int32_t pcm8a_mode = ( pcm8a_volume << 16 ) | ( pcm8a_freq << 8 ) | pcm8a_pan;
    pcm8a_set_polyphonic_mode(1);
    pcm8a_play_linked_array_chain(0, pcm8a_mode, &ch_tbl1);
  } else if (pcm8_mode != PCM8_MODE_NONE) {
    // other PCM8/PCM8A mode
    pcm8_set_polyphonic_mode(0);
    printf("disabled PCM8/PCM8A polyphonic mode.\n");
    int32_t mode = 4 * 256 + 3;
    ADPCMLOT((struct CHAIN2*)(&ch_tbl1), mode);
  } else {
    // IOCS ADPCM mode
    int32_t mode = 4 * 256 + 3;
    ADPCMLOT((struct CHAIN2*)(&ch_tbl1), mode);
  }

  printf("\npush ESC key to stop.\n");

  int16_t cur_buf = 1;

  for (;;) {
   
    // check esc key to exit
    if (B_KEYSNS() != 0) {
      int16_t scan_code = B_KEYINP() >> 8;
      if (scan_code == KEY_SCAN_CODE_ESC) {
        break;
      }
    }

    // for (int32_t t0 = ONTIME(); ONTIME() < t0 + 10;) {}     // wait 100 msec

    // double buffering
    if (cur_buf == 1 && end_flag == 0) {

      int16_t playing_buf = -1;
      if (encode_mode == ENCODE_MODE_PCM8A) {
        void* pcm8a_addr = pcm8a_get_access_address(0);
        if (pcm8a_addr >= ch_tbl2.buffer && pcm8a_addr < ch_tbl2.buffer + ch_tbl2.buffer_bytes) {
          playing_buf = 2;
        }
      } else {
        void* cur_bar = (void*)REG_DMAC_CH3_BAR[0];     // = next chain table pointer
        if (cur_bar == &ch_tbl1) {
          playing_buf = 2;
        }
      }

      if (playing_buf == 2) {

        cur_buf = 2;

        if (encode_mode == ENCODE_MODE_PCM8A) {

          // ADPCM encoding with PCM8A
          size_t fread_len = fread(fread_buffer, 2, fread_buffer_len, fp);  
          if (fread_len < fread_buffer_len) {
            ch_tbl1.next = NULL;
            end_flag = 1;
          }
          size_t resample_len = adpcm_resample(&adpcm_encoder, ch_tbl1.buffer, fread_buffer, fread_len, pcm_freq, pcm_channels, pcm_gain);
          ch_tbl1.buffer_bytes = resample_len * sizeof(int16_t);

        } else if (encode_mode == ENCODE_MODE_NONE) {

          // ADPCM through (no encoding)
          size_t fread_len = fread(ch_tbl1.buffer, 1, adpcm_encoder.buffer_len, fp);
          if (fread_len < adpcm_encoder.buffer_len) {
            ch_tbl1.next = NULL;
            end_flag = 1;
          }
          ch_tbl1.buffer_bytes = fread_len;        

        } else {

          // ADPCM self encoding
          int16_t orig_id = adpcm_encoder.current_buffer_id;
          do {
            size_t fread_len = fread(fread_buffer, 2, fread_buffer_len, fp);
            size_t resample_len = adpcm_resample(&adpcm_encoder, resample_buffer, fread_buffer, fread_len, pcm_freq, pcm_channels, pcm_gain);
            adpcm_encode(&adpcm_encoder, resample_buffer, resample_len * sizeof(int16_t), 16, 1);
            if (fread_len < fread_buffer_len) {
              ch_tbl1.next = NULL;
              end_flag = 1;
              break;
            }
          } while (adpcm_encoder.current_buffer_id == orig_id);

          ch_tbl1.buffer = adpcm_encoder.buffers[ orig_id ];
          ch_tbl1.buffer_bytes = (ch_tbl1.next == NULL) ? adpcm_encoder.buffer_ofs : adpcm_encoder.buffer_len; 

        }

        // buffer underrun check
        if (encode_mode == ENCODE_MODE_PCM8A) {
          void* pcm8a_addr = pcm8a_get_access_address(0);
          if (pcm8a_addr >= ch_tbl1.buffer && pcm8a_addr < ch_tbl1.buffer + ch_tbl1.buffer_bytes) {
            printf("error: buffer underrun during playback.\n");
            goto catch;            
          }
        } else {
          void* cur_bar = (void*)REG_DMAC_CH3_BAR[0];     // = next chain table pointer
          if (cur_bar == &ch_tbl2) {
            printf("error: buffer underrun during playback.\n");
            goto catch;
          }
        }
      }

    } else if (cur_buf == 2 && end_flag == 0) {

      int16_t playing_buf = -1;
      if (encode_mode == ENCODE_MODE_PCM8A) {
        void* pcm8a_addr = pcm8a_get_access_address(0);
        if (pcm8a_addr >= ch_tbl1.buffer && pcm8a_addr < ch_tbl1.buffer + ch_tbl1.buffer_bytes) {
          playing_buf = 1;
        }
      } else {
        void* cur_bar = (void*)REG_DMAC_CH3_BAR[0];     // = next chain table pointer
        if (cur_bar == &ch_tbl2) {
          playing_buf = 1;
        }
      }

      if (playing_buf == 1) {

        cur_buf = 1;

        if (encode_mode == ENCODE_MODE_PCM8A) {

          size_t fread_len = fread(fread_buffer, 2, fread_buffer_len, fp);  
          if (fread_len < fread_buffer_len) {
            ch_tbl2.next = NULL;
            end_flag = 1;
          }
          size_t resample_len = adpcm_resample(&adpcm_encoder, ch_tbl2.buffer, fread_buffer, fread_len, pcm_freq, pcm_channels, pcm_gain);
          ch_tbl2.buffer_bytes = resample_len * sizeof(int16_t);

        } else if (encode_mode == ENCODE_MODE_NONE) {

          size_t fread_len = fread(ch_tbl2.buffer, 1, adpcm_encoder.buffer_len, fp);
          if (fread_len < adpcm_encoder.buffer_len) {
            ch_tbl2.next = NULL;
            end_flag = 1;
          }
          ch_tbl2.buffer_bytes = fread_len;        

        } else {

          int16_t orig_id = adpcm_encoder.current_buffer_id;
          do {
            size_t fread_len = fread(fread_buffer, 2, fread_buffer_len, fp);      // 1 unit = 16bit word
            size_t resample_len = adpcm_resample(&adpcm_encoder, resample_buffer, fread_buffer, fread_len, pcm_freq, pcm_channels, pcm_gain);
            adpcm_encode(&adpcm_encoder, resample_buffer, resample_len * sizeof(int16_t), 16, 1);
            if (fread_len < fread_buffer_len) {
              ch_tbl2.next = NULL;
              end_flag = 1;
              break;
            }
          } while (adpcm_encoder.current_buffer_id == orig_id);

          ch_tbl2.buffer = adpcm_encoder.buffers[ orig_id ];
          ch_tbl2.buffer_bytes = (ch_tbl2.next == NULL) ? adpcm_encoder.buffer_ofs : adpcm_encoder.buffer_len; 

        }

        // buffer underrun check
        if (encode_mode == ENCODE_MODE_PCM8A) {
          void* pcm8a_addr = pcm8a_get_access_address(0);
          if (pcm8a_addr >= ch_tbl2.buffer && pcm8a_addr < ch_tbl2.buffer + ch_tbl2.buffer_bytes) {
            printf("error: buffer underrun during playback.\n");
            goto catch;            
          }
        } else {
          void* cur_bar = (void*)REG_DMAC_CH3_BAR[0];     // = next chain table pointer
          if (cur_bar == &ch_tbl1) {
            printf("error: buffer underrun during playback.\n");
            goto catch;
          }
        }
      }
    }
 
    // exit if not playing
    if (encode_mode == ENCODE_MODE_PCM8A) {
      if (end_flag == 1 && pcm8a_get_data_length(0) == 0) break;
    } else {
      if (end_flag == 1 && ADPCMSNS() == 0) break;
    }
  }

  // success return code
  rc = 0;

catch:
  // reset ADPCM
  if (encode_mode == ENCODE_MODE_PCM8A) {
    pcm8a_pause();
    pcm8a_stop();
  } else {
    ADPCMMOD(0);
  }

  // close pcm file
  if (fp != NULL) {
    fclose(fp);
    fp = NULL;
  }

  // reclaim memory buffers
  if (resample_buffer != NULL) {
    free_himem(resample_buffer, 0);
    resample_buffer = NULL;
  }
  if (fread_buffer != NULL) {
    free_himem(fread_buffer, 0);
    fread_buffer = NULL;
  }

  // close adpcm encoder
  adpcm_close(&adpcm_encoder);

  // enable pcm8 polyphonic mode
  if (pcm8_mode != PCM8_MODE_NONE) {
    if (pcm8_set_polyphonic_mode(-1) == 0) {
      pcm8_set_polyphonic_mode(1);
      printf("enabled PCM8/PCM8A polyphonic mode.\n");
    }
  }

exit:

  // flush key buffer
  while (B_KEYSNS() != 0) {
    B_KEYINP();
  }
 
  // cursor on
  C_CURON();

  // resume abort vectors
  INTVCS(0xFFF1, (int8_t*)abort_vector1);
  INTVCS(0xFFF2, (int8_t*)abort_vector2);  

  return rc;
}

