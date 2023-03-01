#ifndef __H_MP3EXP__
#define __H_MP3EXP__

#define VERSION "0.8.1 (2023/03/01)"

#define REG_DMAC_CH2_CSR (0xE84080 + 0x00)
#define REG_DMAC_CH3_BAR (0xE840C0 + 0x1C)

#define MAX_PATH_LEN (256)

#define MAX_CHAINS (128)

#define FREAD_STAGING_BUFFER_BYTES (65536*4)

#define PCM8_TYPE_NONE    (0)
#define PCM8_TYPE_PCM8    (1)
#define PCM8_TYPE_PCM8A   (2)
#define PCM8_TYPE_PCM8PP  (3)

#define DECODE_MODE_NONE      (0)
#define DECODE_MODE_RESAMPLE  (1)
#define DECODE_MODE_MP3       (2)
#define DECODE_MODE_NAS_ADPCM (3)

#define ENCODE_MODE_NONE   (0)
#define ENCODE_MODE_SELF   (1)
#define ENCODE_MODE_PCM8A  (2)
#define ENCODE_MODE_PCM8PP (3)

typedef struct {
    void* buffer;
    uint16_t buffer_bytes;
    void* next;
} CHAIN_TABLE;

#endif