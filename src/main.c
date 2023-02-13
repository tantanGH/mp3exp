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
#include "mp3exp.h"

static void abort_application() {
  ADPCMMOD(0);
  printf("aborted.\n");
  exit(1);
}

static void show_help_message() {
  printf("  usage: mp3exp [options] <input-file[.pcm|.s32|.s44|.s48|.m32|.m44|.m48|.mp3]>\n");
  printf("options:\n");
  printf("     -a ... use PCM8A.X for ADPCM encoding\n");
  printf("     -u ... use 060turbo high memory for buffering\n");
  printf("     -h ... show help message\n");
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

  // parse command line
  uint8_t* pcm_file_name = NULL;
  int16_t encode_with_pcm8a = 0;
  int16_t use_high_memory = 0; 
  for (int16_t i = 1; i < argc; i++) {
    if (argv[i][0] == '-' && strlen(argv[i]) >= 2) {
      if (argv[i][1] == 'a') {
        encode_with_pcm8a = 1;
      } else if (argv[i][1] == 'u') {
        use_high_memory = 1;
      } else if (argv[i][1] == 'h') {
        show_help_message();
        goto exit;
      } else {
        printf("error: unknown option (%s).\n",argv[i]);
        goto exit;
      }
    } else {
      pcm_file_name = argv[i];
    }
  }

  if (pcm_file_name == NULL) {
    printf("error: no pcm file.\n");
    goto exit;
  }

  // input pcm file name and extension
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

  // reset PCM8 / PCM8A / IOCS ADPCM
  if (pcm8_mode != PCM8_MODE_NONE) {
    pcm8_pause();
    pcm8_stop();
  } else {
    ADPCMMOD(0);
  }

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

  // int chain tables
  CHAIN_TABLE chain_tables[ NUM_CHAINS ];
  for (int16_t i = 0; i < NUM_CHAINS; i++) {
    chain_tables[i].buffer = adpcm_encoder.buffers[i];
    chain_tables[i].buffer_bytes = 0;
    chain_tables[i].next = &(chain_tables[ ( i + 1 ) % NUM_CHAINS ]);
  }

  // init file read buffer
  size_t fread_buffer_len = pcm_freq * pcm_channels * 2;      // max 2 sec to read
  if (encode_mode != ENCODE_MODE_NONE) {
    fread_buffer = malloc_himem(fread_buffer_len * sizeof(int16_t), use_high_memory);
    if (fread_buffer == NULL) {
      printf("error: file read buffer memory allocation error.\n");
      goto catch;
    }
  }

  // resampling buffer
  size_t resample_buffer_len = 15625 * 1 * 2;
  if (encode_mode != ENCODE_MODE_NONE) {
    resample_buffer = malloc_himem(resample_buffer_len * sizeof(int16_t), use_high_memory);
    if (resample_buffer == NULL) {
      printf("error: resampling buffer memory allocation error.\n");
      goto catch;
    }
  }

  // open pcm file
  fp = fopen(pcm_file_name, "rb");
  if (fp == NULL) {
    printf("error: cannot open pcm file (%s).\n", pcm_file_name);
    goto catch;
  }

  // check file size
  fseek(fp, 0, SEEK_END);
  uint32_t pcm_file_size = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  float pcm_1sec_size = pcm_freq * pcm_channels * (encode_mode == 1 ? 2 : 0.5);

  // describe PCM attributes
  printf("\n");
  printf("PCM file name: %s\n", pcm_file_name);
  printf("PCM file size: %d bytes\n", pcm_file_size);
  printf("PCM format: %s\n", encode_mode != ENCODE_MODE_NONE ? "16bit signed PCM (big)" : "ADPCM(MSM6258V)");
  printf("PCM frequency: %d [Hz]\n", pcm_freq);
  printf("PCM channels: %s\n", pcm_channels == 1 ? "mono" : "stereo");
  printf("PCM length: %4.2f seconds\n", (float)pcm_file_size / pcm_1sec_size);
  printf("\n");

  // describe PCM drivers
  if (pcm8_mode == PCM8_MODE_NONE) {
    printf("PCM driver: IOCS\n");
  } else if (pcm8_mode == PCM8_MODE_PCM8) {
    printf("PCM driver: PCM8\n");
  } else if (pcm8_mode == PCM8_MODE_PCM8A) {
    printf("PCM driver: PCM8A\n");
  }
  if (encode_mode == ENCODE_MODE_PCM8A) {
    printf("ADPCM encoding: PCM8A\n");
  } else if (encode_mode == ENCODE_MODE_SELF) {
    printf("ADPCM encoding: MP3EXP\n");
  } else {
    printf("ADPCM encoding: (none)\n");
  }

  // preplay buffering
  printf("\nbuffering...");
  int16_t end_flag = 0;
  for (int16_t i = 0; i < NUM_CHAINS; i++) {

    if (end_flag) break;

    if (encode_mode == ENCODE_MODE_PCM8A) {

      // ADPCM encode with PCM8A
      size_t fread_len = fread(fread_buffer, 2, fread_buffer_len, fp);  
      if (fread_len < fread_buffer_len) {
        chain_tables[i].next = NULL;
        end_flag = 1;
      }
      size_t resample_len = adpcm_resample(&adpcm_encoder, chain_tables[i].buffer, fread_buffer, fread_len, pcm_freq, pcm_channels, pcm_gain);
      chain_tables[i].buffer_bytes = resample_len * sizeof(int16_t);

    } else if (encode_mode == ENCODE_MODE_NONE) {

      // ADPCM through (no encoding)
      size_t fread_len = fread(chain_tables[i].buffer, 1, adpcm_encoder.buffer_len, fp);
      if (fread_len < adpcm_encoder.buffer_len) {
        chain_tables[i].next = NULL;
        end_flag = 1;
      }
      chain_tables[i].buffer_bytes = fread_len;

    } else {

      // ADPCM self encoding
      int16_t orig_id = adpcm_encoder.current_buffer_id;
      do {
        size_t fread_len = fread(fread_buffer, 2, fread_buffer_len, fp);
        size_t resample_len = adpcm_resample(&adpcm_encoder, resample_buffer, fread_buffer, fread_len, pcm_freq, pcm_channels, pcm_gain);
        adpcm_encode(&adpcm_encoder, resample_buffer, resample_len * sizeof(int16_t), 16, 1);
        if (fread_len < fread_buffer_len) {
          chain_tables[i].next = NULL;
          end_flag = 1;
          break;
        }
      } while (adpcm_encoder.current_buffer_id == orig_id);

      chain_tables[i].buffer = adpcm_encoder.buffers[ orig_id ];
      chain_tables[i].buffer_bytes = (chain_tables[i].next == NULL) ? adpcm_encoder.buffer_ofs : adpcm_encoder.buffer_len; 

    }

  }

  // start playing
  if (encode_mode == ENCODE_MODE_PCM8A) {

    // PCM8A encoding mode
    int16_t pcm8a_volume = 0x08;
    int16_t pcm8a_freq = 0x14;
    int16_t pcm8a_pan = 0x03;
    int32_t pcm8a_mode = ( pcm8a_volume << 16 ) | ( pcm8a_freq << 8 ) | pcm8a_pan;
    pcm8a_set_polyphonic_mode(1);   // must use polyphonic mode for 16bit PCM use
    pcm8a_play_linked_array_chain(0, pcm8a_mode, &(chain_tables[0]));

  } else if (pcm8_mode != PCM8_MODE_NONE) {

    // other PCM8/PCM8A mode
    pcm8_set_polyphonic_mode(0);    // disable polyphonic mode for performance

    int32_t mode = 4 * 256 + 3;
    ADPCMLOT((struct CHAIN2*)(&chain_tables[0]), mode);

  } else {

    // IOCS ADPCM mode
    int32_t mode = 4 * 256 + 3;
    ADPCMLOT((struct CHAIN2*)(&chain_tables[0]), mode);

  }

  printf("\rpush ESC key to stop.\n");

  int16_t current_chain = 0;

  for (;;) {
   
    // dummy wait
    // for (int32_t t0 = ONTIME(); ONTIME() < t0 + 10;) {}

    // check esc key to exit
    if (B_KEYSNS() != 0) {
      int16_t scan_code = B_KEYINP() >> 8;
      if (scan_code == KEY_SCAN_CODE_ESC) {
        break;
      }
    }
 
    // exit if not playing
    if (end_flag) {
      if (encode_mode == ENCODE_MODE_PCM8A) {
        if (pcm8a_get_data_length(0) == 0) break;
      } else {
        if (ADPCMSNS() == 0) break;
      }
    }

    // check buffer flip
    CHAIN_TABLE* cta = &(chain_tables[ current_chain ]);
    int16_t buffer_flip = 0;
    if (encode_mode == ENCODE_MODE_PCM8A) {
      void* pcm8a_addr = pcm8a_get_access_address(0);
      if (pcm8a_addr < cta->buffer || pcm8a_addr >= cta->buffer + cta->buffer_bytes) {
        buffer_flip = 1;
      }
    } else {
      void* cur_bar = (void*)REG_DMAC_CH3_BAR[0];     // = next chain table pointer
      if (cur_bar != cta->next) {
        buffer_flip = 1;
      }
    }

    // process additional data if buffer flip happens
    if (buffer_flip) {

      current_chain = ( current_chain + 1 ) % NUM_CHAINS;

      if (encode_mode == ENCODE_MODE_PCM8A) {

        // ADPCM encoding with PCM8A
        size_t fread_len = fread(fread_buffer, 2, fread_buffer_len, fp);  
        if (fread_len < fread_buffer_len) {
          cta->next = NULL;
          end_flag = 1;
        }
        size_t resample_len = adpcm_resample(&adpcm_encoder, cta->buffer, fread_buffer, fread_len, pcm_freq, pcm_channels, pcm_gain);
        cta->buffer_bytes = resample_len * sizeof(int16_t);

      } else if (encode_mode == ENCODE_MODE_NONE) {

        // ADPCM through (no encoding)
        size_t fread_len = fread(cta->buffer, 1, adpcm_encoder.buffer_len, fp);
        if (fread_len < adpcm_encoder.buffer_len) {
          cta->next = NULL;
          end_flag = 1;
        }
        cta->buffer_bytes = fread_len;        

      } else {

        // ADPCM self encoding
        int16_t orig_id = adpcm_encoder.current_buffer_id;
        do {
          size_t fread_len = fread(fread_buffer, 2, fread_buffer_len, fp);
          size_t resample_len = adpcm_resample(&adpcm_encoder, resample_buffer, fread_buffer, fread_len, pcm_freq, pcm_channels, pcm_gain);
          adpcm_encode(&adpcm_encoder, resample_buffer, resample_len * sizeof(int16_t), 16, 1);
          if (fread_len < fread_buffer_len) {
            cta->next = NULL;
            end_flag = 1;
            break;
          }
        } while (adpcm_encoder.current_buffer_id == orig_id);

        cta->buffer = adpcm_encoder.buffers[ orig_id ];
        cta->buffer_bytes = (cta->next == NULL) ? adpcm_encoder.buffer_ofs : adpcm_encoder.buffer_len; 

      }

      // buffer underrun check
      if (end_flag == 0) {    // if already in the last chain, continue risk play
        if (encode_mode == ENCODE_MODE_PCM8A) {
          void* pcm8a_addr = pcm8a_get_access_address(0);
          if (pcm8a_addr >= cta->buffer && pcm8a_addr < cta->buffer + cta->buffer_bytes) {
            printf("error: buffer underrun during playback.\n");
            goto catch;            
          }
        } else {
          void* cur_bar = (void*)REG_DMAC_CH3_BAR[0];     // = next chain table pointer
          if (cur_bar == cta->next) {
            printf("error: buffer underrun during playback.\n");
            goto catch;
          }
        }
      }
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
    free_himem(resample_buffer, use_high_memory);
    resample_buffer = NULL;
  }
  if (fread_buffer != NULL) {
    free_himem(fread_buffer, use_high_memory);
    fread_buffer = NULL;
  }

  // close adpcm encoder
  adpcm_close(&adpcm_encoder);

  // enable pcm8 polyphonic mode
  if (pcm8_mode != PCM8_MODE_NONE) {
    if (pcm8_set_polyphonic_mode(-1) == 0) {
      pcm8_set_polyphonic_mode(1);
      //printf("enabled PCM8/PCM8A polyphonic mode.\n");
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

