#ifndef __H_MP3EXP__
#define __H_MP3EXP__

#define VERSION "0.1.0 (2023/02/13)"

#define REG_DMAC_CH3_BAR ((volatile uint32_t*)(0xE840C0 + 0x1C))

#define ENCODE_MODE_NONE  (0)
#define ENCODE_MODE_SELF  (1)
#define ENCODE_MODE_PCM8A (2)

#define PCM8_MODE_NONE  (0)
#define PCM8_MODE_PCM8  (1)
#define PCM8_MODE_PCM8A (2)

#define NUM_CHAINS (4)

typedef struct {
    void* buffer;
    uint16_t buffer_bytes;
    void* next;
} CHAIN_TABLE;

#endif