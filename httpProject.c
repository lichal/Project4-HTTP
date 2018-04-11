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
#define NUM_TYPES 4

char *httpResponse(char *file, char *code, char *option);
char *parseFile(char *request);
char *typeResponse(char *filename);
char *lengthResponse(char *filePath);
char *fileResponse(char *filePath);
char *modifiedResponse(char *filePath);
char* timeResponse();
char *statusResponse(char *code, char *type);

struct client{
    int socket;
    char *name;
};

struct client clientList[LIST_SIZE];
int positionApart;

/* A Content-Type Struct */
struct type{
	char *type;
	char *extention;
};

/* A global array for cotent type lookup table */
struct type typePairs[NUM_TYPES];

/* Handle receiving operation */
void * handleclient(void * arg){
    int clientsocket = *(int *)arg;
    printf("Client %d Connected\n",clientList[clientsocket-positionApart].socket);
    
    while (1) {
        char *chatRecv;
        chatRecv = (char *)malloc(sizeof(char*)*5000);
        if(recv(clientsocket,chatRecv, 5000,0)>0){
            
            printf("\n\nSecond request \n\n%s\n\n", chatRecv);
            
            char *filename = parseFile(chatRecv);
            char *content = httpResponse(filename, "200", "OK");
            
            send(clientsocket,content,strlen(content)+1,0);

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
char *statusResponse(char *code, char *type){
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
char* timeResponse(){
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
char *modifiedResponse(char *filePath){
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
char *fileResponse(char *filePath){
	FILE *op = fopen(filePath, "rb");
	fseek(op, 0, SEEK_END);
	long fsize = ftell(op);
	fseek(op, 0, SEEK_SET);
	char *buffer = (char *)malloc(sizeof(char)*(fsize+4));

	fread(buffer, fsize, 1, op);
	//strcat(buffer, "\r\n");
    
	fclose(op);
	return buffer;
}

/**************************************************
 * Content-length header.
 *************************************************/
char *lengthResponse(char *filePath){
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
 * This method will create our lookup table
 *************************************************/
void typeTable(){
	/* html extention */
	typePairs[0].extention = "html";
	typePairs[0].type = "text";
	/* txt extention */
	typePairs[1].extention = "txt";
	typePairs[1].type = "text";
	/* jpeg extention */
	typePairs[2].extention = "jpeg";
	typePairs[2].type = "image";
	/* pdf extention */
	typePairs[3].extention = "pdf";
	typePairs[3].type = "image";
}

/**************************************************
 * Return content-type header
 *************************************************/
char *typeResponse(char *filename){
	char *typeHeader = (char *)malloc(sizeof(char)*(50));
	char *type = (char *)malloc(sizeof(char)*(10));
	char *ext = (char *)malloc(sizeof(char)*(10));

    // Read file extention and remove the . off the char pointer.
	char *extn = strrchr(filename, '.');
    char *removeDot = extn + 1;
    printf("Extention: %s\n\n", removeDot);

    // Loop to find the pair of content type
	int i;
	for(i = 0; i < NUM_TYPES; i++){
		if(!strcmp(removeDot, typePairs[i].extention)){
            type = typePairs[i].type;
            ext = typePairs[i].extention;
		}
	}
    
	// Construct the header
	strcpy(typeHeader, "Content-Type: ");
    strcat(typeHeader, type);
    strcat(typeHeader, "/");
    strcat(typeHeader, ext);
    strcat(typeHeader, "\r\n");
    return typeHeader;
}

/**************************************************
 * Return a string containing http response.
 *************************************************/
char *httpResponse(char *file, char *code, char *option){
	/* Content buffe that will hold http response */
	char *content = (char *)malloc(sizeof(char)*20000);

	/* Instantiate char pointer for all headers */
	char *header = statusResponse(code, option);
	char *nowTime = timeResponse();
	char *modified = modifiedResponse(file);
	char *length = lengthResponse(file);
	char *fileType = typeResponse(file);
	char *filebuffer = fileResponse(file);

	/* Store all the headers into content buffer */
	strcpy(content, header);
	strcat(content, nowTime);
	strcat(content, modified);
	strcat(content, length);
	strcat(content, fileType);
	strcat(content, "\r\n");
    printf("Response Header:\n%s", content);
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

/*******************************************************
 * Parse the first line of http request, return filename
 ******************************************************/
char *parseFile(char *request){
    char method[10];
    int current = 0;
    int startParsing = 0;
    while(request[current] != '/'){
        method[startParsing] = request[current];
        startParsing++;
        current++;
    }
    // printf("Method: %s\n", method);
    current++;
    
    char *name = (char *)malloc(sizeof(char)*50);
    startParsing = 0;
    while(request[current] != ' '){
        name[startParsing] = request[current];
        startParsing++;
        current++;
    }
    printf("File name Parsed %s\n", name);
    
    current++;
    
    char protocol[10];
    startParsing = 0;
    while(request[current] != '\n'){
        protocol[startParsing] = request[current];
        startParsing++;
        current++;
    }
    
    // printf("Protocol Parsed %s\n", protocol);
    return name;
}

char* fileNotFound(char *filename){
    FILE *op = fopen(filename, "rb");
    char *notFound = "index.html";
    char *content;
    if(op==NULL){
        content = httpResponse(notFound, "404", "Not Found");
        return notFound;
    }else{
        content = httpResponse(filename, "200", "OK");
    }
    return content;
}

int main(int argc, char **argv){
    int sockfd = socket(AF_INET,SOCK_STREAM,0);
	pthread_t child[LIST_SIZE]; // an array of threads
    int clientsocket[LIST_SIZE];
    
    printf("\nTHIS IS A HTTP SERVER\n");

	/* Make a lookup table for content-type */
	typeTable();

	/**************************
	 * Header Check
	 ***************************/
	// char *content = httpResponse("index.html", "200", "OK");
	// printf("%s",content);
	/**************************
	 * Header Check End
	 ***************************/

    struct sockaddr_in serveraddr,clientaddr;
    serveraddr.sin_family=AF_INET;
    serveraddr.sin_port=htons(8080);
    serveraddr.sin_addr.s_addr=INADDR_ANY;
    
    bind(sockfd,(struct sockaddr*)&serveraddr,sizeof(serveraddr));
    listen(sockfd,10);

    printf("\nWaiting for connection from client!\n\n");
    
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
        
        // store usr name and socket into a struct
        clientList[count].socket=clientsocket[count];
        clientList[count].name=initialInfo;
        
        // position apart from socket to array
        positionApart=clientsocket[count]-count;
        printf("\nHTTP GET REQUEST \n\n%s\n\n", clientList[count].name);
        
        // Initial test response
        char *filename = parseFile(initialInfo);
        char *content = httpResponse(filename, "200", "OK");
        //char *content = httpResponse(filename, "200", "OK");
        
        send(clientsocket[count],content,strlen(content)+1,0);
        //free(content);
        
        // Create a thread for both send and receive
        pthread_create(&child[count],NULL,handleclient,&clientsocket[count]);
        // Detach thread after done
        pthread_detach(child[count]);
        // increase count for next connection
        count++;
    }
}
