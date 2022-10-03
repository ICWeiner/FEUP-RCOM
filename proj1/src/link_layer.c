// Link layer protocol implementation
#include "link_layer.h"

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source

////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llopen(LinkLayer connectionParameters)
{
    // TODO?
    // Open serial port device for reading and writing
    // the O_RDWR flag indicates that the port will be open for reading and writing
    //the O_NOCTTY flag tells UNIX that this program doesn't want to be the "controlling terminal" for that port.
    int fd = open(connectionParameters.serialPort, O_RDWR | O_NOCTTY);
    if (fd < 0)
    {
        perror(connectionParameters.serialPort);
        exit(-1);
    }

    struct termios oldtio;
    struct termios newtio;

    // Save current port settings
    if (tcgetattr(fd, &oldtio) == -1)
    {
        perror("tcgetattr");
        exit(-1);
    }

    // Clear struct for new port settings
    memset(&newtio, 0, sizeof(newtio));

    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    // Set input mode (non-canonical, no echo,...)
    newtio.c_lflag = 0;
    newtio.c_cc[VTIME] = 0; // Inter-character timer unused
    newtio.c_cc[VMIN] = 1;  // Blocking read until 5 chars received

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

    printf("New termios structure set\n");

    signal(SIGALARM, alarm_handler);

    switch (connectionParameters.role){
    case LlTx:
        linkLayer.role = LlTx;
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
int llread(unsigned char *packet)
{
    // TODO

    return 0;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics)
{
    // TODO
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
