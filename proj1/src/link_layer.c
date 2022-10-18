// Link layer protocol implementation
#include "link_layer.h"
#include "macros.h"
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source

typedef struct {
    state curr_state;
    unsigned char adr;
    unsigned char ctrl;
    unsigned char bcc;
    unsigned char *data;
    unsigned int data_size;
} State;


struct termios oldtio;
struct termios newtio;
LinkLayer connectionParameters;
State stateData;
volatile int STOP; //do i still need this?
int alarmCount = 0; // current amount of tries
int alarmEnabled = FALSE;
int ERROR_FLAG = FALSE;
int DISCreceived = FALSE;
unsigned char buf[512];
int fd;
unsigned char DATA_S_FLAG = 0; //this could probably be an int


void alarm_handler() {
    alarmEnabled = FALSE;
    alarmCount++;

    //printf("Alarm #%d\n", alarmCount);
}

void state_handler(unsigned char byte,State* stateData){
    switch (stateData -> curr_state){
        case SMREJ:
        case SMEND:
            stateData -> curr_state=SMSTART;
        case SMSTART:
            if(byte == FLAG){
                stateData ->curr_state = SMFLAG;
            }
            break;
        case SMFLAG:
            if(byte == ADR_TX || byte == ADR_RX){
                stateData->curr_state=SMADR;
                stateData->adr=byte;
            }
            else if(byte == FLAG){
                //do nothing, because we want to remain in the same state, should probably remove this condition...
            }else{
                stateData=SMSTART;
            }
            break;
        case SMADR:
            if(byte == CTRL_UA     || byte == CTRL_SET
            || byte == CTRL_DISC   || byte == CTRL_REJ(0)
            || byte == CTRL_REJ(1) || byte == CTRL_DATA(0)
            || byte == CTRL_DATA(1)|| byte == CTRL_RR(0)
            || byte == byte == CTRL_RR(1)){
                stateData->curr_state =SMCTRL;
                stateData->ctrl = byte;
                stateData->bcc = stateData->adr ^ stateData->ctrl;
            }
            else if (byte == FLAG){
                stateData->curr_state=SMFLAG;
            }else {
                stateData->curr_state=SMSTART;
            }
            break;    
        case SMCTRL:
            if(byte == stateData->bcc){
                stateData->curr_state=SMBCC1;
            }
            else if (byte == FLAG){
                stateData->curr_state=SMFLAG;
            }else{
                stateData->curr_state=SMSTART;
            }
            break;
        case SMBCC1:
            if(byte==FLAG){
                if(stateData->ctrl==CTRL_DATA(0) || stateData->ctrl==CTRL_DATA(0) ){
                    stateData->curr_state=SMFLAG;
                }else{
                    stateData->curr_state=SMEND;
                }
            }else if( stateData->ctrl == CTRL_DATA(0) || stateData->ctrl == CTRL_DATA(1)){
                if(stateData->curr_state != NULL){
                    stateData->data_size = 0;
                    if(byte == ESCAPE){
                        stateData->curr_state = SMESC;
                        stateData->bcc = 0;
                    }else{
                        stateData->data[stateData->data_size++] = byte;
                        stateData->bcc = byte;
                        stateData->curr_state = SMDATA;
                    }
                }
            }else{
                stateData->curr_state = SMSTART;
            }
            break;
        case SMDATA:
            if(byte == ESCAPE){
                stateData->curr_state = SMESC;
            }else if(byte = FLAG){
                stateData->curr_state = SMREJ;
            }else if(byte == stateData->bcc){
                stateData->curr_state = SMBCC2;
            }else{
                stateData->data[stateData->data_size++] = byte;
                stateData->bcc^=byte;
            }
            break;
        case SMESC:
            if(byte == FLAG){
                stateData ->curr_state = SMREJ; 
            }else if(byte ==ESCAPE_FLAG){
                if(stateData->bcc==FLAG){
                    stateData->curr_state=SMBCC2;
                }else{
                    stateData->bcc^=FLAG;
                    stateData->data[stateData->data_size++]=FLAG;
                    stateData->curr_state=SMDATA;
                }
            }else if(byte == ESCAPE_ESCAPE){
                if(stateData->bcc == ESCAPE){
                    stateData->curr_state = SMBCC2;
                }else{
                    stateData->bcc^=ESCAPE;
                    stateData->data[stateData->data_size++] = ESCAPE;
                    stateData->curr_state = SMDATA;
                }
            }else{
                stateData->curr_state=SMSTART;
            }
            break;
        case SMBCC2:
            if(byte == FLAG){
                stateData->curr_state=SMEND;
            }else if( byte == 0){
                stateData->data[stateData->data_size++]=stateData->bcc;
                stateData->bcc = 0;
            }else{
                stateData->data[stateData->data_size++]=stateData->bcc;
                stateData->data[stateData->data_size++]=byte;
                stateData->bcc=byte;
                stateData->curr_state=SMDATA;
            }
            break;
    }
}

int stuffing(const unsigned char* message, int length, unsigned char* dest, unsigned char *bcc){
    int dest_length = 0;

    for(int i = 0; i < length; i++){
        *bcc^=message[i];
        if(message[i] == ESCAPE) {
			dest[dest_length++] = ESCAPE;
			dest[dest_length++]= ESCAPE_ESCAPE;
			break;
		}

		else if(message[i] ==  FLAG) {
			dest[dest_length++] = ESCAPE;
			dest[dest_length++] = ESCAPE_FLAG;
			break;
		}else{
            dest[dest_length++] = message[i];
        }
    }

    return dest_length;
}

unsigned char* destuffing(unsigned char* message, int* length) {/*REDO THIS TO BE IN LINE WITH NEW stuffing function
    unsigned int array_length = 133;
    unsigned char* str = malloc(array_length * sizeof(unsigned char));
    int new_length = 0;

    for(int i = 0; i < *length; i++){
        new_length++;

        if(new_length >= array_length) {
            array_length *= 2;
            str = realloc(str, array_length * sizeof(unsigned char));
        }

        if(message[i] == ESC) {
            if(message[i + 1] == 0x5d){
                str[new_length - 1] = ESC;
                i++;
            }

            else if(message[i + 1] == 0x5e){
                str[new_length - 1] = FLAG;
                i++;
            }
        }
        else{
            str[new_length - 1] = message[i];
        }
        
    }
    *length = new_length;

    free(message); 
    
    return str;
    */
   return -1;
}




////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llopen(LinkLayer connectionParameters)
{
    // TODO:check functionality
    fd = open(connectionParameters.serialPort, O_RDWR | O_NOCTTY);
    int result = FALSE;//if this var is false, it means we failed to set the mode
    connectionParameters_ptr = &connectionParameters;
    controlValue = 0;

    if (fd < 0)
    {
        perror(connectionParameters.serialPort);
        exit(-1);
    }

    struct termios newtio;

    // Save current port settings
    if (tcgetattr(fd, &oldtio) == -1)
    {
        perror("tcgetattr");
        exit(-1);
    }

    // Clear struct for new port settings
    bzero(&newtio, sizeof(newtio));

    newtio.c_cflag = connectionParameters.baudRate | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    // Set input mode (non-canonical, no echo,...)
    newtio.c_lflag = 0;

    newtio.c_cc[VTIME] = 1; // Inter-character timer 
    newtio.c_cc[VMIN] = 0;  // Blocking read until 1 chars received
    
    tcflush(fd, TCIOFLUSH);

    // Set new port settings
    if (tcsetattr(fd, TCSANOW, &newtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    if(connectionParameters.role == LlTx) //set as transmitter
        result = set_as_transmitter(&fd);
    else if (connectionParameters.role ==LlRx)//set as receiver
        result = set_as_receiver(&fd);

    if(result == TRUE)
        return fd;
    else{//FAILED TO SET MODE, close attempted connection
        llclose(0);
        return -1;
    }

    return fd;
}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(const unsigned char *buf, int bufSize){
    unsigned char* full_message = create_frame(buf, &bufSize), elem, frame[5];
    int res, frame_length = 0, state = S0;

    if(bufSize < 0)
        return FALSE;

    alarmCount = 1;
    alarmEnabled = TRUE;
    ERROR_FLAG = FALSE;
    STOP = FALSE;

    while (alarmEnabled == TRUE && connectionParameters_ptr->nRetransmissions > alarmCount){
        res = write(fd,full_message, bufSize);
        alarm(connectionParameters_ptr->timeout);
        alarmEnabled = FALSE;

        //Wait for response

        while( alarmEnabled == FALSE && STOP == FALSE){
        res = read(fd,&elem, bufSize);

            if(res > 0){
                frame_length++;
                state_handler(elem, &state,frame,&frame_length,FRAME_S);
                }
            }

        if (STOP == TRUE){
            if(control_values[controlValue + 4] == frame[2]){
                alarmEnabled = TRUE;
                alarmCount = 0;
                ERROR_FLAG = FALSE;
                STOP = FALSE;
                state = S0;
                frame_length = 0;
            }
        }
    }

    if (ERROR_FLAG == TRUE)
        return FALSE;

    return TRUE;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet){
    unsigned int message_array_length = 138;
    unsigned char elem, *message = malloc(message_array_length * sizeof(unsigned char)), *finish = malloc(sizeof(unsigned char));

    int res, state = S0,lenght = 0;

    ERROR_FLAG = FALSE;
    STOP = FALSE;
    finish[0] = DISC;

    while(STOP == FALSE){
        res = read(fd, &elem,1);

        if(res > 0){
            lenght++;

            if(message_array_length < lenght){
                message_array_length *= 2;
                message = realloc(message, message_array_length * sizeof(unsigned char));
            }

            state_handler(elem,&state,message,lenght,FRAME_I);
        }
    }

    if (message[4] == ADDRESS_R && ERROR_FLAG == FALSE){
        //random error functions
    }

    if(ERROR_FLAG == TRUE || message[2] == CONTROL_T || message == CONTROL_R){
        return '\0';
    }

    if (message[2] == DISC){
        llclose(0);
        return finish;
    }

    return 0;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics){
    //unsigned char*  received;
    unsigned char UA[5] = {FLAG, ADDRESS_T, CONTROL_R, BCC_R, FLAG};
    //unsigned char* received;
    if(connectionParameters_ptr->role == LlTx ){
        //received = send_DISC(fd);
        send_DISC(fd);
        write(fd, UA, 5);
        sleep(1);
    }else if(connectionParameters_ptr->role == LlRx){
        //received = send_DISC(fd);
        send_DISC(fd);
    }
    
    // Wait until all bytes have been written to the serial port
    sleep(1);

    // Restore the old port settings
    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    return close(fd);
}