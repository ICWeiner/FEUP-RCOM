// Link layer protocol implementation
#include "link_layer.h"
#include "macros.h"
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source

struct termios oldtio;
LinkLayer *connectionParameters_ptr;
volatile int STOP;
int alarmEnabled = FALSE;
int alarmCount = 0; // current amount of trie
int ERROR_FLAG = FALSE;
int fd;
int controlValue;
unsigned char control_values[] = {0x00, 0x40, 0x05, 0x85, 0x01, 0x81};

void alarm_handler() {
    alarmEnabled = FALSE;
    alarmCount++;

    //printf("Alarm #%d\n", alarmCount);
}

void state_handler(unsigned char c,int* state, unsigned char* frame, int *length, int frame_type){
    switch (*state){
    case S0://START state, set to next state if FLAG received
        if(c == FLAG){
            *state = S1;
            frame[*length - 1] = c; // if state == s1 then its less 1 state to read
        }
        break;
    case S1://FLAG_RCV state, set to next state if ack received
        if(c != FLAG) {
            frame[*length - 1] = c;
            if(*length == 4) {
                if ((frame[1] ^ frame[2]) == frame[3])//check if UA received
                    *state = S2;
                 else 
                    *state = SESC; // state escape
                }
            else{
                *length = 1;
                frame[*length - 1] = c;
                }
            }
        break;
    case S2://A_RCV state, set to next state if control received
        frame[*length - 1] = c;

        if(c == FLAG){
            STOP = TRUE;
            alarm(0);
            alarmEnabled = FALSE;
        }
        else{
            if(frame_type == FRAME_S){//supervision frame
                *state = S0;
                *length = 0;
            }
            break;
        }
    case SESC://
        frame[*length - 1] = c;

        if ( c == FLAG){
            if (frame_type == FRAME_I){//information frame
                ERROR_FLAG = 1;
                STOP = TRUE;
            }else{
                *state = S0;
                *length = 0;
            }
        }
        
    default:
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

        if(message[i] == ESC) { // octeto de escape
			str[j] = ESC;
			str[j + 1]= 0x5d; // substituido por 0x5d
			j++;
		}

		else if(message[i] ==  0x7e) { // flag pattern 
			str[j] = ESC;
			str[j + 1] = 0x5e; // octeto é substituido por 0x5e
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

unsigned char* destuffing(unsigned char* message, int* length) { // reverse process of stuffing
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
    full_frame[1] = ADDRESS_T; // campo de endereço
    full_frame[2] = 0;         // campo de controlo 
    full_frame[3] = full_frame[1] ^ full_frame[2]; // BCC = Campos protecao independentes (cabecalho 1 e  dados 2)

    for(int i = 0; i < *length; i++){
        full_frame[i+4] = stuffed_frame[i]; // i+4 avança frame 0,1,2,3 e avança para campo de informação
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
int llwrite(const unsigned char *buf, int length){
    unsigned char* full_message = create_frame(buf, &length), elem, frame[5];
    int res, frame_length = 0, state = S0;

    if(length < 0)
        return FALSE;

    alarmCount = 1;
    alarmEnabled = TRUE;
    ERROR_FLAG = FALSE;
    STOP = FALSE;

    while (alarmEnabled == TRUE && connectionParameters_ptr->nRetransmissions > alarmCount){
        res = write(fd,full_message, length);
        alarm(connectionParameters_ptr->timeout);
        alarmEnabled = FALSE;

        //Wait for response

        while( alarmEnabled == FALSE && STOP == FALSE){
        res = read(fd,&elem, length);

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
unsigned char* llread(int fd, int* length) {
    unsigned int message_array_length = 138;
	unsigned char elem, *message = malloc(message_array_length * sizeof(unsigned char)), *finish = malloc(1 * sizeof(unsigned char));
	int res, state = S0;

	*length = 0;
	flag_error = 0;
	STOP = FALSE;
	finish[0] = DISC;
	
    while(STOP == FALSE) {
		res = read(fd, &elem, 1);
		
        if(res > 0) {
			*length += 1;
			
            if(message_array_length <= *length) {
				message_array_length *= 2;
				message = realloc(message, message_array_length * sizeof(unsigned char));
			}
			state_machine(elem, &state, message, length, FRAME_I);
		}
	}

	if(message[4] == ADDRESS_R && flag_error != 1) {
		message = BCC1_random_error(message, *length);
		message = BCC2_random_error(message, *length);
	}

	if(flag_error == 1 || message[2] == CONTROL_T || message[2] == CONTROL_R)
		return NULL;

	if(message[2] == DISC) {
		llclose(fd, RECEIVER);
		return finish;
	}

	duplicate = (control_values[datalink.control_value] == message[2]) ? FALSE : TRUE;
	unsigned char temp = message[2], *no_head_message = remove_supervision_frame(message, length), *no_BCC2_message = BCC2(no_head_message, length);

	if(*length == -1) {
		if(duplicate == TRUE) { // se recebeu trama duplicada envia RR receiver ready 
			send_RR_REJ(fd, RR, temp);
			return NULL;
		}
		else {
			send_RR_REJ(fd, REJ, temp);
			return NULL;
		}
	}

	else {
		if(duplicate != TRUE) {
			datalink.control_value = send_RR_REJ(fd, RR, temp);
			return no_BCC2_message;
		}
		else {
			send_RR_REJ(fd, RR, temp);
			return NULL;
		}
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