#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <netinet/in.h>
#include<arpa/inet.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>

int main(int argc, char **argv) {
    struct hostent *h;
    char buf[512],user[50],password[50],host[100],urlPath[512];
    if (argc != 2) { // tem de dar 2 argumentos 
        fprintf(stderr, "Usage: %s ftp://[<user>:<password>@]<host>/<url-path>\n", argv[0]);
        exit(-1);
    }
    sscanf(argv[1],"ftp://%511s",buf); 
    {
        size_t distance=0,length=strlen(buf);
        distance=strcspn(buf,":"); // caracteres em buf que não estejam em ":"
        if(distance<length){
            size_t distance2=strcspn(buf,"@"); // caracteres em buf que não estejam em "@"
            if(distance2==length){
                fprintf(stderr, "Usage: %s ftp://[<user>:<password>@]<host>/<url-path>\n", argv[0]);
                exit(-1);
            }
            strncpy(user,buf,distance); // copia n distances de buf para user  -> aqui distance é 4 -> rcom
            user[distance]='\0';  // user = rcom \'0'  
            strncpy(password,buf+distance+1,distance2-distance-1); // distance 2 = 9 então 9 - 4 -1 = 4 -> rcom
            password[distance2-distance-1]='\0'; // password = rcom
            distance=strcspn(buf+distance2+1,"/");  // distance = 16 -> netlab1.fe.up.pt
            strncpy(host,buf+distance2+1,distance); // copia 16 caracteres de distance, contidos em buf+distance2+1 para host
            host[distance]='\0';              // host = netlab1.fe.up.pt
            strcpy(urlPath,buf+distance2+distance+1);
            //printf("urlPath: %s\n", urlPath); // urlPath = pipe.txt
            //printf("buf: %s\n", buf);                  // buf = rcom:rcom@netlab1.fe.up.pt/pipe.txt
            //printf("distance: %ld\n", distance);       // distance = 16 -> netlab1.fe.up.pt
            //printf("distance2: %ld\n", distance2);     // distance2 = 9  -> rcom:rcom    
        }else   
        {
            distance=strcspn(buf,"/");
            strncpy(host,buf,distance);
            host[distance]='\0';
            strcpy(urlPath,buf+distance+1);   
            sprintf(user,"anonymous");
            sprintf(password,"password");
        }
    }printf("user:%s password:%s host:%s path:%s|\n",user,password,host,urlPath);
    //ftp://rcom:rcom@netlab1.fe.up.pt

    if ((h = gethostbyname(host)) == NULL) { // hostname -> mirrors.up.pt 
        herror("gethostbyname()");
        printf("%s\n",host);
        exit(-1);
    }
    //# define	h_addr	h_addr_list[0] /* Address, for backward compatibility.*/
    printf("Host name  : %s\n", h->h_name); // hostname -> mirrors.up.pt 
    printf("IP Address : %s\n", inet_ntoa(*((struct in_addr *) h->h_addr_list[0])));

    int sockfd;
    struct sockaddr_in server_addr; // sockaddr_in -> berkeley socket 
    char buf2[519];
    size_t bytes;

    /*server address handling*/
    bzero((char *) &server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(inet_ntoa(*((struct in_addr *) h->h_addr_list[0])));    /*32 bit Internet address network byte ordered -> slide 40 */ 
    server_addr.sin_port = htons(21);        /*server TCP port must be network byte ordered */

    /*open a TCP socket*/
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) { // socket(int family, int type, int protocol) -> returns socket descriptor 
        perror("socket()");
        exit(-1);
    }
    /*connect to the server*/
    if (connect(sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) { // int connect(int sockfd, struct sockaddr* serveraddr, int addrlen)
            printf("%s\n",h->h_addr_list[0]);
        perror("connect()");
        exit(-1);
    }
    /*send a string to the server*/
    usleep(100000);
    int bytes_read=read(sockfd,buf,512);
    buf[bytes_read]='\0';
    printf("%s\n",buf); // 220

    sprintf(buf2,"user %s\r\n",user); // user = anonymous 
    bytes = write(sockfd, buf2, strlen(buf2)); // sockfd = 3 
    if (bytes > 0)
        printf("Bytes escritos %ld\n", bytes);
    else {
        perror("write()");
        exit(-1);
    }
    bytes_read=read(sockfd,buf,512);
    buf[bytes_read]='\0';
    printf("%s\n",buf); // 331
    sprintf(buf2,"pass %s\r\n",password); // password = password
    write(sockfd,buf2,strlen(buf2)); // buf2 = pass password
    //printf("sockfd value: %d\nbuf2 value: %s", sockfd, buf2);
    bytes_read=read(sockfd,buf,512);
    buf[bytes_read]='\0';
    printf("%s",buf); // 230

    sprintf(buf2,"pasv\r\n");
    write(sockfd,buf2,strlen(buf2)); // sockfd = 3, buf2 = pasv
    bytes_read=read(sockfd,buf,512);
    buf[bytes_read]='\0';
    printf("%s",buf); // 227 
    
    int h1=0,h2=0,h3=0,h4=0,p1=0,p2=0,pasvport;
    sscanf(buf,"227 Entering Passive Mode (%i,%i,%i,%i,%i,%i).",&h1,&h2,&h3,&h4,&p1,&p2);
    pasvport= p1*256+p2; // slide 43

    int sockfd2;
    struct sockaddr_in server_addr2;

    /*server address handling*/
    struct in_addr my;
    sprintf(buf,"%i.%i.%i.%i",h1,h2,h3,h4); // buf = 193.137.29.15
    bzero((char *) &server_addr2, sizeof(server_addr2));
    server_addr2.sin_family = AF_INET;
    server_addr2.sin_addr.s_addr = inet_addr(buf);  /*32 bit Internet address network byte ordered*/
    server_addr2.sin_port = htons(pasvport);        /*server TCP port must be network byte ordered -> pasvport = 51051 */
    //printf("buf value: %s\npasvport value: %d", buf, pasvport);

    if ((sockfd2 = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket()");
        exit(-1);
    }//printf("106\n");
    /*connect to the server*/
    if (connect(sockfd2,(struct sockaddr *) &server_addr2, sizeof(server_addr2)) < 0) {
        perror("connect()");
        exit(-1);
    }//printf("113 %s\n",urlPath);
    sprintf(buf2,"retr %s\r\n",urlPath); // urlPath: pub/kodi/timestamp.txt 
    bytes=write(sockfd,buf2,strlen(buf2));
    if (bytes > 0)
        printf("Bytes escritos %ld\n", bytes); // 29 
    else {
        perror("write()");
        exit(-1);
    }
    bytes_read=read(sockfd,buf,512);
    buf[bytes_read]='\0'; // 150 

    char* filename = strrchr(urlPath, '/');

    printf("Receiving file %s...\n",filename+1);

    FILE* fd = fopen(filename+1,"w");

    if(fd == NULL)
    {
        /* File not created hence exit */
        printf("Unable to create file locally.\n");
        exit(EXIT_FAILURE);
    }
    while((bytes_read=read(sockfd2,buf,512))>0){
        fwrite(buf,1,bytes_read,fd);
        //printf("%s",buf);
    }

    fclose(fd); //close received file
    printf("File Received!\n");

    if (close(sockfd)<0) {
        perror("close()");
        exit(-1);
    }
    if (close(sockfd2)<0) {
        perror("close()");
        exit(-1);
    }
    return 0;

}