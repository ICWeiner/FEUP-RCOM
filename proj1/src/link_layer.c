// Link layer protocol implementation
#include "link_layer.h"
#include "macros.h"
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <termios.h>

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source

typedef struct {
    state currState;
    unsigned char adr;
    unsigned char ctrl;
    unsigned char bcc;
    unsigned char *data;
    unsigned int data_size;
} State;


struct termios oldtio;
struct termios newtio;
LinkLayer connection;
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
}


int stuffing(const unsigned char* message, int length, unsigned char* dest, unsigned char *bcc){
    int destLenght = 0;

    for(int i = 0; i < length; i++){
        if(bcc != NULL){//if bcc isnt checked for null, funky things will happen
            *bcc ^= message[i];
        }
        
        if(message[i] == ESCAPE) {
			dest[destLenght++] = ESCAPE;
			dest[destLenght++] = ESCAPE_ESCAPE;
			break;
		}

		else if(message[i] ==  FLAG) {
			dest[destLenght++] = ESCAPE;
			dest[destLenght++] = ESCAPE_FLAG;
			break;
		}else{
            dest[destLenght++] = message[i];
        }
    }

    return destLenght;
}

int buildFrame(unsigned char* buffer, unsigned char address, unsigned char control){
    
    buffer[0] = FLAG;
    buffer[1] = address;
    buffer[2] = control;
    buffer[3] = address ^ control;
    buffer[4] = FLAG;
    return 5;
}
int buildDataFrame(unsigned char* framebuf, const unsigned char* data,unsigned int data_size, unsigned char address, unsigned char control){
    framebuf[0] = FLAG;
    framebuf[1] = address;
    framebuf[2] = control;
    framebuf[3] = address ^ control;
    int offset = 0;
    unsigned char bcc = 0;
    for(unsigned int i = 0;i < data_size;++i){
        offset+=stuffing(data + i, 1, framebuf + offset + 4, &bcc);
    }
    offset += stuffing(&bcc,1,framebuf+offset+4,NULL);
    framebuf[4 + offset]=FLAG;
    return 5 + offset;
}

void state_handler(unsigned char byte,State* stateData){
    switch (stateData -> currState){
        case REJ_RCV:
        case END_RCV:
            stateData -> currState=START;
        case START:
            if(byte == FLAG){
                stateData ->currState = FLAG_RCV;
            }break;
        case FLAG_RCV:
            stateData->data_size = 0;
            if(byte == ADR_TX || byte == ADR_RX){
                stateData->currState=ADR_RCV;
                stateData->adr=byte;
            }else if(byte == FLAG){
                //do nothing, because we want to remain in the same state, should probably remove this condition...
            }else{
                stateData = START;
            }
            break;
        case ADR_RCV:
            if(byte == CTRL_UA     || byte == CTRL_SET
            || byte == CTRL_DISC   || byte == CTRL_REJ(0)
            || byte == CTRL_REJ(1) || byte == CTRL_DATA(0)
            || byte == CTRL_DATA(1)|| byte == CTRL_RR(0)
            || byte == CTRL_RR(1)){
                stateData->currState = CTRL_RCV;
                stateData->ctrl = byte;
                stateData->bcc = stateData->adr ^ stateData->ctrl;
            }
            else if (byte == FLAG){
                stateData->currState = FLAG_RCV;
            }else {
                stateData->currState = START;
            }
            break;    
        case CTRL_RCV:
            if(byte == stateData->bcc){
                stateData->currState = BCC1_RCV;
            }
            else if (byte == FLAG){
                stateData->currState = FLAG_RCV;
            }else{
                stateData->currState = START;
            }
            break;
        case BCC1_RCV:
            if(byte==FLAG){
                if(stateData->ctrl == CTRL_DATA(0) || stateData->ctrl == CTRL_DATA(0) ){
                    stateData->currState = FLAG_RCV;
                }else{
                    stateData->currState = END_RCV;
                }
            }else if( stateData->ctrl == CTRL_DATA(0) || stateData->ctrl == CTRL_DATA(1)){
                if(stateData->data != NULL){
                    stateData->data_size = 0;
                    if(byte == ESCAPE){
                        stateData->currState = ESC_RCV;
                        stateData->bcc = 0;
                    }else{
                        stateData->data[stateData->data_size++] = byte;
                        stateData->bcc = byte;
                        stateData->currState = DATA_RCV;
                    }
                }
            }else{
                stateData->currState = START;
            }
            break;
        case DATA_RCV:
            if(byte == ESCAPE){
                stateData->currState = ESC_RCV;
            }else if(byte == FLAG){
                stateData->currState = REJ_RCV;
            }else if(byte == stateData->bcc){
                stateData->currState = BCC2_RCV;
            }else{
                stateData->data[stateData->data_size++] = byte;
                stateData->bcc ^= byte;
            }
            break;
        case ESC_RCV:
            if(byte == FLAG){
                stateData ->currState = REJ_RCV; 
            }else if(byte ==ESCAPE_FLAG){
                if(stateData->bcc == FLAG){
                    stateData->currState = BCC2_RCV;
                }else{
                    stateData->bcc ^= FLAG;
                    stateData->data[stateData->data_size++] = FLAG;
                    stateData->currState = DATA_RCV;
                }
            }else if(byte == ESCAPE_ESCAPE){
                if(stateData->bcc == ESCAPE){
                    stateData->currState = BCC2_RCV;
                }else{
                    stateData->bcc ^= ESCAPE;
                    stateData->data[stateData->data_size++] = ESCAPE;
                    stateData->currState = DATA_RCV;
                }
            }else{
                stateData->currState = START;
            }
            break;
        case BCC2_RCV:
            if(byte == FLAG){
                stateData->currState = END_RCV;
            }else if( byte == 0){
                stateData->data[stateData->data_size++] = stateData->bcc;
                stateData->bcc = 0;
            }else if(byte == ESCAPE){
                stateData->data[stateData->data_size++] = stateData->bcc;    
                stateData->bcc = 0;
                stateData->currState = ESC_RCV;
            }
            else{
                stateData->data[stateData->data_size++] = stateData->bcc;
                stateData->data[stateData->data_size++] = byte;
                stateData->bcc = byte;
                stateData->currState = DATA_RCV;
            }
            break;
    }
}

////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llopen(LinkLayer connectionParameters)
{

    connection = connectionParameters;
    fd = open(connection.serialPort, O_RDWR | O_NOCTTY);
    int result = FALSE;//if this var is false, it means we failed to set the mode
    

    if (fd < 0){
        perror(connection.serialPort);
        exit(-1);
    }

    struct termios newtio;

    // Save current port settings
    if (tcgetattr(fd, &oldtio) == -1){
        perror("tcgetattr");
        exit(-1);
    }

    // Clear struct for new port settings
    bzero(&newtio, sizeof(newtio));

    newtio.c_cflag = connection.baudRate | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    // Set input mode (non-canonical, no echo,...)
    newtio.c_lflag = 0;

    newtio.c_cc[VTIME] = 0; // Inter-character timer 
    newtio.c_cc[VMIN] = 0;  // Blocking read until 1 chars received
    
    tcflush(fd, TCIOFLUSH);

    // Set new port settings
    if (tcsetattr(fd, TCSANOW, &newtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    if(connection.role == LlTx){ //set as transmitter
        int receivedUA = 0;
        stateData.currState=START;
        alarmCount = 0;
        while(alarmCount<connection.nRetransmissions && receivedUA == FALSE){
            alarm(connection.timeout);
            alarmEnabled = TRUE;
            if(alarmCount > 0){
                puts("Timed out.\n");
            }
            int size = buildFrame(buf,ADR_TX,CTRL_SET);
            puts("llopen: Sent SET.\n");
            write(fd,buf,size);
            puts("HELLO" + size);
            while(alarmEnabled == TRUE && receivedUA == FALSE){
                int bytes_read = read(fd,buf,PACKET_SIZE_LIMIT);
                if(bytes_read < 0)
                    return -1;
                for(unsigned int i = 0;i < bytes_read && receivedUA == FALSE; ++i){
                    state_handler(buf[i], &stateData);
                    if(stateData.currState == END_RCV && stateData.adr == ADR_TX && stateData.ctrl == CTRL_UA)
                        receivedUA=1;
                }
            }
        }
        if(receivedUA){
            puts("llopen: Received UA.\n");
        } 
        return 1;
    }
    else if (connection.role == LlRx){//set as receiver
        alarmCount = 0;
        
        stateData.currState = START;
        int receivedSET = FALSE;
            while(receivedSET == FALSE){
                int bytes_read = read(fd, buf, PACKET_SIZE_LIMIT);
                if(bytes_read < 0)
                    return -1;
                for(unsigned int i = 0;i < bytes_read && receivedSET == FALSE; ++i){
                    state_handler(buf[i], &stateData);
                    if(stateData.currState == END_RCV && stateData.adr == ADR_TX && stateData.ctrl == CTRL_SET)
                        receivedSET = TRUE;
                }
            }
            if(receivedSET == TRUE){
                puts("llopen: Received Set.\n");
            } 
            int frame_size = buildFrame(buf,ADR_TX,CTRL_UA);//Build UA FRAME
            write(fd,buf,frame_size); //sends UA reply.
            puts("llopen: Sent UA.\n");
            return 1;
        }
    return fd;
}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(const unsigned char *buf, int bufSize){
    unsigned char bigBuf[bufSize*2+100];
    int frame_size = buildDataFrame(bigBuf,buf,bufSize,ADR_TX,CTRL_DATA(DATA_S_FLAG));
    
    for(unsigned int sent = 0;sent < frame_size;){ //In case write doesnt write all bytes from the first call.
        int ret = write(fd,bigBuf+sent,frame_size-sent);
        if(ret == -1){
            return -1;
        }
        sent += ret;
    }

    int receivedPacket = FALSE, resend = FALSE, retransmissions = 0;
    stateData.data=NULL; //State machine writes to packet buffer directly.
    
    alarmEnabled = TRUE;
    alarm(connection.timeout);
    while(receivedPacket == FALSE){
        if(alarmEnabled == FALSE){
            resend = TRUE;
            alarmEnabled = TRUE;
            alarm(connection.timeout);
        }
        if(resend == TRUE){
            if(retransmissions == connection.nRetransmissions){
                puts("Exceeded retransmission limit.\n");
                return -1;
            }
            for(unsigned int sent = 0;sent < frame_size;){
                int ret=write(fd,bigBuf+sent,frame_size-sent);
                if(ret == -1){
                    return -1;
                }   
                sent += ret;
            }
            resend = FALSE;
            retransmissions++;
        }
        int bytes_read = read(fd,buf,PACKET_SIZE_LIMIT);
        if(bytes_read < 0){
            return -1;
        }  
        for(unsigned int i = 0;i < bytes_read && receivedPacket == FALSE; ++i){ 
            state_handler(buf[i],&stateData);
            if(stateData.currState == END_RCV){
                if(stateData.adr == ADR_TX && stateData.ctrl == CTRL_RR(DATA_S_FLAG)){
                    receivedPacket = TRUE;
                    break;
                }
                if(stateData.adr == ADR_TX && stateData.ctrl == CTRL_REJ(DATA_S_FLAG)){
                    resend=1;
                    break;
                }
            }
        }
    }
    return 0;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet){
     int receivedPacket = 0;
    stateData.data = packet; //State machine writes to packet buffer directly.
    while(receivedPacket == FALSE){
        int bytes_read = read(fd,buf,PACKET_SIZE_LIMIT);
        if(bytes_read<0)
            return -1;
        for(unsigned int i = 0;i < bytes_read && receivedPacket == FALSE; ++i){
            state_handler(buf[i],&stateData);
            
            if(stateData.currState >= BCC1_RCV && stateData.data != NULL){//WIP
                //printf("(state:%i,packet:%i,data_size:%i,last_data:%i)\n",state.state,packet[i], state.data_size,state.data[state.data_size-1]);
            }

            if(stateData.currState == REJ_RCV && stateData.adr == ADR_TX){
                int frame_size = buildFrame(buf,ADR_TX,(stateData.ctrl==CTRL_DATA(0)?CTRL_REJ(0):CTRL_REJ(1)));
                write(fd,buf,frame_size); //sends REJ reply.
                puts("llread: Sent REJ.\n");
            }
            if(stateData.currState == END_RCV && stateData.adr == ADR_TX && stateData.ctrl == CTRL_SET){
                int frame_size = buildFrame(buf,ADR_TX,CTRL_UA);
                write(fd,buf,frame_size); //sends UA reply.
                puts("llread: Sent UA.\n");
            }
            if(stateData.currState == END_RCV && stateData.adr == ADR_TX){//TODO:too much duplicate code?
                if(stateData.ctrl == CTRL_DATA(0)){
                    int frame_size = buildFrame(buf,ADR_TX,CTRL_RR(0));
                    write(fd,buf,frame_size);
                    puts("llread: Sent RR.\n" + DATA_S_FLAG);
                    return stateData.data_size;
                }
                else if(stateData.ctrl == CTRL_DATA(1)){
                    int frame_size = buildFrame(buf,ADR_TX,CTRL_RR(1));
                    write(fd,buf,frame_size);
                    puts("llread: Sent RR.\n" + DATA_S_FLAG);
                    return stateData.data_size;
                }
                else{
                    int frame_size = buildFrame(buf,ADR_TX,CTRL_RR(0));
                    write(fd,buf,frame_size);
                    puts("llread: Sent RR\n"+ DATA_S_FLAG);
                }
            }
            if(stateData.ctrl == CTRL_DISC) {
                DISCreceived = TRUE;
                puts("llread: Received DISC.\n");
                return -1;
            }
        }
    }
    return 0;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics){
    //trasmistor - sends DISC, receives UA
    //receiver - receives DISC in llread, sends UA
    signal(SIGALRM, alarm_handler);
    alarmCount=0;

    if(connection.role == LlTx) { //Transmitter case

        int DISCreceived_tx = 0;
        
        while(alarmCount<connection.nRetransmissions && DISCreceived_tx == FALSE){
            alarm(connection.timeout);
            alarmEnabled = TRUE;
            if(alarmCount > 0){
                puts("Timed out.\n");
            }
            int size = buildFrame(buf,ADR_TX,CTRL_DISC);
            puts("llclose: Sent DISC.\n");
            write(fd,buf,size);
            while(alarmEnabled == TRUE && DISCreceived_tx == FALSE){
                int bytes_read = read(fd,buf,PACKET_SIZE_LIMIT);
                if(bytes_read<0)
                    return -1;
                for(unsigned int i = 0;i < bytes_read && DISCreceived_tx == FALSE;++i){
                    state_handler(buf[i],&stateData);
                    if(stateData.currState == END_RCV && stateData.adr == ADR_TX && stateData.ctrl == CTRL_DISC)
                        DISCreceived_tx=1;
                }
            }
        }
        if(DISCreceived_tx == TRUE){
            puts("llclose: Received DISC.\n");
        } 
        int frame_size = buildFrame(buf,ADR_TX,CTRL_UA);//Build UA
        write(fd,buf,frame_size);
        puts("llclose: Sent UA.\n");
        sleep(1);

    } else { //Receiver
        while(DISCreceived == FALSE){
            int bytes_read = read(fd,buf,PACKET_SIZE_LIMIT);
            if(bytes_read < 0)
                return -1;
            for(unsigned int i = 0;i < bytes_read && DISCreceived == FALSE;++i){
                state_handler(buf[i], &stateData);
                if(stateData.currState == END_RCV && stateData.adr == ADR_TX && stateData.ctrl == CTRL_DISC)
                    DISCreceived = TRUE;
            }
        }
        if( DISCreceived == TRUE){
            puts("llclose: Received DISC .\n");
        } 
        int frame_size = buildFrame(buf,ADR_TX,CTRL_DISC);//BUILD DISC
        write(fd,buf,frame_size); 
        puts("llclose: Sent DISC.\n");

        int receivedUA = FALSE;
        while( receivedUA == FALSE){
            int bytes_read = read(fd,buf,PACKET_SIZE_LIMIT);
            if(bytes_read<0)
                return -1;
            for(unsigned int i = 0;i < bytes_read && receivedUA == FALSE; ++i){
                state_handler(buf[i],&stateData);
                if(stateData.currState == END_RCV && stateData.adr == ADR_TX && stateData.ctrl == CTRL_UA)
                    receivedUA = TRUE;
            }
        }
        if(receivedUA == TRUE){
            puts("llclose: Received UA .\n");
        } 
    }

    // Restore the old port settings
    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    close(fd);
    return 1;
}