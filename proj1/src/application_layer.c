// Application layer protocol implementation

#include "application_layer.h"

void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename)
{
    // application layer figures out what it has to do and uses functions 
    //from link_layer to do it

    //Figure out how 
    linkLayer linklayer = {serialPort,role,baudRate,nTries,timeout}

    switch (*role){
    case 'tx'://TRANSMITTER
        llopen()
        /* code */
        break;
    case 'rx'://RECEIVER
    default:
        break;
    }

}
