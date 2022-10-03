// Link layer protocol implementation
#include "link_layer.h"
#include "macros.h"

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source


struct link_layer linkLayer;
struct termios oldtio;
volatile int STOP;

////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llopen(LinkLayer connectionParameters)
{
    // TODO:check functionality
    // Open serial port device for reading and writing
    // the O_RDWR flag indicates that the port will be open for reading and writing
    //the O_NOCTTY flag tells UNIX that this program doesn't want to be the "controlling terminal" for that port.
    int fd = open(connectionParameters.serialPort, O_RDWR | O_NOCTTY);
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

    // VTIME e VMIN should be changed in order to protect with a
    // timeout the reception of the following character(s)

    // Now clean the line and activate the settings for the port
    // tcflush() discards data written to the object referred to
    // by fd but not transmitted, or data received but not read,
    // depending on the value of queue_selector:
    //   TCIFLUSH - flushes data received but not read.
    tcflush(fd, TCIOFLUSH);

    // Set new port settings
    if (tcsetattr(fd, TCSANOW, &newtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }



    //TODO use signal() for timeouts
    //signal(SIGALARM, alarm_handler);

    unsigned char UA[5] = {FLAG, ADDRESS_T, CONTROL_R, BCC_R, FLAG}, elem, frame[5];
    int res, frame_length = 0, state = S0;

    switch (connectionParameters.role){
    case LlTx:
        //TODO: set as transmitter
        break;
    case LlRx:
        //TODO: set as receiver
        break;
    
    default:
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
