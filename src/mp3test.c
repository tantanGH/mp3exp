#include <stdio.h>
#include <stdint.h>
#include "memory.h"
#include "mp3.h"

#define FREAD_BUFFER_LEN (3000000)
#define RESAMPLE_BUFFER_LEN (65280/2)

int32_t main(int32_t argc, uint8_t* argv[]) {

  void* fread_buffer = NULL;
  int16_t* resample_buffer = NULL;
  FILE* fp = NULL;
  MP3_DECODE_HANDLE decoder = { 0 }; 

  fp = fopen("hunterv5.mp3","rb");
  if (fp == NULL) {
    printf("error: mp3 file open error.\n");
    goto catch;
  }

  size_t skip_offset = 0;
  uint8_t mp3_header[10];
  size_t ret = fread(mp3_header, 1, 10, fp);
  if (ret != 10) {
    printf("error: cannot read mp3 file.\n");
    goto catch;
  }

  // check if the MP3 file has an ID3v2 tag
  if (mp3_header[0] == 'I' && mp3_header[1] == 'D' && mp3_header[2] == '3') {
    // Extract the tag size
    uint32_t tag_size = ((mp3_header[6] & 0x7f) << 21) | ((mp3_header[7] & 0x7f) << 14) |
                        ((mp3_header[8] & 0x7f) << 7) | (mp3_header[9] & 0x7f);

    skip_offset = tag_size + 10;
  }

  fseek(fp, 0, SEEK_END);
  size_t mp3_len = ftell(fp) - skip_offset;
  fseek(fp, skip_offset, SEEK_SET);

  fread_buffer = malloc_himem(mp3_len,1);
  resample_buffer = malloc_himem(RESAMPLE_BUFFER_LEN*sizeof(int16_t),1);

  size_t fread_len = 0;
  do {
    size_t len = fread(fread_buffer + fread_len, 1, mp3_len - fread_len, fp);
    fread_len += len;
  } while (fread_len < mp3_len);

  mp3_init(&decoder, fread_buffer, fread_len);

  int16_t end_flag = 0;
  size_t total_size = 0;

  while (!end_flag) {
  
    size_t resampled_len;
    mp3_decode(&decoder, resample_buffer, RESAMPLE_BUFFER_LEN, 15625, 16, &resampled_len);

    if (resampled_len == 0) break;

    printf("resampled %d bytes\n",resampled_len*sizeof(int16_t));

  }

  fclose(fp);
  fp = NULL;

  printf("total MP3 size = %d bytes\n", total_size);

catch:
  mp3_close(&decoder);
  if (fp != NULL) {
    fclose(fp);
    fp = NULL;
  }
  if (fread_buffer != NULL) {
    free_himem(fread_buffer, 1);
  }
  if (resample_buffer != NULL) {
    free_himem(resample_buffer, 1);
  }

exit:
  return 0;
}