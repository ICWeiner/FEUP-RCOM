// Application layer protocol implementation

#include "application_layer.h"
#include "link_layer.h"
#include "macros.h"
#include <string.h>
#include <stdio.h>


unsigned char applicationbuffer[PACKET_SIZE_LIMIT + 30];

int get_type_length_value(unsigned char *address, unsigned char* type, unsigned char* length, unsigned char** value){
	*type = address[0];
	*length = address[1];
	*value = address + 2;

	return 2 + *length;//returns total size of TYPE + LENGTH + VALUE 
}

void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename)
{
    // application layer figures out what it has to do and uses functions 
    //from link_layer to do it

    //struct holding necessary date for the link layer
    LinkLayer connection;
	connection.baudRate = baudRate;
	connection.nRetransmissions = nTries;
	connection.timeout = timeout;
	strcpy(connection.serialPort,serialPort);


    if(strcmp(role,"tx") == 0) {// set transmitter
		connection.role=LlTx;
		puts("I am a transmitter");
    }else if(strcmp(role,"rx") == 0) {/* do receiver things */
		connection.role=LlRx;
		puts("I am a receiver");
    }else{//TODO: HANDLE ERROR
		puts("ERROR:Unknown role, use 'tx' or 'rx'");
    }


	if(llopen(connection) < 0){
		puts("ERROR: could not open connection");
		llclose(0);
		return -1;
	}

	if(connection.role == LlTx) {
		FILE* file = fopen(filename,"r");

		if(file == FALSE){
			puts("ERROR, could not open file");
			return -1;
		}else{
			puts("File opened sucessfuly");
		}

		fseek(file,0L,SEEK_END);
		long int file_size = ftell(file);
		fseek(file,0,SEEK_SET);

		//TODO write file size?

		applicationbuffer[0] = CONTROL_START;
		applicationbuffer[1] = TYPE_FILESIZE;
		applicationbuffer[2] = sizeof(long);

		*(long*)(applicationbuffer+3)=file_size;
		llwrite(applicationbuffer,10);

		unsigned char failure = 0;
		unsigned long bytes_sent = 0;

		for(unsigned char i = 0; bytes_sent < file_size;++i){
			int bytequant = file_size - bytes_sent< (PACKET_SIZE)? file_size-bytes_sent : (PACKET_SIZE);
			unsigned long file_bytes = fread(applicationbuffer + 4, 1, bytequant, file);

			if(file_bytes != bytequant){
                puts("File read failure");//TODO write file size?
                failure=TRUE;
                break;
            }

			applicationbuffer[0] = CONTROL_DATA;
			applicationbuffer[1] = i;
			applicationbuffer[2] = file_bytes>>8;
			applicationbuffer[3] = file_bytes%256;

			if(llwrite(applicationbuffer,file_bytes +4 ) < 0){
				puts("ERROR: failure on writing");
				failure = TRUE;
				break;
			}else{
				puts("Sent packet with this many bytes:" + bytequant);
			}
			bytes_sent += bytequant; 
		}
		if(failure == FALSE){
			applicationbuffer[0] = CONTROL_END;

			if(llwrite(applicationbuffer,1) < 0){
				puts("ERROR: failure on end packet");
			}else{
				puts("Sucess on send");
			}
		}fclose(file);

	}else if(connection.role == LlRx){
		long int file_size = 0 , file_size_received = 0;
		int bytes_read = llread(applicationbuffer);
		unsigned char t,l,*v;

		if(applicationbuffer[0] == CONTROL_START){
			int offset = 1;
			for(;offset<bytes_read;){
				offset+=get_type_length_value(applicationbuffer+offset,&t,&l,&v);
				if(t==TYPE_FILESIZE){
					file_size = *((unsigned long*)v);
					//TODO: WRITE FILESIZE
					}
			}

			FILE* file = fopen(filename, "w");
			if(!file) {
				puts("ERROR: Couldn't open file to write");
				return;
			} else {
				puts("Successfully opened file to write");
			}

			unsigned char FINISH_EARLY = FALSE, last_sequence_number=0;
            for(;file_size_received < file_size;){
                int numbytes=llread(applicationbuffer);
                if(numbytes<1){
                    if(numbytes==-1)
                        puts("error on llread");
                    else
                        puts("Received a packet that is too small");
						puts(numbytes + " bytes)");
                }
                if(applicationbuffer[0] == CONTROL_END){
                    puts("Disconnected before EOF");
                    FINISH_EARLY = TRUE;
                    break;
                }
                if(applicationbuffer[0] == CONTROL_DATA){
                    if(numbytes < 5)
                        puts("Received a packet that is too small to be correct");
						puts(numbytes + " bytes)");
                    if(applicationbuffer[1] != last_sequence_number){
						puts("Received packet on the wrong sequence actual number: ");
						puts(applicationbuffer[1] + "  expected number" + last_sequence_number - 1);

                    }else{
                        unsigned long size = applicationbuffer[3] + applicationbuffer[2] * 256;
                        if(size != numbytes-4){
							puts("Received packet size doesnt match info on header: ");
							puts("was:" +  (numbytes-4));
							puts("expected:" + size);
						}
						fwrite(applicationbuffer+4,1,size,file);
						file_size_received += size;
						puts("received packet with number:" + (last_sequence_number++));
                    }
                }
            }
			fclose(file);

			if(FINISH_EARLY == FALSE){
                int numbytes=llread(applicationbuffer);
                if(numbytes<1){
                    if(numbytes == -1){
						puts("error on llread");
					}else{
						puts("Received a packet that is too small to be correct");
						puts(numbytes + " bytes)");
					}
                }
                if(applicationbuffer[0]!=CONTROL_END){
                    puts("Received wrong packet type. Expected packet type: control end .");
                }else{
                    puts("Received packet of type end.closing\n");
                }
            }
			
        }else{
            puts("Transmission didn't start with a start packet.\n");
            for(unsigned int i=0;i<10;++i){
				puts(applicationbuffer[i]);
			}
        }
		
		}
		llclose(0);
		sleep(1);
	}

