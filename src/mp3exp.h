#ifndef __H_MP3EXP__
#define __H_MP3EXP__

#define VERSION "0.3.0 (2023/02/16)"

#define DECODE_MODE_NONE  (0)
#define DECODE_MODE_MP3   (1)

#define ENCODE_MODE_NONE  (0)
#define ENCODE_MODE_SELF  (1)
#define ENCODE_MODE_PCM8A (2)

#define PCM8_MODE_NONE  (0)
#define PCM8_MODE_PCM8  (1)
#define PCM8_MODE_PCM8A (2)

#define MAX_CHAINS (32)

typedef struct {
    void* buffer;
    uint16_t buffer_bytes;
    void* next;
} CHAIN_TABLE;

#endif