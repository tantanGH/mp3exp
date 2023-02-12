#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <iocslib.h>
#include "memory.h"
#include "keyboard.h"
#include "adpcm.h"
#include "pcm8.h"
#include "pcm8a.h"

#define VERSION "0.1.0 (2023/02/13)"

#define REG_DMAC_CH3_MAR ((volatile uint32_t*)(0xE840C0 + 0x0C))
#define REG_DMAC_CH3_BAR ((volatile uint32_t*)(0xE840C0 + 0x1C))

static void show_help_message() {
  printf("usage: pcmexp [options] <pcm-file[.pcm|.s32|.s44|.s48|.m32|.m44|.m48]>\n");
//  printf("options:\n");
//  printf("     -p ... convert PCM to ADPCM with PCM8A.X (faster, default)\n");
//  printf("     -a ... convert PCM to ADPCM with PCMEXP.X (better qulity)\n");
}

int32_t main(int32_t argc, uint8_t* argv[]) {

  // default return code
  int32_t rc = 1;

  // credit
  printf("PCMEXP.X - ADPCM & 16bit PCM player version " VERSION " by tantan\n");

  // argument count check
  if (argc < 2) {
    show_help_message();
    goto exit;
  }

  // input pcm file name and extension
  uint8_t* pcm_file_name = argv[1];
  uint8_t* pcm_file_exp = pcm_file_name + strlen(pcm_file_name) - 4;

  // input format check
  int32_t pcm_freq = 15625;
  int32_t pcm_channels = 1;
  int32_t encode_mode = 0;
  if (stricmp(".s32", pcm_file_exp) == 0) {
    pcm_freq = 32000;
    pcm_channels = 2;
    encode_mode = 1;
  } else if (stricmp(".s44", pcm_file_exp) == 0) {
    pcm_freq = 44100;
    pcm_channels = 2;
    encode_mode = 1;
  } else if (stricmp(".s48", pcm_file_exp) == 0) {
    pcm_freq = 48000;
    pcm_channels = 2;
    encode_mode = 1;
  } else if (stricmp(".m32", pcm_file_exp) == 0) {
    pcm_freq = 32000;
    pcm_channels = 1;
    encode_mode = 1;
  } else if (stricmp(".m44", pcm_file_exp) == 0) {
    pcm_freq = 44100;
    pcm_channels = 1;
    encode_mode = 1;
  } else if (stricmp(".m48", pcm_file_exp) == 0) {
    pcm_freq = 48000;
    pcm_channels = 1;
    encode_mode = 1;
  } else if (stricmp(".pcm", pcm_file_exp) == 0) {
    pcm_freq = 15625;
    pcm_channels = 1;
    encode_mode = 0;
  } else {
    printf("error: unknown format.\n");
    goto exit;
  }

  // enter supervisor mode
  B_SUPER(0);

  // pcm8 mode
  int16_t pcm8_mode = 0;      // no PCM8

  // PCM8.X running check
  if (pcm8_keepchk()) {
    pcm8_mode = 1;
  }

  // PCM8A.X running check
  if (pcm8a_keepchk()) {
    pcm8_mode = 2;
  }

  // reset PCM8.X / PCM8A.X
  if (pcm8_mode != 0) {
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
  void* convert_buffer = NULL;
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
      printf("error: memory allocation error.\n");
      goto catch;
    }
  }

  // convert buffer
  size_t convert_buffer_len = 15625 * 1 * 2;
  if (encode_mode) {
    convert_buffer = malloc_himem(convert_buffer_len * sizeof(int16_t), 0);
    if (convert_buffer == NULL) {
      printf("error: memory allocation error.\n");
      goto catch;
    }
  }

  // open pcm file
  fp = fopen(pcm_file_name,"rb");

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
  if (pcm8_mode == 0) {
    printf("ADPCM control: IOCS\n");
  } else if (pcm8_mode == 1) {
    printf("ADPCM control: PCM8\n");
  } else if (pcm8_mode == 2) {
    printf("ADPCM control: PCM8A\n");
  }

  if (encode_mode) {

    int16_t orig_id = adpcm_encoder.current_buffer_id;

    // fill current buffer
    do {

      size_t fread_len = fread(fread_buffer, 2, fread_buffer_len, fp);      // 1 unit = 16bit word
      size_t convert_len = adpcm_resample(&adpcm_encoder, convert_buffer, fread_buffer, fread_len, pcm_freq, pcm_channels);
      adpcm_encode(&adpcm_encoder, convert_buffer, convert_len * sizeof(int16_t), 16, 1);
      if (fread_len < fread_buffer_len) {
        ch_tbl1.next = NULL;
        break;
      }


    } while (adpcm_encoder.current_buffer_id == orig_id);

    ch_tbl1.buffer = adpcm_encoder.buffers[ orig_id ];
    ch_tbl1.buffer_bytes = (ch_tbl1.next == NULL) ? adpcm_encoder.buffer_ofs : adpcm_encoder.buffer_len; 

  } else {

    size_t fread_len = fread(ch_tbl1.buffer, 1, adpcm_encoder.buffer_len, fp);    // 1 unit = 8bit byte
    if (fread_len < adpcm_encoder.buffer_len) {
      ch_tbl1.next = NULL;
    }
    ch_tbl1.buffer_bytes = fread_len;

  }

  if (ch_tbl1.next != NULL) {

    if (encode_mode) {

      int16_t orig_id = adpcm_encoder.current_buffer_id;

      do {

        size_t fread_len = fread(fread_buffer, 2, fread_buffer_len, fp);      // 1 unit = 16bit word
        size_t convert_len = adpcm_resample(&adpcm_encoder, convert_buffer, fread_buffer, fread_len, pcm_freq, pcm_channels);
        adpcm_encode(&adpcm_encoder, convert_buffer, convert_len * sizeof(int16_t), 16, 1);
        if (fread_len < fread_buffer_len) {
          ch_tbl2.next = NULL;
          break;
        }
     
      } while (adpcm_encoder.current_buffer_id == orig_id);

      ch_tbl2.buffer = adpcm_encoder.buffers[ orig_id ];
      ch_tbl2.buffer_bytes = (ch_tbl2.next == NULL) ? adpcm_encoder.buffer_ofs : adpcm_encoder.buffer_len; 

    } else {

      size_t fread_len = fread(ch_tbl2.buffer, 1, adpcm_encoder.buffer_len, fp);    // 1 unit = 8bit byte
      if (fread_len < adpcm_encoder.buffer_len) {
        ch_tbl2.next = NULL;
      }
      ch_tbl2.buffer_bytes = fread_len;

    }
  }

  // start play
  if (pcm8_mode != 0) {
    int16_t volume = 0x08;
    int32_t channel_mode = ( volume << 16 ) | ( 0x04 << 8 ) | 3;
    //pcm8_set_polyphonic_mode(1);
    pcm8_play_linked_array_chain(0, channel_mode, &ch_tbl1);
  } else {
    int32_t mode = 4 * 256 + 3;
    ADPCMLOT((struct CHAIN2*)(&ch_tbl1), mode);
  }

  printf("push ESC key to stop.\n");

  uint32_t cur_bar = REG_DMAC_CH3_BAR[0];
  int16_t cur_buf = 1;

  for (;;) {
   
    // check esc key to exit
    if (B_KEYSNS() != 0) {
      int16_t scan_code = B_KEYINP() >> 8;
      if (scan_code == KEY_SCAN_CODE_ESC) {
        break;
      }
    }

    for (int32_t t0 = ONTIME(); ONTIME() < t0 + 10;) {}     // wait 100 msec

//    printf("%d, %X, %X, %X, %X\n", cur_buf, cur_mar, cur_bar, ch_tbl1.buffer, ch_tbl2.buffer);

    // double buffering
    if (cur_buf == 1 && ch_tbl2.next != NULL) {
      int16_t playing_buf = -1;
      if (pcm8_mode == 0) {
        void* cur_bar = (void*)REG_DMAC_CH3_BAR[0];     // = next chain table pointer
        if (cur_bar == &ch_tbl1) {
          playing_buf = 2;
        }
        //printf("%d, %X, %X, %X, %X, %X\n", cur_buf, cur_bar, &ch_tbl1, &ch_tbl2, ch_tbl1.buffer, ch_tbl2.buffer);
      } else if (pcm8_mode == 1) {
        // TODO
      } else if (pcm8_mode == 2) {
        void* pcm8a_addr = pcm8a_get_access_address(0);
        if (pcm8a_addr >= ch_tbl2.buffer && pcm8a_addr < ch_tbl2.buffer + ch_tbl2.buffer_bytes) {
          playing_buf = 2;
        }
      }

      if (playing_buf == 2) {

        cur_buf = 2;

        if (encode_mode) {

          int16_t orig_id = adpcm_encoder.current_buffer_id;

          do {

            size_t fread_len = fread(fread_buffer, 2, fread_buffer_len, fp);      // 1 unit = 16bit word
            size_t convert_len = adpcm_resample(&adpcm_encoder, convert_buffer, fread_buffer, fread_len, pcm_freq, pcm_channels);
            adpcm_encode(&adpcm_encoder, convert_buffer, convert_len * sizeof(int16_t), 16, 1);
            if (fread_len < fread_buffer_len) {
              ch_tbl1.next = NULL;
              break;
            }

          } while (adpcm_encoder.current_buffer_id == orig_id);

          ch_tbl1.buffer = adpcm_encoder.buffers[ orig_id ];
          ch_tbl1.buffer_bytes = (ch_tbl1.next == NULL) ? adpcm_encoder.buffer_ofs : adpcm_encoder.buffer_len; 

        } else {

          size_t fread_len = fread(ch_tbl1.buffer, 1, adpcm_encoder.buffer_len, fp);
          if (fread_len < adpcm_encoder.buffer_len) {
            ch_tbl1.next = NULL;
          }
          ch_tbl1.buffer_bytes = fread_len;        

        }

      }

    } else if (cur_buf == 2 && ch_tbl1.next != NULL) {

      int16_t playing_buf = -1;
      if (pcm8_mode == 0) {
        void* cur_bar = (void*)REG_DMAC_CH3_BAR[0];   // = next chain table ptr
        if (cur_bar == &ch_tbl2) {
          playing_buf = 1;
        }
        //printf("%d, %X, %X, %X, %X, %X\n", cur_buf, cur_bar, &ch_tbl1, &ch_tbl2, ch_tbl1.buffer, ch_tbl2.buffer);
      } else if (pcm8_mode == 1) {
        // TODO
      } else if (pcm8_mode == 2) {
        void* pcm8a_addr = pcm8a_get_access_address(0);
        if (pcm8a_addr >= ch_tbl1.buffer && pcm8a_addr < ch_tbl1.buffer + ch_tbl1.buffer_bytes) {
          playing_buf = 1;
        }
      }

      if (playing_buf == 1) {

        cur_buf = 1;

        if (encode_mode) {

          int16_t orig_id = adpcm_encoder.current_buffer_id;

          do {

            size_t fread_len = fread(fread_buffer, 2, fread_buffer_len, fp);      // 1 unit = 16bit word
            size_t convert_len = adpcm_resample(&adpcm_encoder, convert_buffer, fread_buffer, fread_len, pcm_freq, pcm_channels);
            adpcm_encode(&adpcm_encoder, convert_buffer, convert_len * sizeof(int16_t), 16, 1);
            if (fread_len < fread_buffer_len) {
              ch_tbl2.next = NULL;
              break;
            }
        
          } while (adpcm_encoder.current_buffer_id == orig_id);

          ch_tbl2.buffer = adpcm_encoder.buffers[ orig_id ];
          ch_tbl2.buffer_bytes = (ch_tbl2.next == NULL) ? adpcm_encoder.buffer_ofs : adpcm_encoder.buffer_len; 

        } else {

          size_t fread_len = fread(ch_tbl2.buffer, 1, adpcm_encoder.buffer_len, fp);
          if (fread_len < adpcm_encoder.buffer_len) {
            ch_tbl2.next = NULL;
          }
          ch_tbl2.buffer_bytes = fread_len;        

        }
      }
    }
 
    // exit if not playing
    if (pcm8_mode != 0) {
      if (pcm8_get_data_length(0) == 0) break;
    } else {
      if (ADPCMSNS() == 0) break;
    }
  }

  // reset PCM8
  if (pcm8_mode != 0) {
    pcm8_pause();
    pcm8_stop();
  } else {
    ADPCMMOD(0);
  }

  // success return code
  rc = 0;

catch:

  // close pcm file
  if (fp != NULL) {
    fclose(fp);
    fp = NULL;
  }

  // reclaim memory buffers
  if (convert_buffer != NULL) {
    free_himem(convert_buffer, 0);
    convert_buffer = NULL;
  }
  if (fread_buffer != NULL) {
    free_himem(fread_buffer, 0);
    fread_buffer = NULL;
  }
  if (ch_tbl1.buffer != NULL) {
    free_himem(ch_tbl1.buffer, 0);
    ch_tbl1.buffer = NULL;
  }
  if (ch_tbl2.buffer != NULL) {
    free_himem(ch_tbl2.buffer, 0);
    ch_tbl2.buffer = NULL;
  }

  // close adpcm encoder
  adpcm_close(&adpcm_encoder);

exit:

  // flush key buffer
  while (B_KEYSNS() != 0) {
    B_KEYINP();
  }
 
  return rc;
}

