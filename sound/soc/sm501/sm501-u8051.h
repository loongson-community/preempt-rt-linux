#ifndef _SM501_U8051_h
#define _SM501_U8051_h

#define SET_TX_SLOT_0                    0
#define SET_TX_SLOT_1                    1
#define SET_TX_SLOT_2                    2
#define SET_PLAYBACK_BUFFER_ADDRESS      3
#define SET_PLAYBACK_BUFFER_SIZE         4
#define SET_PLAYBACK_BUFFER_READY        5
#define GET_BUFFER_STATUS                6
#define START_STOP_AUDIO_PLAYBACK        7
#define GET_RX_SLOT0                     8
#define GET_RX_SLOT1                     9
#define GET_RX_SLOT2                    10
#define SET_CAPTURE_BUFFER_ADDRESS      11
#define SET_CAPTURE_BUFFER_SIZE         12
#define SET_CAPTURE_BUFFER_EMPTY        13
#define START_STOP_AUDIO_CAPTURE        14
#define SET_GET_AC97_REGISTER           15

typedef struct
{
    unsigned char firmware[0x3000];                     /* 0x0000 */
    union
    {
        struct
        {
            unsigned char playback_buffer_0[0x0300];    /* 0x3000 */
            unsigned char playback_buffer_1[0x0300];    /* 0x3600 */
        }B;
        struct
        {
            unsigned char filler[0x0600];               /* 0x3000 */
            unsigned char capture_buffer_0[0x0300];     /* 0x3600 */
            unsigned char capture_buffer_1[0x0300];     /* 0x3900 */
        }C;
    }A;
    unsigned char filler1[0x0200];                      /* 0x3C00 */
    unsigned short ac97_regs[64];                       /* 0x3E00 */
    unsigned char filler2[0x0170];                      /* 0x3E80 */
    unsigned char command_byte;                         /* 0x3FF0 */
    unsigned char status_byte;                          /* 0x3FF1 */
    unsigned char data_byte[8];                         /* 0x3FF2 */
    unsigned char filler3[2];                           /* 0x3FFA */
    unsigned char buffer_status;                        /* 0x3FFC */
    unsigned char command_busy;                         /* 0x3FFD */
    unsigned char filler4;                              /* 0x3FFE */
    unsigned char init_count;                           /* 0x3FFF */
}
__attribute__((packed)) FIRMWARE_SRAM;

extern int sm501_write_command(struct sm501_audio *s, unsigned char command, unsigned int data0, unsigned int data1);
extern int __devinit sm501_load_firmware(struct sm501_audio *s, const char* firmware);

#endif
