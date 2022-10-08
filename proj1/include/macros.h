#ifndef MACROS_H
#define MACROS_H

#define FALSE 0
#define TRUE 1

#define RECEIVER 0
#define TRANSMITTER 1

#define FLAG 0x7e //FLAG used to set limits on the frame (begin,end)
#define ESC 0x7d // Escape character used if a FLAG is found inside the frame
#define ADDRESS_T 0x03
#define ADDRESS_R 0x01
#define CONTROL_T 0x03
#define CONTROL_R 0x07
#define BCC_T 0x00
#define BCC_R 0x04

#define CONTROL_START 2
#define CONTROL_END 3

#define FRAME_S 0
#define FRAME_I 1

#define REJ 0
#define RR 1

#define DISC 0x0b

#define ERROR_PERCENTAGE_BCC1 0
#define ERROR_PERCENTAGE_BCC2 0

#define S0 0
#define S1 1
#define S2 2
#define S3 3
#define S4 4
#define SESC 5

#endif