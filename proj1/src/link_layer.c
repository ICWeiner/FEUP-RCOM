// Link layer protocol implementation
#include "link_layer.h"
#include "macros.h"

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source

struct termios oldtio;
LinkLayer *connectionParameters_ptr;
volatile int STOP;
int alarmEnabled = FALSE;
int alarmCount = 0; // current amount of trie
int ERROR_FLAG = FALSE;
int fd;


void alarm_handler() {
    alarmEnabled = FALSE;
    alarmCount++;

    printf("Alarm #%d\n", alarmCount);
}

void state_handler(unsigned char c,int* state, unsigned char* frame, int *length, int frame_type){
    switch (*state){
    case S0://START state, set to next state if FLAG received
        if(c == FLAG){
            *state = S1;
            frame[*length - 1] = c;
        }
        break;
    case S1://FLAG_RCV state, set to next state if ack received
        if(c != FLAG) {
            frame[*length - 1] = c;
            if(*length == 4) {
                if ((frame[1] ^ frame[2] == frame[3]))//check if UA received
                    *state = S2;
                 else 
                    *state = SESC;
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
    
    while(alarmEnabled == TRUE && connectionParameters_ptr->nRetransmissions > alarmCount) {
        res = write(fd, SET, 5);

        alarm(connectionParameters_ptr->timeout);
        alarmCount = 0;

        //Wait for UA signal.

        while(alarmEnabled == 0 && STOP == FALSE) {
            res = read(fd, &elem, 1);
       		
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



////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llopen(LinkLayer connectionParameters)
{
    // TODO:check functionality
    fd = open(connectionParameters.serialPort, O_RDWR | O_NOCTTY);
    int result = FALSE;//if this var is false, it means we failed to set the mode
    connectionParameters_ptr = &connectionParameters;

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
    memset(&newtio, 0, sizeof(newtio));

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

    if(strcmp(connectionParameters.role,LlTx)) //set as transmitter
        result = set_as_transmitter(&fd);
    else if (strcmp(connectionParameters.role,LlRx))//set as receiver
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
    unsigned char* full_message = create_frame(buf, bufSize), elem, frame[5];
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
    }

    //Wait for response

    while( alarmEnabled == FALSE && STOP == FALSE){
        res = read(fd,&elem, bufSize);

        if(res > 0){
            frame_length++;
            state_handler(elem, &state,frame,&frame_length,FRAME_S);
        }
    }
    
    if (STOP == TRUE){

    }

    return 0;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet){
    // TODO

    return 0;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics){
    unsigned char* received, UA[5] = {FLAG, ADDRESS_T, CONTROL_R, BCC_R, FLAG};

    if(strcmp(connectionParameters_ptr->role,LlTx)){
        received = send_DISC(fd);
        write(fd, UA, 5);
        sleep(1);
    }else if(strcmp(connectionParameters_ptr->role,LlRx)){
        received = send_DISC(fd);;  
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

unsigned char* create_frame(unsigned char* buf, int* bufSize) {
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