#ifndef __H_MP3EXP__
#define __H_MP3EXP__

#define VERSION "0.9.5 (2023/03/13)"

#define REG_DMAC_CH2_CSR (0xE84080 + 0x00)
#define REG_DMAC_CH3_BAR (0xE840C0 + 0x1C)

#define MAX_PATH_LEN (256)

#define MAX_CHAINS (128)
#define CHAIN_TABLE_BUFFER_BYTES (0xff00)

#define FREAD_STAGING_BUFFER_BYTES (65536*4)

#define PCM8_TYPE_NONE    (0)
#define PCM8_TYPE_PCM8    (1)
#define PCM8_TYPE_PCM8A   (2)
#define PCM8_TYPE_PCM8PP  (3)

#define DRIVER_MP3EXP  (0)
#define DRIVER_PCM8A   (1)
#define DRIVER_PCM8PP  (2)

#define FORMAT_ADPCM   (0)
#define FORMAT_RAW     (1)
#define FORMAT_YM2608  (2)
#define FORMAT_WAV     (3)
#define FORMAT_MP3     (4)

typedef struct {
    void* buffer;
    uint16_t buffer_bytes;
    void* next;
} CHAIN_TABLE;

#endif