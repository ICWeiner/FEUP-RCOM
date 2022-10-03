// Application layer protocol implementation

#include "application_layer.h"
#include "link_layer.h"
#include "macros.h"

void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename)
{
    // application layer figures out what it has to do and uses functions 
    //from link_layer to do it

    //struct holding necessary date for the link layer, this probably shouldn't be here, but the pre defined llopen function expects a LinkLayer Type
    LinkLayer link_layer = {*serialPort,*role,baudRate,nTries,timeout};

    switch (*role){
    case 'tx'://TRANSMITTER
        llopen(link_layer);
        /* do transmitter things */
        break;
    case 'rx'://RECEIVER
        llopen(link_layer);
        /* do receiver things */
        break;
    default://TODO: HANDLE ERROR
        break;
    }

}
