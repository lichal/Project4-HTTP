#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#define LIST_SIZE 30

struct client{
    int socket;
    char *name;
};

struct client clientList[LIST_SIZE];
int positionApart;

/* Handle receiving operation */
void * handleclient(void * arg){
    int clientsocket = *(int *)arg;
    printf("Client %d Connected\n",clientList[clientsocket-positionApart].socket-positionApart);
    
    while (1) {
        char *chatRecv;
        chatRecv = (char *)malloc(sizeof(char*)*5000);
        if(recv(clientsocket,chatRecv, 5000,0)>0){
            printf("From %s: %s\n",clientList[clientsocket-positionApart].name,chatRecv);
            
            // Checks if the input asks to Quit.
            if(!strncmp(chatRecv,"Quit",4)){
                printf("Disconnecting....\n\tExit\n");
                send(clientsocket,chatRecv,strlen(chatRecv)+1,0);
                close(clientsocket);
                exit(0);
            }
            free(chatRecv);
        }
    }
}
/* Handle sending operation */
void * sendchat(void * arg){
    while (1) {
        int clientsocket = *(int *)arg;
        char *chatSend;
        chatSend = (char *)malloc(sizeof(char*)*5000);
        if(read(0,chatSend,5000)!=-1){
            printf("\nSend to client: %s\n", chatSend);
            send(clientsocket,chatSend,strlen(chatSend)+1,0);
        }
        free(chatSend);
    }
}

/**************************************************
 * Construct the status header for HTTP.
 *************************************************/
char *statusRequest(char *code, char *type){
	char *header = (char *)malloc(sizeof(char)*20);
	strcpy(header, "HTTP/1.1 ");
	strcat(header, code);
	strcat(header, " ");
	strcat(header, type);
	strcat(header, "\r\n");

	return header;
}

/**************************************************
 * Get the current time in GMT and format the time.
 *************************************************/
char* timeRequest(){
	// Declare Time Variables
	time_t now;
	char *timeString = (char *)malloc(sizeof(char)*40);
	char timeFormat[40];

	/* Date Format: Weekday, date month year time GMT. */
	time(&now);
	strftime(timeFormat, 40, "%a, %d %b %Y %X GMT\r\n", gmtime(&now));
	
	/* Construct Header */
	strcpy(timeString, "Date: ");
	strcat(timeString, timeFormat);

	return timeString;
}

/**************************************************
 * Construct a Last-Modified header.
 *************************************************/
char *modifiedRequest(char *filePath){
	char *lastModified  = (char *)malloc(sizeof(char)*50);
	struct stat attr;
	char dateString[40];

	/* Date Format: Weekday, date month year time GMT. */
	stat(filePath, &attr);
	strftime(dateString, 40, "%a, %d %b %Y %X GMT\r\n", gmtime(&attr.st_ctime));

	/* Construct Header */
	strcpy(lastModified, "Last-Modified: ");
	strcat(lastModified, dateString);
	
	return lastModified;
}

/**************************************************
 * Read file contents
 *************************************************/
char *fileRequest(char *filePath){
	FILE *op = fopen(filePath, "rb");
	fseek(op, 0, SEEK_END);
	long fsize = ftell(op);
	fseek(op, 0, SEEK_SET);
	char *buffer = (char *)malloc(sizeof(char)*(fsize+4));

	fread(buffer, fsize, 1, op);
	strcat(buffer, "\r\n");
	fclose(op);
	return buffer;
}

/**************************************************
 * Content-length header.
 *************************************************/
char *lengthRequest(char *filePath){
	// Open file and read size
	FILE *op = fopen(filePath, "rb");
	fseek(op, 0, SEEK_END);
	int fsize = ftell(op);
	fseek(op, 0, SEEK_SET);

	char *lengthHeader = (char *)malloc(sizeof(char)*50);
	// Store size into char
	char size[10];
	sprintf(size, "%d", fsize);

	// Construct the header
	strcpy(lengthHeader, "Content-Length: ");
	strcat(lengthHeader, size);
	strcat(lengthHeader, "\r\n");
	
	fclose(op);
	return lengthHeader;
}

/**************************************************
 * Return content-type header
 *************************************************/
char *typeRequest(char *filePath){
	char *typeHeader = (char *)malloc(sizeof(char)*(50));

	// Construct the header
	strcpy(typeHeader, "Content-Type: ");
}

/**************************************************
 * Return a string containing http response.
 *************************************************/
char *httpResponse(char *code, char *file){
	/* Content buffe that will hold http response */
	char *content = (char *)malloc(sizeof(char)*1500);

	/* Instantiate char pointer for all headers */
	char *header = statusRequest(code, "OK");
	char *nowTime = timeRequest();
	char *modified = modifiedRequest(file);
	char *length = lengthRequest(file);
	char *fileType = typeRequest(file);
	char *filebuffer = fileRequest(file);

	/* Store all the headers into content buffer */
	strcpy(content, header);
	strcat(content, nowTime);
	strcat(content, modified);
	strcat(content, length);
	strcat(content, fileType);
	strcat(content, "\r\n");
	strcat(content, filebuffer);

	/* Free everything since its all stored in content */
	free(header);
	free(nowTime);
	free(modified);
	free(length);
	free(fileType);
	free(filebuffer);

	return content;
}

int main(int argc, char **argv){
    int sockfd = socket(AF_INET,SOCK_STREAM,0);
    
    printf("\nTHIS IS A HTTP SERVER\n");

	/**************************
	 * Header testing
	 ***************************/
	char *content = httpResponse("200", "index.html");
	printf("%s",content);
	/**************************
	 * Header testing End
	 ***************************/

    struct sockaddr_in serveraddr,clientaddr;
    serveraddr.sin_family=AF_INET;
    serveraddr.sin_port=htons(8080);
    serveraddr.sin_addr.s_addr=INADDR_ANY;
    
    bind(sockfd,(struct sockaddr*)&serveraddr,sizeof(serveraddr));
    listen(sockfd,10);
    
    pthread_t child[LIST_SIZE]; // an array of threads
    int clientsocket[LIST_SIZE];

    printf("\nWaiting for connection from client!\n\nType enter/return to send a message...\n\n");
    
    /******************************************************************************************
     * This loop accepts new incoming client connections and creates new threads
     * for both handleserver and handle client for each. Max of 8 clients but can
     * be increased by updating the definition of LIST_SIZE
     *****************************************************************************************/
    int count=0;
    while(count<LIST_SIZE){
        int len = sizeof(clientaddr);
        clientsocket[count] = accept(sockfd,(struct sockaddr*)&clientaddr,&len);
        printf("CLient Socket: %d\n", clientsocket[count]);
        
        // Receive a usr name from client
        char *initialInfo;
        initialInfo=(char *)malloc(sizeof(char*)*5000);
        recv(clientsocket[count],initialInfo, 5000,0);
        
        char method[10];
        int current = 0;
        int startParsing = 0;
        while(initialInfo[current] != '/'){
            method[startParsing] = initialInfo[current];
            startParsing++;
            current++;
        }
        printf("Method: %s\n", method);
        current++;
        
        char name[50];
        startParsing = 0;
        while(initialInfo[current] != ' '){
            name[startParsing] = initialInfo[current];
            startParsing++;
            current++;
        }
        printf("File name Parsed %s\n", name);
        
        current++;
        
        char protocol[10];
        startParsing = 0;
        while(initialInfo[current] != '\n'){
            protocol[startParsing] = initialInfo[current];
            startParsing++;
            current++;
        }
        
        printf("Protocol Parsed %s\n", protocol);

		send(clientsocket[count],content,strlen(content)+1,0);
        
        // store usr name and socket into a struct
        clientList[count].socket=clientsocket[count];
        clientList[count].name=initialInfo;
        
        // position apart from socket to array
        positionApart=clientsocket[count]-count;
        printf("\nHTTP GET REQUEST \n\n%s\n\n", clientList[count].name);
        
        // Create a thread for both send and receive
        pthread_create(&child[count],NULL,handleclient,&clientsocket[count]);
        // Detach thread after done
        pthread_detach(child[count]);
        
        // increase count for next connection
        count++;
    }
}
