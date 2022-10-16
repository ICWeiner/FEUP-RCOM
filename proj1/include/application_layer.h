// Application layer protocol header.
// NOTE: This file must not be changed.

#ifndef _APPLICATION_LAYER_H_
#define _APPLICATION_LAYER_H_

#define DATA_CONTROL 1

#define MAX_SIZE_LINK 7 // 2 * 1(BCC2) + 5 other element trama datalink

#define START_PACKAGE_TYPE 1
#define END_PACKAGE_TYPE 0

typedef struct {
  int file_size;
  char* file_name;
  FILE* fp;
  int read_size;
} File;

typedef struct {
  int file_descriptor;
  char* status;
} Application;

File file;
Application application;

int send_message(unsigned char* message, int length);

unsigned char* get_message();

unsigned char* get_only_data(unsigned char* message_read, int* length);

unsigned char* data_package(unsigned char* message, int* length);

int START_END_package(unsigned char* package, int type);

int file_size();

void file_parameters(unsigned char* message);

void readfile();

void writefile(unsigned char* data, int read_size);

int verify(unsigned char* message);

#endif


// Application layer main function.
// Arguments:
//   serialPort: Serial port name (e.g., /dev/ttyS0).
//   role: Application role {"tx", "rx"}.
//   baudrate: Baudrate of the serial port.
//   nTries: Maximum number of frame retries.
//   timeout: Frame timeout.
//   filename: Name of the file to send / receive.
void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename);



#endif // _APPLICATION_LAYER_H_
