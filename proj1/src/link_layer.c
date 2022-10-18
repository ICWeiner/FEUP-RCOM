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

int set_as_transmitter() {
    unsigned char SET[5] = {FLAG, ADDRESS_T, CONTROL_T, BCC_T, FLAG}, elem, frame[5];
    int res, frame_length = 0, state = S0;
	
    (void) signal(SIGALRM, alarm_handler);
    
    while(connectionParameters_ptr->nRetransmissions > alarmCount) {
        res = write(fd, SET, 5);

        alarm(connectionParameters_ptr->timeout);
        alarmCount = 0;
        alarmEnabled =TRUE;

        //Wait for UA signal.

        while(alarmEnabled == TRUE && STOP == FALSE) {
            res = read(fd, &elem, 1);

            if(res < 0){
                STOP = TRUE;
                return -1;
            }       		
            if(res > 0) {
                frame_length++;
          		state_handler(elem, &state, frame, &frame_length, FRAME_S);
       		}
      }
  }

  if(ERROR_FLAG == FALSE)
     return FALSE;

  else
    return TRUE;
}

int set_as_receiver() {
    unsigned char UA[5] = {FLAG, ADDRESS_T, CONTROL_R, BCC_R, FLAG}, elem, frame[5];
    int res, frame_length = 0, state = S0;

    while(STOP == FALSE) {
        res = read(fd, &elem, 1);

        if(res > 0) {
		    frame_length++;
            state_handler(elem, &state, frame, &frame_length, FRAME_S);
      }
    }

	res = write(fd, UA, 5);

	return TRUE;
}

unsigned char* stuffing(unsigned char* message, int* length){
    unsigned int array_length = *length;
    unsigned char* str = malloc(array_length * sizeof(unsigned char));
    int j = 0;

    for(int i = 0; i < *length; i++, j++){
        if(j >= array_length) {
            array_length*=2;
            str = realloc(str, array_length * sizeof(unsigned char));
        }

        if(message[i] == ESC) {
			str[j] = ESC;
			str[j + 1]= 0x5d;
			j++;
		}

		else if(message[i] ==  0x7e) {
			str[j] = ESC;
			str[j + 1] = 0x5e;
			j++;
		}

		else{
            str[j] = message[i];
        }
    }
    *length = j;
    
    free(message);

    return str;
}

unsigned char* destuffing(unsigned char* message, int* length) {
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
}

unsigned char* frame_header(unsigned char* stuffed_frame, int* length){
    unsigned char* full_frame = malloc((*length + 5) * sizeof(unsigned char));

    full_frame[0] = FLAG;
    full_frame[1] = ADDRESS_T;
    full_frame[2] = 0;
    full_frame[3] = full_frame[1] ^ full_frame[2];

    for(int i = 0; i < *length; i++){
        full_frame[i+4] = stuffed_frame[i];
    }

    full_frame[*length + 4] = FLAG;
    *length +=5;

    free(stuffed_frame);

    return full_frame;
}


unsigned char* create_frame(const unsigned char* buf, int* bufSize) {
	unsigned char BCC2 = 0x00, *new_message = malloc((*bufSize + 1) * sizeof(unsigned char));
	
    
    for(int i = 0; i < *bufSize; i++) {
		new_message[i] = buf[i];
		BCC2 ^= buf[i];
	}
	
    new_message[*bufSize] = BCC2;
	*bufSize += 1;
	

	unsigned char* stuffed_message = stuffing(new_message, bufSize), *control_message = frame_header(stuffed_message, bufSize);

	return control_message;
}


unsigned char* remove_supervision_frame(unsigned char* message, int* length) {
    unsigned char* control_message = malloc((*length -5) *sizeof(unsigned char));

    for (int i = 4, j = 0; i < *length; i++ , j++){
        control_message[j] = message[i];
    }

    *length +=5;

    free(message);

    return control_message;   
}

unsigned char* BCC2(unsigned char* control_message, int* length){
    unsigned char control_BCC2 = 0x00, *destuffed_message = destuffing(control_message,length);

    for(int i = 0; i < *length - 1; i++){
        control_BCC2^= destuffed_message[i];
    }

    if(destuffed_message[*length - 1] != control_BCC2){
        *length = -1;
        return '\0';
    }

    *length = -1;
    unsigned char* data_message = malloc(*length * sizeof(unsigned char));

    for(int i = 0; i < *length; i++){
        data_message[i] = destuffed_message[i];
    }

    free(destuffed_message);

    return data_message;
}

int send_RR_REJ(int fd, unsigned int type, unsigned char c){
    unsigned char bool_val, response[5];
    
    response[0] = FLAG;
    response[1] = ADDRESS_T;
    response[4] = FLAG;

    if( c == 0x00){
        bool_val = FALSE;
    }
    else{
        bool_val = TRUE;
    }

    switch (type){
        case RR:
            response[2] = control_values[(bool_val ^ 1) + 2]; //what is this?
            break;
        case REJ:
            response[2] = control_values[bool_val + 4]; //ditto
            break;
    }

    response[3] = response[1] ^ response[2];

    write(fd,response,5);

    return bool_val ^ 1;

}

unsigned char* send_DISC(){
    unsigned char elem, *frame = malloc(5 * sizeof(unsigned char)), disc[5] = {FLAG,ADDRESS_T,DISC,ADDRESS_T ^DISC, FLAG};
    int res, frame_length = 0, state = S0;

    alarmCount = 1;
    alarmEnabled = TRUE;
    ERROR_FLAG = FALSE;
    STOP = FALSE;

    while(alarmEnabled == TRUE && connectionParameters_ptr->nRetransmissions > alarmCount){
        res = write(fd,disc,5);
        alarm(connectionParameters_ptr->timeout);
        alarmEnabled = FALSE;

        while (alarmEnabled == FALSE && STOP == FALSE){
            res = read(fd,&elem,1);

            if (res > 0){
                frame_length++;
                state_handler(elem,&state,frame,&frame_length,FRAME_S);
            }
        }
    }
    return frame;
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