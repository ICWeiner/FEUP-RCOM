#ifndef MACROS_H
#define MACROS_H

#define FLAG (0x7e)
#define ESCAPE (0x7d)
#define ESCAPE_FLAG (0x5e)
#define ESCAPE_ESCAPE (0x5d)

#define ADR_TX (0x03)
#define ADR_RX (0x01)

#define CTRL_SET (0x03)
#define CTRL_DISC (0x0b)
#define CTRL_UA (0x07)
#define CTRL_RR(R) (R%2?0b10000101:0b00000101)
#define CTRL_REJ(R) (R%2?0b10000001:0b00000001)
#define CTRL_DATA(S) (S%2?0b01000000:0b00000000)

#define PACKET_SIZE (1024)
#define PACKET_SIZE_LIMIT 256

#define CONTROL_START (0x02)
#define CONTROL_END (0x03)
#define CONTROL_DATA (0x01)
#define TYPE_FILESIZE (0)



typedef enum
{
     SMSTART,
     SMFLAG,
     SMADR,
     SMCTRL,
     SMBCC1,
     SMDATA,
     SMESC,
     SMBCC2,
     SMEND,
     SMREJ
} state;

#endif