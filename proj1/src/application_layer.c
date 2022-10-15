// Application layer protocol implementation

#include "application_layer.h"
#include "link_layer.h"
#include "macros.h"
#include <string.h>
#include <stdio.h>

void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename)
{
    // application layer figures out what it has to do and uses functions 
    //from link_layer to do it

    //struct holding necessary date for the link layer, this probably shouldn't be here, but the pre defined llopen function expects a LinkLayer Type
    LinkLayer link_layer = {*serialPort,*role,baudRate,nTries,timeout};
    
    puts(role);
    char transmitter[3] = "tx\0", receiver[3] = "rx\0";

    puts("ola");
    if(strcmp(role,transmitter)) {
        puts("ola tx");
        //llopen(link_layer);
        /* do transmitter things */
    }else if(strcmp(role,receiver)) {
        puts("ola rx");
        //llopen(link_layer);
        /* do receiver things */
    }else{//TODO: HANDLE ERROR

    }

}
