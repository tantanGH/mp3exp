#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stat.h>
#include <doslib.h>
#include <iocslib.h>
#include "keyboard.h"
#include "crtc.h"
#include "himem.h"
#include "pcm8.h"
#include "pcm8a.h"
#include "pcm8pp.h"
#include "adpcm.h"
#include "nas_adpcm.h"
#include "mp3.h"
#include "mp3exp.h"

//#define DEBUG

// abort vector handler
static void abort_application() {

  // stop ADPCM
  ADPCMMOD(0);

  // cursor on
  C_CURON();
 
  // flush key buffer
  while (B_KEYSNS() != 0) {
    B_KEYINP();
  }
 
  printf("aborted.\n");
  exit(1);
}

// show help message
static void show_help_message() {
  printf("usage: mp3exp [options] <input-file[.pcm|.s(32|44|48)|.m(32|44|48)|.a(32|44|48)|.n(32|44|48)|.mp3]>\n");
  printf("options:\n");
  printf("     -a    ... use MP3EXP for ADPCM encoding\n");
  printf("     -b<n> ... buffer size [x 64KB] (2-96,default:4)\n");
  printf("     -u    ... use 060turbo/TS-6BE16 high memory for buffering\n");
  printf("     -l[n] ... loop count (none:infinite, default:1)\n");
  printf("     -q[n] ... mp3 quality (0:high, 1:normal, 2:low, default:1)\n");
  printf("     -t[n] ... mp3 album art display brightness (1-100, default:off)\n");
  printf("     -x    ... mp3 album art display full size\n");
  printf("     -v[n] ... pcm8a/pcm8pp volume (1-15, default:7)\n");
  printf("     -c    ... do not use .s44/.a44 as mp3 cache\n");
  printf("     -h    ... show help message\n");
}

// main
int32_t main(int32_t argc, uint8_t* argv[]) {

  // default return code
  int32_t rc = 1;

  // credit
  printf("MP3EXP.X - ADPCM/PCM/MP3 player for X680x0 version " VERSION " by tantan\n");

  // parse command line options
  uint8_t* pcm_file_name = NULL;
  int16_t encode_with_self = 0;
  int16_t num_chains = 4;
  int16_t loop_count = 1;
  int16_t use_high_memory = 0;
  int32_t adpcm_output_freq = 15625;
  int16_t mp3_quality = 1;
  int16_t mp3_pic_brightness = 0;
  int16_t mp3_pic_half_size = 1;
  int16_t mp3_cache_unuse = 0;
  int16_t pcm8_volume = 7;
  for (int16_t i = 1; i < argc; i++) {
    if (argv[i][0] == '-' && strlen(argv[i]) >= 2) {
      if (argv[i][1] == 'a') {
        encode_with_self = 1;
      } else if (argv[i][1] == 'b') {
        num_chains = atoi(argv[i]+2);
        if (num_chains < 2 || num_chains > 96) {
          show_help_message();
          goto exit;
        }
      } else if (argv[i][1] == 'l') {
        loop_count = atoi(argv[i]+2);
      } else if (argv[i][1] == 'u') {
        if (!himem_isavailable()) {
          printf("error: high memory driver is not installed.\n");
          goto exit;
        }
        use_high_memory = 1;
      } else if (argv[i][1] == 'q') {
        mp3_quality = atoi(argv[i]+2);
        if (mp3_quality < 0 || mp3_quality > 2 || strlen(argv[i]) < 3) {
          show_help_message();
          goto exit;
        }
      } else if (argv[i][1] == 't') {
        mp3_pic_brightness = atoi(argv[i]+2);
        if (mp3_pic_brightness < 0 || mp3_pic_brightness > 100 || strlen(argv[i]) < 3) {
          show_help_message();
          goto exit;
        }
      } else if (argv[i][1] == 'x') {
        mp3_pic_half_size = 0;
      } else if (argv[i][1] == 'c') {
        mp3_cache_unuse = 1;
      } else if (argv[i][1] == 'v') {
        pcm8_volume = atoi(argv[i]+2);
        if (pcm8_volume < 1 || pcm8_volume > 15 || strlen(argv[i]) < 3) {
          show_help_message();
          goto exit;
        }
      } else if (argv[i][1] == 'o') {
        int16_t out_freq = atoi(argv[i]+2);
        if (out_freq == 2) {
          adpcm_output_freq = 7812;
        } else if (out_freq == 1) {
          adpcm_output_freq = 10417;
        } else {
          adpcm_output_freq = 15625;
        }
      } else if (argv[i][1] == 'h') {
        show_help_message();
        goto exit;
      } else {
        printf("error: unknown option (%s).\n",argv[i]);
        goto exit;
      }
    } else {
      if (pcm_file_name != NULL) {
        printf("error: multiple files are not supported.\n");
        goto exit;
      }
      pcm_file_name = argv[i];
    }
  }

  if (pcm_file_name == NULL || strlen(pcm_file_name) < 5) {
    show_help_message();
    goto exit;
  }

  // input pcm file name and extension
  uint8_t* pcm_file_exp = pcm_file_name + strlen(pcm_file_name) - 4;

  // cached pcm file name
  static uint8_t pcm_cache_file_name[ MAX_PATH_LEN ];

  // input format check
  int32_t pcm_freq = 15625;
  int16_t pcm_channels = 1;
  int16_t pcm_gain = encode_with_self ? 1 : 16; 
  int16_t decode_mode = DECODE_MODE_NONE;
  int16_t encode_mode = ENCODE_MODE_NONE;
  int16_t mp3_cache_use = 0;
  if (stricmp(".pcm", pcm_file_exp) == 0) {
    pcm_freq = 15625;                 // fixed
    pcm_channels = 1;
    decode_mode = DECODE_MODE_NONE;
    encode_mode = ENCODE_MODE_NONE;
    adpcm_output_freq = 15625;        // fixed
  } else if (stricmp(".s32", pcm_file_exp) == 0) {
    pcm_freq = 32000;
    pcm_channels = 2;
    decode_mode = DECODE_MODE_RESAMPLE;
    encode_mode = ENCODE_MODE_SELF;
  } else if (stricmp(".s44", pcm_file_exp) == 0) {
    pcm_freq = 44100;
    pcm_channels = 2;
    decode_mode = DECODE_MODE_RESAMPLE;
    encode_mode = ENCODE_MODE_SELF;
  } else if (stricmp(".s48", pcm_file_exp) == 0) {
    pcm_freq = 48000;
    pcm_channels = 2;
    decode_mode = DECODE_MODE_RESAMPLE;
    encode_mode = ENCODE_MODE_SELF;
  } else if (stricmp(".m32", pcm_file_exp) == 0) {
    pcm_freq = 32000;
    pcm_channels = 1;
    decode_mode = DECODE_MODE_RESAMPLE;
    encode_mode = ENCODE_MODE_SELF;
  } else if (stricmp(".m44", pcm_file_exp) == 0) {
    pcm_freq = 44100;
    pcm_channels = 1;
    decode_mode = DECODE_MODE_RESAMPLE;
    encode_mode = ENCODE_MODE_SELF;
  } else if (stricmp(".m48", pcm_file_exp) == 0) {
    pcm_freq = 48000;
    pcm_channels = 1;
    decode_mode = DECODE_MODE_RESAMPLE;
    encode_mode = ENCODE_MODE_SELF;
  } else if (stricmp(".a32", pcm_file_exp) == 0) {
    pcm_freq = 32000;
    pcm_channels = 2;
    decode_mode = DECODE_MODE_NAS_ADPCM;
    encode_mode = ENCODE_MODE_SELF;
  } else if (stricmp(".a44", pcm_file_exp) == 0) {
    pcm_freq = 44100;
    pcm_channels = 2;
    decode_mode = DECODE_MODE_NAS_ADPCM;
    encode_mode = ENCODE_MODE_SELF;
  } else if (stricmp(".a48", pcm_file_exp) == 0) {
    pcm_freq = 48000;
    pcm_channels = 2;
    decode_mode = DECODE_MODE_NAS_ADPCM;
    encode_mode = ENCODE_MODE_SELF;
  } else if (stricmp(".n32", pcm_file_exp) == 0) {
    pcm_freq = 32000;
    pcm_channels = 2;
    decode_mode = DECODE_MODE_NAS_ADPCM;
    encode_mode = ENCODE_MODE_SELF;
  } else if (stricmp(".n44", pcm_file_exp) == 0) {
    pcm_freq = 44100;
    pcm_channels = 2;
    decode_mode = DECODE_MODE_NAS_ADPCM;
    encode_mode = ENCODE_MODE_SELF;
  } else if (stricmp(".n48", pcm_file_exp) == 0) {
    pcm_freq = 48000;
    pcm_channels = 2;
    decode_mode = DECODE_MODE_NAS_ADPCM;
    encode_mode = ENCODE_MODE_SELF;
  } else if (stricmp(".mp3", pcm_file_exp) == 0) {
    pcm_freq = -1;
    pcm_channels = -1;
    decode_mode = DECODE_MODE_MP3;
    encode_mode = ENCODE_MODE_PCM8A;

    // in case of quarter frequency mode, use 10.417kHz internal ADPCM
    if (mp3_quality == 2) {
      adpcm_output_freq = 10417;
    }

    // check s44/a44 file existence for cache use
    if (!mp3_cache_unuse) {
      struct stat stat_buf;
      strcpy(pcm_cache_file_name, pcm_file_name);
      strcpy(pcm_cache_file_name + strlen(pcm_cache_file_name) - 4, ".s44");
      if (stat(pcm_cache_file_name, &stat_buf) == 0) {
        pcm_freq = 44100;
        pcm_channels = 2;
        decode_mode = DECODE_MODE_RESAMPLE;
        mp3_cache_use = 1;
        mp3_quality = 0;
      } else {
        strcpy(pcm_cache_file_name + strlen(pcm_cache_file_name) - 4, ".a44");
        if (stat(pcm_cache_file_name, &stat_buf) == 0) {
          pcm_freq = 44100;
          pcm_channels = 2;
          decode_mode = DECODE_MODE_NAS_ADPCM;
          mp3_cache_use = 1;
          mp3_quality = 0;
        } else {
          pcm_cache_file_name[0] = '\0';
        }
      }
    }

  } else {
    printf("error: unknown format file (%s).\n", pcm_file_name);
    goto exit;
  }

  // cursor off
  C_CUROFF();

  // set abort vectors
  uint32_t abort_vector1 = INTVCS(0xFFF1, (int8_t*)abort_application);
  uint32_t abort_vector2 = INTVCS(0xFFF2, (int8_t*)abort_application);  

  // enter supervisor mode and init crtc if album art is required
  if (mp3_pic_brightness > 0) {
    B_SUPER(0);
    G_CLR_ON();
    crtc_set_extra_mode(0);
  }

  // determine PCM8 type
  int16_t pcm8_type = PCM8_TYPE_NONE;
  if (pcm8pp_keepchk()) {
    pcm8_type = PCM8_TYPE_PCM8PP;
  } else if (pcm8a_keepchk()) {
    pcm8_type = PCM8_TYPE_PCM8A;
  } else if (pcm8_keepchk()) {
    pcm8_type = PCM8_TYPE_PCM8;
  }

  // can we use PCM8A/PCM8PP for encode?
  if (encode_mode != ENCODE_MODE_NONE && !encode_with_self) {
    if (pcm8_type == PCM8_TYPE_PCM8PP) {
      encode_mode = ENCODE_MODE_PCM8PP;
      if (decode_mode == DECODE_MODE_RESAMPLE) {
        decode_mode = DECODE_MODE_NONE;
      }
      //if (decode_mode == DECODE_MODE_MP3 && mp3_quality == 2) {
      //  printf("error: MP3 with PCM8PP does not support low quality mode.\n");
      //  goto exit;
      //}
    } else if (pcm8_type == PCM8_TYPE_PCM8A) {
      encode_mode = ENCODE_MODE_PCM8A;
    } else {
      if (decode_mode == DECODE_MODE_MP3) {
        printf("error: PCM8A (>=1.02) or PCM8PP (>=0.83d) is required for MP3 playback.\n");
        goto exit;
      } else if (decode_mode == DECODE_MODE_NAS_ADPCM) {
        printf("error: PCM8A (>=1.02) or PCM8PP (>=0.83d) is required for YM2608 ADPCM playback.\n");
        goto exit;
      } else {
        printf("\n<<warning>> PCM8A/PCM8PP is not running. Use MP3EXP for ADPCM encoding.\n");
        encode_mode = ENCODE_MODE_SELF;
        pcm_gain = 1;
      }
    }
  }

  // reset PCM8 / PCM8A / PCM8PP / IOCS ADPCM
  if (pcm8_type != PCM8_TYPE_NONE) {
    pcm8_pause();
    pcm8_stop();
  } else {
    ADPCMMOD(0);
  }

  int16_t first_play = 1;

loop:
  // file read buffer and resample buffer
  void* fread_buffer = NULL;
  void* fread_staging_buffer = NULL;
  void* resample_buffer = NULL;
  FILE* fp = NULL;

try:
  // encoder, decoder and chain tables
  ADPCM_HANDLE adpcm_encoder = { 0 };
  MP3_DECODE_HANDLE mp3_decoder = { 0 };
  NAS_ADPCM_DECODE_HANDLE nas_adpcm_decoder = { 0 };
  static CHAIN_TABLE chain_tables[ MAX_CHAINS ];

  // init adpcm encoder
  if (adpcm_init(&adpcm_encoder, num_chains+1, decode_mode == DECODE_MODE_MP3 && pcm8_type == PCM8_TYPE_PCM8A ? use_high_memory : 0) != 0) {
    printf("error: ADPCM encoder initialization error.\n");
    goto catch;
  }

  // init mp3 decoder
  if (mp3_cache_use || decode_mode == DECODE_MODE_MP3) {
    if (mp3_init(&mp3_decoder) != 0) {
      printf("error: MP3 decoder initialization error.\n");
      goto catch;    
    }
  }

  // init nas adpcm decoder if needed
  if (decode_mode == DECODE_MODE_NAS_ADPCM) {
    if (nas_adpcm_init(&nas_adpcm_decoder, pcm_freq * pcm_channels * 4, pcm_freq, pcm_channels) != 0) {
      printf("error: YM2608 adpcm decoder initialization error.\n");
      goto catch;
    }
  }

  // init chain tables
  for (int16_t i = 0; i < num_chains; i++) {
    chain_tables[i].buffer = adpcm_encoder.buffers[i];
    chain_tables[i].buffer_bytes = 0;
    chain_tables[i].next = &(chain_tables[ ( i + 1 ) % num_chains ]);
  }

  // open pcm/mp3 file
  fp = fopen(pcm_file_name, "rb");
  if (fp == NULL) {
    printf("error: cannot open pcm/mp3 file (%s).\n", pcm_file_name);
    goto catch;
  }

  // read the first 10 bytes of the MP3 file
  size_t skip_offset = 0;
  if (mp3_cache_use || decode_mode == DECODE_MODE_MP3) {
    printf("\rparsing ID3v2 tag and album art...");
    int32_t ofs = mp3_parse_tags(&mp3_decoder, mp3_pic_brightness, mp3_pic_half_size, fp);
    if (ofs < 0) {
      printf("\rerror: ID3v2 tag parse error.\x1b[0K\n");
      goto catch;
    }
    skip_offset = ofs;
  }

  // in case mp3 cache mode, reopen the file
  if (mp3_cache_use) {
    fclose(fp);
    fp = fopen(pcm_cache_file_name, "rb");
    if (fp == NULL) {
      printf("error: cannot open pcm/mp3 file (%s).\n", pcm_file_name);
      goto catch;
    }
    skip_offset = 0;
  }

  // check file size
  fseek(fp, 0, SEEK_END);
  uint32_t pcm_file_size = ftell(fp) - skip_offset;
  fseek(fp, skip_offset, SEEK_SET);

  // allocate file read buffer
  //   mp3 ... full read
  //   pcm ... incremental (max 2 sec)
  size_t fread_buffer_len = 
    decode_mode == DECODE_MODE_MP3 ? 2 + pcm_file_size / sizeof(int16_t) : 
    decode_mode == DECODE_MODE_NAS_ADPCM ? adpcm_encoder.buffer_bytes / 4 :
    pcm_freq * pcm_channels * 2;
  if (encode_mode != ENCODE_MODE_NONE) {
    fread_buffer = himem_malloc(fread_buffer_len * sizeof(int16_t), decode_mode == DECODE_MODE_MP3 ? use_high_memory : 0);
    if (fread_buffer == NULL) {
      printf("\rerror: file read buffer memory allocation error.\n");
      goto catch;
    }
  }

  // allocate resampling buffer
  size_t resample_buffer_len = adpcm_output_freq * 2 + 32;     // max 2 second samples + error allowance
  if (encode_mode != ENCODE_MODE_NONE) {
    resample_buffer = himem_malloc(resample_buffer_len * sizeof(int16_t), use_high_memory);
    if (resample_buffer == NULL) {
      printf("\rerror: resampling buffer memory allocation error.\n");
      goto catch;
    }
  }

  // init mp3 decoder if needed
  if (decode_mode == DECODE_MODE_MP3) {
    // full read with staging buffer
    printf("\rloading MP3...\x1b[0K");
    fread_staging_buffer = himem_malloc(FREAD_STAGING_BUFFER_BYTES, 0);
    if (fread_staging_buffer == NULL) {
      printf("\rerror: file read staging buffer memory allocation error.\n");
      goto catch;
    }    
    size_t read_len = 0; 
    do {
      size_t len = fread(fread_staging_buffer, 1, FREAD_STAGING_BUFFER_BYTES, fp);
      memcpy(fread_buffer + read_len, fread_staging_buffer, len);
      read_len += len;
    } while (read_len < pcm_file_size);
    fclose(fp);
    fp = NULL;
    himem_free(fread_staging_buffer, 0);
    fread_staging_buffer = NULL;
    if (mp3_decode_setup(&mp3_decoder, fread_buffer, pcm_file_size, mp3_quality) != 0) {
      printf("\rerror: MP3 decoder initialization error.\n");
      goto catch;
    }
    printf("\r\x1b[0K");
  }

  // describe PCM attributes
  if (first_play) {

    printf("\n");

    printf("File name     : %s\n", pcm_file_name);
    printf("Data size     : %d [bytes]\n", pcm_file_size);
    printf("Data format   : %s\n", 
      decode_mode == DECODE_MODE_MP3 ? "MP3" : 
      decode_mode == DECODE_MODE_NAS_ADPCM ? "YM2608 ADPCM" :
      encode_mode != ENCODE_MODE_NONE ? "16bit signed PCM (big)" : 
      "ADPCM(MSM6258V)");

    if (decode_mode == DECODE_MODE_NONE) {
      float pcm_1sec_size = pcm_freq * pcm_channels * (encode_mode == ENCODE_MODE_NONE ? 0.5 : 2);
      printf("PCM frequency : %d [Hz]\n", pcm_freq);
      printf("PCM channels  : %s\n", pcm_channels == 1 ? "mono" : "stereo");
      printf("PCM length    : %4.2f [sec]\n", (float)pcm_file_size / pcm_1sec_size);
    }

    if (!mp3_cache_use && decode_mode == DECODE_MODE_NAS_ADPCM) {
      printf("PCM frequency : %d [Hz]\n", pcm_freq);
      printf("PCM channels  : %s\n", pcm_channels == 1 ? "mono" : "stereo");
    }

    // describe PCM drivers
    printf("PCM driver    : %s\n",
      pcm8_type == PCM8_TYPE_PCM8   ? "PCM8"   :
      pcm8_type == PCM8_TYPE_PCM8A  ? "PCM8A"  :
      pcm8_type == PCM8_TYPE_PCM8PP ? "PCM8PP" :
      "MP3EXP");
  
    // describe ADPCM encoding
    if (encode_mode == ENCODE_MODE_NONE || pcm8_type == PCM8_TYPE_PCM8PP) {
      //printf("ADPCM encode  : (none)\n");
    } else {
      printf("ADPCM encode  : %s / %d [Hz]\n",
        encode_mode == ENCODE_MODE_SELF   ? "MP3EXP" :
        encode_mode == ENCODE_MODE_PCM8A  ? "PCM8A"  :
        "(unknown)", adpcm_output_freq);
    }

    // describe MP3 decoding rate
    if (mp3_cache_use || decode_mode == DECODE_MODE_MP3) {
      printf("MP3 quality   : %s\n",
        mp3_quality == 2 ? "low" :
        mp3_quality == 1 ? "normal" : "high");
      if (mp3_decoder.mp3_title != NULL) {
        printf("MP3 title     : %s\n", mp3_decoder.mp3_title);
      }
      if (mp3_decoder.mp3_artist != NULL) {
        printf("MP3 artist    : %s\n", mp3_decoder.mp3_artist);
      }
      if (mp3_decoder.mp3_album != NULL) {
        printf("MP3 album     : %s\n", mp3_decoder.mp3_album);
      }
    }

    printf("\n");

    first_play = 0;
  }

  // initial buffering
  int16_t end_flag = 0;
  for (int16_t i = 0; i < num_chains; i++) {

    if (end_flag) break;

    // check esc key to exit
    if (B_KEYSNS() != 0) {
      int16_t scan_code = B_KEYINP() >> 8;
      if (scan_code == KEY_SCAN_CODE_ESC || scan_code == KEY_SCAN_CODE_Q) {
        printf("\rcanceled.\x1b[0K");
        rc = 1;
        goto catch;
      }
    }

    printf("\rnow buffering (%d/%d) on %s ...", i+1, num_chains, 
      use_high_memory ? "high memory and main memory" : "main memory");

    if (encode_mode == ENCODE_MODE_NONE) {

      // ADPCM through (no encoding)
      size_t fread_len = fread(chain_tables[i].buffer, 1, adpcm_encoder.buffer_bytes, fp);
      if (fread_len < adpcm_encoder.buffer_bytes) {
        chain_tables[i].next = NULL;
        end_flag = 1;
      }
      chain_tables[i].buffer_bytes = fread_len;
  
    } else if (encode_mode == ENCODE_MODE_PCM8PP) {

      if (decode_mode == DECODE_MODE_MP3) {

        // MP3 decode and PCM through (no encoding) with PCM8PP
        size_t decoded_bytes;
        if (mp3_decode_full(&mp3_decoder, chain_tables[i].buffer, adpcm_encoder.buffer_bytes, &decoded_bytes) != 0) {
          printf("\rerror: mp3 decode error.\x1b[0K");
          goto catch;
        }
        chain_tables[i].buffer_bytes = decoded_bytes;
        if (decoded_bytes == 0) {
          chain_tables[i].next = NULL;
          end_flag = 1;
        }

      } else if (decode_mode == DECODE_MODE_NAS_ADPCM) {

        // NAS ADPCM decode and PCM through (no resample, no encoding) with PCM8PP
        size_t fread_len = fread(fread_buffer, 1, fread_buffer_len, fp);
        if (fread_len < fread_buffer_len) {
          chain_tables[i].next = NULL;
          end_flag = 1;
        }
        size_t decoded_bytes =
          nas_adpcm_decode_buffer(&nas_adpcm_decoder, fread_buffer, fread_len, 
            chain_tables[i].buffer, adpcm_encoder.buffer_bytes / sizeof(int16_t)) * sizeof(int16_t);
        chain_tables[i].buffer_bytes = decoded_bytes;

      } else if (decode_mode == DECODE_MODE_RESAMPLE) {

        // PCM to ADPCM encode with PCM8PP
        size_t fread_len = fread(fread_buffer, 2, fread_buffer_len, fp);  
        if (fread_len < fread_buffer_len) {
          chain_tables[i].next = NULL;
          end_flag = 1;
        }
        size_t resampled_len = adpcm_resample(&adpcm_encoder, chain_tables[i].buffer,
                                              fread_buffer, fread_len, pcm_freq, pcm_channels, pcm_gain);
        chain_tables[i].buffer_bytes = resampled_len * sizeof(int16_t);

      } else {

        // PCM through (no encoding) with PCM8PP
        size_t fread_len = fread(chain_tables[i].buffer, 2, adpcm_encoder.buffer_bytes/2, fp);
        if (fread_len < adpcm_encoder.buffer_bytes/2) {
          chain_tables[i].next = NULL;
          end_flag = 1;
        }
        chain_tables[i].buffer_bytes = fread_len * 2;

      }

    } else if (encode_mode == ENCODE_MODE_PCM8A) {

      if (decode_mode == DECODE_MODE_MP3) {

        // MP3 decode and ADPCM encode with PCM8A
        size_t resampled_len;
        if (mp3_decode_resample(&mp3_decoder, chain_tables[i].buffer, adpcm_encoder.buffer_bytes / sizeof(int16_t), adpcm_output_freq, &resampled_len) != 0) {
          printf("\rerror: mp3 decode error.\x1b[0K");
          goto catch;
        }
        chain_tables[i].buffer_bytes = resampled_len * sizeof(int16_t);
        if (resampled_len == 0) {
          chain_tables[i].next = NULL;
          end_flag = 1;
        }

      } else if (decode_mode == DECODE_MODE_NAS_ADPCM) {

        // NAS ADPCM decode and ADPCM encode with PCM8A
        size_t fread_len = fread(fread_buffer, 1, fread_buffer_len * sizeof(int16_t), fp);  
        if (fread_len < fread_buffer_len * sizeof(int16_t)) {
          chain_tables[i].next = NULL;
          end_flag = 1;
        }
        nas_adpcm_decode(&nas_adpcm_decoder, fread_buffer, fread_len);
        size_t resampled_bytes = nas_adpcm_resample(&nas_adpcm_decoder, chain_tables[i].buffer, pcm_gain) * sizeof(int16_t);
        chain_tables[i].buffer_bytes = resampled_bytes;

      } else if (decode_mode == DECODE_MODE_RESAMPLE) {

        // PCM to ADPCM encode with PCM8A
        size_t fread_len = fread(fread_buffer, 2, fread_buffer_len, fp);  
        if (fread_len < fread_buffer_len) {
          chain_tables[i].next = NULL;
          end_flag = 1;
        }
        size_t resampled_len = adpcm_resample(&adpcm_encoder, chain_tables[i].buffer,
                                              fread_buffer, fread_len, pcm_freq, pcm_channels, pcm_gain);
        chain_tables[i].buffer_bytes = resampled_len * sizeof(int16_t);

      } else {
        printf("error: unknown decode mode for PCM8A.");
        goto catch;
      }

    } else {

      // ADPCM self encoding
      int16_t orig_id = adpcm_encoder.current_buffer_id;
      do {
        size_t fread_len = fread(fread_buffer, 2, fread_buffer_len, fp);
        size_t resampled_len = adpcm_resample(&adpcm_encoder, resample_buffer, 
                                              fread_buffer, fread_len, pcm_freq, pcm_channels, pcm_gain);
        adpcm_encode(&adpcm_encoder, resample_buffer, resampled_len * sizeof(int16_t), 16, 1);
        if (fread_len < fread_buffer_len) {
          chain_tables[i].next = NULL;
          end_flag = 1;
          break;
        }
      } while (adpcm_encoder.current_buffer_id == orig_id);     // make sure a buffer is fully fulfilled

      chain_tables[i].buffer = adpcm_encoder.buffers[ orig_id ];
      chain_tables[i].buffer_bytes = (chain_tables[i].next == NULL) ? adpcm_encoder.buffer_ofs : adpcm_encoder.buffer_bytes; 

    }

  }

#ifdef DEBUG
  for (int16_t i = 0; i < num_chains; i++) {
    printf("chain_tables[%d] self=%X,buffer=%X,buffer_bytes=%d,next=%X\n", i, &(chain_tables[i]), chain_tables[i].buffer, chain_tables[i].buffer_bytes, chain_tables[i].next);
  }
#endif

  // start playing
  if (pcm8_type == PCM8_TYPE_PCM8PP) {

    if (decode_mode == DECODE_MODE_MP3) {
      pcm_freq = mp3_decoder.mp3_sample_rate;
      pcm_channels = mp3_decoder.mp3_channels;
#ifdef DEBUG
      printf("mp3 freq=%d,channels=%d\n",pcm_freq,pcm_channels);
#endif      
    }

    int16_t pcm8pp_volume = pcm8_volume;
    int16_t pcm8pp_pan = 0x03;
    int16_t pcm8pp_freq = pcm_freq == 16000 && pcm_channels == 1 ? 0x09 :
                          pcm_freq == 22050 && pcm_channels == 1 ? 0x0a :
                          pcm_freq == 24000 && pcm_channels == 1 ? 0x0b :
                          pcm_freq == 32000 && pcm_channels == 1 ? 0x0c :
                          pcm_freq == 44100 && pcm_channels == 1 ? 0x0d :
                          pcm_freq == 48000 && pcm_channels == 1 ? 0x0e :
                          (pcm_freq == 8000 || pcm_freq == 11025 || pcm_freq == 12000) && pcm_channels == 1 ? 0x0f :
                          pcm_freq == 16000 && pcm_channels == 2 ? 0x19 :
                          pcm_freq == 22050 && pcm_channels == 2 ? 0x1a :
                          pcm_freq == 24000 && pcm_channels == 2 ? 0x1b :
                          pcm_freq == 32000 && pcm_channels == 2 ? 0x1c :
                          pcm_freq == 44100 && pcm_channels == 2 ? 0x1d :
                          pcm_freq == 48000 && pcm_channels == 2 ? 0x1e :
                          (pcm_freq == 8000 || pcm_freq == 11025 || pcm_freq == 12000) && pcm_channels == 2 ? 0x1f :
                          adpcm_output_freq < 8000  ? 0x02 :
                          adpcm_output_freq < 11000 ? 0x03 : 0x04;
    uint32_t pcm8pp_channel_mode = ( pcm8pp_volume << 16 ) | ( pcm8pp_freq << 8 ) | pcm8pp_pan;
    pcm8pp_play_linked_array_chain(0, pcm8pp_channel_mode, 1, pcm_freq * 256, &(chain_tables[0]));

  } else if (pcm8_type == PCM8_TYPE_PCM8A && encode_mode == ENCODE_MODE_PCM8A) {

    // must use polyphonic mode for 16bit PCM use
    pcm8a_set_polyphonic_mode(1);   
 
    int16_t pcm8a_volume = pcm8_volume;
    int16_t pcm8a_pan = 0x03;
    int16_t pcm8a_freq = adpcm_output_freq < 8000  ? 0x12 :
                         adpcm_output_freq < 11000 ? 0x13 : 0x14;
    uint32_t pcm8a_channel_mode = ( pcm8a_volume << 16 ) | ( pcm8a_freq << 8 ) | pcm8a_pan;
    pcm8a_play_linked_array_chain(0, pcm8a_channel_mode, &(chain_tables[0]));

  } else if (pcm8_type == PCM8_TYPE_PCM8 || pcm8_type == PCM8_TYPE_PCM8A) {

    // disable PCM8 polyphonic mode
    pcm8_set_polyphonic_mode(0);    

    int16_t freq = adpcm_output_freq < 8000  ? 2 :
                   adpcm_output_freq < 11000 ? 3 : 4;
    int32_t iocs_adpcm_mode = freq * 256 + 3;
    ADPCMLOT((struct CHAIN2*)(&chain_tables[0]), iocs_adpcm_mode);

  } else {

    // IOCS ADPCM mode
    int16_t freq = adpcm_output_freq < 8000  ? 2 :
                   adpcm_output_freq < 11000 ? 3 : 4;
    int32_t iocs_adpcm_mode = freq * 256 + 3;
    ADPCMLOT((struct CHAIN2*)(&chain_tables[0]), iocs_adpcm_mode);

  }

  printf("\rnow playing ... push [ESC]/[Q] key to stop.\x1b[0K");

  // dummy wait to make sure DMAC start (300 msec)
  for (int32_t t0 = ONTIME(); ONTIME() < t0 + 30;) {}

  int16_t current_chain = 0;
  int32_t pcm8pp_block_counter = 0;
  if (pcm8_type == PCM8_TYPE_PCM8PP) {
    pcm8pp_block_counter = pcm8pp_get_block_counter(0);
  }

  for (;;) {
   
    // check esc key to exit
    if (B_KEYSNS() != 0) {
      int16_t scan_code = B_KEYINP() >> 8;
      if (scan_code == KEY_SCAN_CODE_ESC || scan_code == KEY_SCAN_CODE_Q) {
        printf("\rstopped.\x1b[0K");
        rc = 1;
        break;
      }
    }

    // exit if not playing
    if (encode_mode == ENCODE_MODE_PCM8PP) {
      if (pcm8pp_get_data_length(0) == 0) {
      //if (B_BPEEK(REG_DMAC_CH2_CSR) & 0x80) {   // ch2 dmac operation complete?
        if (end_flag) { 
          printf("\rfinished.\x1b[0K");
          rc = 0;
        } else {
          printf("\rerror: buffer underrun detected.\x1b[0K");
          rc = 1;
        }
        break;
      }
    } else if (encode_mode == ENCODE_MODE_PCM8A) {
      if (pcm8a_get_data_length(0) == 0) {
        if (end_flag) { 
          printf("\rfinished.\x1b[0K");
          rc = 0;
        } else {
          printf("\rerror: buffer underrun detected.\x1b[0K");
          rc = 1;
        }
        break;
      }
    } else {
      if (ADPCMSNS() == 0) {
        if (end_flag) {
          printf("\rfinished.\x1b[0K");
          rc = 0;
        } else {
          printf("\rerror: buffer underrun detected.\x1b[0K");
          rc = 1;
        }
        break;
      }
    }

    // check buffer flip
    CHAIN_TABLE* cta = &(chain_tables[ current_chain ]);
    CHAIN_TABLE* ctb = &(chain_tables[ (current_chain - 1 + num_chains) % num_chains ]);
    int16_t buffer_flip = 0;
    if (pcm8_type == PCM8_TYPE_PCM8PP) {
      int32_t bc = pcm8pp_get_block_counter(0);
      if (bc != pcm8pp_block_counter) {
        buffer_flip = 1;
        pcm8pp_block_counter = bc;
      }
    } else if (pcm8_type == PCM8_TYPE_PCM8A && encode_mode == ENCODE_MODE_PCM8A) {
      void* cur_pcm8a_addr = pcm8a_get_access_address(0);
      if (cur_pcm8a_addr < cta->buffer || cur_pcm8a_addr >= cta->buffer + cta->buffer_bytes) {
        buffer_flip = 1;
#ifdef DEBUG
        printf("pcm8a=%X, ct0 buffer=%X - %X, ct1 buffer=%X - %X, ct2 buffer=%X - %X, ct3 buffer=%X - %X\n", cur_pcm8a_addr, 
        chain_tables[0].buffer, chain_tables[0].buffer + chain_tables[0].buffer_bytes,
        chain_tables[1].buffer, chain_tables[1].buffer + chain_tables[1].buffer_bytes,
        chain_tables[2].buffer, chain_tables[2].buffer + chain_tables[2].buffer_bytes, 
        chain_tables[3].buffer, chain_tables[3].buffer + chain_tables[3].buffer_bytes);
#endif
      }
    } else {
      void* cur_dmac_bar = (void*)B_LPEEK((uint32_t*)REG_DMAC_CH3_BAR);     // = next chain table pointer
      if (cur_dmac_bar != cta->next) {
        buffer_flip = 1;
#ifdef DEBUG
        printf("cur_bar=%X, cta->next=%X\n", cur_dmac_bar, cta->next);
#endif
      }
    }

    // process additional data if buffer flip happens
    if (!end_flag && buffer_flip) {

#ifdef DEBUG
      printf("buffer flip (current chain = %d)\n", current_chain);
#endif
      // cut link tantatively
      void* orig_ctb_next = ctb->next;
      ctb->next = NULL;

      if (encode_mode == ENCODE_MODE_NONE) {

        // ADPCM through (no encoding)
        size_t fread_len = fread(cta->buffer, 1, adpcm_encoder.buffer_bytes, fp);
        if (fread_len < adpcm_encoder.buffer_bytes) {
          cta->next = NULL;
          end_flag = 1;
        }
        cta->buffer_bytes = fread_len;        

      } else if (encode_mode == ENCODE_MODE_PCM8PP) {

        if (decode_mode == DECODE_MODE_MP3) {

          // MP3 decode and PCM through (no encoding) with PCM8PP
          size_t decoded_bytes;
          if (mp3_decode_full(&mp3_decoder, cta->buffer, adpcm_encoder.buffer_bytes, &decoded_bytes) != 0) {
            printf("\rerror: mp3 decode error.\x1b[0K");
            goto catch;
          }
          cta->buffer_bytes = decoded_bytes;
          if (decoded_bytes == 0) {
            cta->next = NULL;
            end_flag = 1;
          }

        } else if (decode_mode == DECODE_MODE_NAS_ADPCM) {

          // NAS ADPCM decode and PCM through (no resample, no encoding) with PCM8PP
          size_t fread_len = fread(fread_buffer, 1, fread_buffer_len, fp);
          if (fread_len < fread_buffer_len) {
            cta->next = NULL;
            end_flag = 1;
          }
          size_t decoded_bytes = 
            nas_adpcm_decode_buffer(&nas_adpcm_decoder, fread_buffer, fread_len, 
              cta->buffer, adpcm_encoder.buffer_bytes / sizeof(int16_t)) * sizeof(int16_t);
          cta->buffer_bytes = decoded_bytes;

        } else if (decode_mode == DECODE_MODE_RESAMPLE) {

          // ADPCM encoding with PCM8PP
          size_t fread_len = fread(fread_buffer, 2, fread_buffer_len, fp);  
          if (fread_len < fread_buffer_len) {
            cta->next = NULL;
            end_flag = 1;
          }
          size_t resampled_len = adpcm_resample(&adpcm_encoder, cta->buffer, 
                                                fread_buffer, fread_len, pcm_freq, pcm_channels, pcm_gain);
          cta->buffer_bytes = resampled_len * sizeof(int16_t);

        } else {

          // PCM through (no encoding) with PCM8PP
          size_t fread_len = fread(cta->buffer, 1, adpcm_encoder.buffer_bytes, fp);
          if (fread_len < adpcm_encoder.buffer_bytes) {
            cta->next = NULL;
            end_flag = 1;
          }
          cta->buffer_bytes = fread_len;

        }

      } else if (encode_mode == ENCODE_MODE_PCM8A) {

        if (decode_mode == DECODE_MODE_MP3) {

          // MP3 decode and ADPCM encode
          size_t resampled_len;
          if (mp3_decode_resample(&mp3_decoder, cta->buffer, adpcm_encoder.buffer_bytes / sizeof(int16_t), adpcm_output_freq, &resampled_len) != 0) {
            printf("\rerror: mp3 decode error.\x1b[0K");
            goto catch;
          }
          cta->buffer_bytes = resampled_len * sizeof(int16_t);
          if (resampled_len == 0) {
            cta->next = NULL;
            end_flag = 1;
          }

        } else if (decode_mode == DECODE_MODE_NAS_ADPCM) {

          // NAS ADPCM decode and ADPCM encode with PCM8A
          size_t fread_len = fread(fread_buffer, 1, fread_buffer_len, fp);  
          if (fread_len < fread_buffer_len) {
            cta->next = NULL;
            end_flag = 1;
          }
          nas_adpcm_decode(&nas_adpcm_decoder, fread_buffer, fread_len);
          size_t resampled_bytes = nas_adpcm_resample(&nas_adpcm_decoder, cta->buffer, pcm_gain) * sizeof(int16_t);
          cta->buffer_bytes = resampled_bytes;

        } else {

          // ADPCM encoding with PCM8A
          size_t fread_len = fread(fread_buffer, 2, fread_buffer_len, fp);  
          if (fread_len < fread_buffer_len) {
            cta->next = NULL;
            end_flag = 1;
          }

          size_t resampled_len = adpcm_resample(&adpcm_encoder, cta->buffer, 
                                                fread_buffer, fread_len, pcm_freq, pcm_channels, pcm_gain);
          cta->buffer_bytes = resampled_len * sizeof(int16_t);
        }

      } else {

        // ADPCM self encoding
        int16_t orig_id = adpcm_encoder.current_buffer_id;
        do {
          size_t fread_len = fread(fread_buffer, 2, fread_buffer_len, fp);
          size_t resample_len = adpcm_resample(&adpcm_encoder, resample_buffer, 
                                                fread_buffer, fread_len, pcm_freq, pcm_channels, pcm_gain);
          adpcm_encode(&adpcm_encoder, resample_buffer, resample_len * sizeof(int16_t), 16, 1);
          if (fread_len < fread_buffer_len) {
            cta->next = NULL;
            end_flag = 1;
            break;
          }
        } while (adpcm_encoder.current_buffer_id == orig_id);

        cta->buffer = adpcm_encoder.buffers[ orig_id ];
        cta->buffer_bytes = (cta->next == NULL) ? adpcm_encoder.buffer_ofs : adpcm_encoder.buffer_bytes; 

      }

      // resume link
      ctb->next = orig_ctb_next;

      // increment focus chain
      current_chain = ( current_chain + 1 ) % num_chains;

    }

  }

catch:
  // reset ADPCM
  if (pcm8_type == PCM8_TYPE_PCM8PP && encode_mode == ENCODE_MODE_PCM8PP) {
    pcm8pp_pause();
    pcm8pp_stop();
  } else if (pcm8_type == PCM8_TYPE_PCM8A && encode_mode == ENCODE_MODE_PCM8A) {
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
    himem_free(resample_buffer, use_high_memory);
    resample_buffer = NULL;
  }
  if (fread_staging_buffer != NULL) {
    himem_free(fread_staging_buffer, 0);
    fread_staging_buffer = NULL;
  }
  if (fread_buffer != NULL) {
    himem_free(fread_buffer, decode_mode == DECODE_MODE_MP3 ? use_high_memory : 0);
    fread_buffer = NULL;
  }

  // close adpcm encoder
  adpcm_close(&adpcm_encoder);

  // close mp3 decoder
  if (decode_mode == DECODE_MODE_MP3) {
    mp3_close(&mp3_decoder);
  }

  // close nas adpcm decoder
  if (decode_mode == DECODE_MODE_NAS_ADPCM) {
    nas_adpcm_close(&nas_adpcm_decoder);
  }

  // enable pcm8 polyphonic mode
  if (pcm8_type != PCM8_TYPE_NONE) {
    if (pcm8_set_polyphonic_mode(-1) == 0) {
      pcm8_set_polyphonic_mode(1);
    }
  }

  // loop check
  if (rc == 0) {
    if (loop_count == 0 || --loop_count > 0) goto loop;
  }

  printf("\n");

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
