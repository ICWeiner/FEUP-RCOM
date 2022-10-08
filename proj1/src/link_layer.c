// Link layer protocol implementation
#include "link_layer.h"
#include "macros.h"

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source

struct termios oldtio;
volatile int STOP;
int alarmEnabled = FALSE;
int alarmCount = 0; // current amount of trie
int ERROR_FLAG = FALSE;

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
        
    default:
        break;
    }
}

int set_as_transmitter(int* fd, LinkLayer *connectionParameters_ptr) {
    unsigned char SET[5] = {FLAG, ADDRESS_T, CONTROL_T, BCC_T, FLAG}, elem, frame[5];
    int res, frame_length = 0, state = S0;
	
    (void) signal(SIGALRM, alarm_handler);
    
    while(alarmEnabled == TRUE && connectionParameters_ptr->nRetransmissions > alarmCount) {
        res = write(*fd, SET, 5);

        alarm(connectionParameters_ptr->timeout);
        alarmCount = 0;

        //Wait for UA signal.

        while(alarmEnabled == 0 && STOP == FALSE) {
            res = read(*fd, &elem, 1);
       		
            if(res > 0) {
                frame_length++;
          		state_machine(elem, &state, frame, &frame_length, FRAME_S);
       		}
      }
  }

  if(ERROR_FLAG == FALSE)
     return FALSE;

  else
    return TRUE;
}

int set_as_receiver(int* fd,LinkLayer *connectionParameters_ptr) {
    unsigned char UA[5] = {FLAG, ADDRESS_T, CONTROL_R, BCC_R, FLAG}, elem, frame[5];
    int res, frame_length = 0, state = S0;

    while(STOP == FALSE) {
        res = read(*fd, &elem, 1);

        if(res > 0) {
		    frame_length++;
            state_machine(elem, &state, frame, &frame_length, FRAME_S);
      }
    }

	res = write(*fd, UA, 5);

	return TRUE;
}



////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llopen(LinkLayer connectionParameters)
{
    // TODO:check functionality
    int fd = open(connectionParameters.serialPort, O_RDWR | O_NOCTTY);
    int result = FALSE;//if this var is false, it means we failed to set the mode
    struct LinkLayer *connectionParameters_ptr;
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
        result = set_as_transmitter(&fd,connectionParameters_ptr);
    else if (strcmp(connectionParameters.role,LlRx))//set as receiver
        result = set_as_receiver(&fd,connectionParameters_ptr);

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
int llwrite(const unsigned char *buf, int bufSize)
{
    // TODO
   

    /*
    // Create string to send
    unsigned char send_buf[BUF_SIZE] = {0};
    unsigned char rcv_buf[BUF_SIZE] = {0};
    
    int bytes=0;
    
    while(STOP == FALSE){
		printf("Start writing:");
		scanf("%s",&send_buf);
		
		int i = 0;
        for (; i < BUF_SIZE; i++) {
			if (send_buf[i] == '\0')
			break;
    }
    
    bytes = write(fd, send_buf, i+1);
    printf("Sent: %s:%d\n", send_buf, bytes);
    
    unsigned char rcv_char;
    bytes = 0;
    do {
		bytes += read(fd, &rcv_char, 1);
		rcv_buf[bytes-1] = rcv_char;
	} while(rcv_char != '\0');
	
	printf("Receivd: %s:%d\n", rcv_buf, bytes);
	if (send_buf[0] == 'z') {
		STOP = TRUE;
	}*/
    

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
    
    // TODO handle this part diferently, depending if im a transmiter or a receiver
    /*
    switch (connectionParameters.role){
    case LlTx:
        //TODO: state machine set as transmitter
        if (){ //TODO:Handle error if error occurs

        }
        break;
    case LlRx:
        //TODO: state machine set as receiver
        if (){ //TODO:Handle error if error occurs
        
        }
        break;
    
    default:
        return -1;
    }*/

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
