#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#define LIST_SIZE 2000
#define NUM_TYPES 5

/* Decalre the file name to send in the case of 404 not found */
char *NotFound404 = "notFound.txt";

char *httpResponse(char *file, char *code, char *option);
char *parseFile(char *request);
char *typeResponse(char *filename);
char *lengthResponse(char *filePath);
char *fileResponse(char *filePath);
char *modifiedResponse(char *filePath);
char *timeResponse();
char *statusResponse(char *code, char *type);
char *fileNotFound(char *filename, char *dateParsed);
char *path;
void sendingFileChunks(char *filename, int clientsocket);
FILE* logFile;
int loggingFile;
char *logFilename;
void send501(int socket);
time_t modified;
int modifiedRequest;
char *notFound;
int checkClose(char *message);

struct client{
    int socket;
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
    char *header = (char *)malloc(sizeof(char)*30);
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
    char *timeString = (char *)malloc(sizeof(char)*50);
    char timeFormat[50];
    
    /* Date Format: Weekday, date month year time GMT. */
    time(&now);
    strftime(timeFormat, 50, "%a, %d %b %Y %X GMT\r\n", gmtime(&now));
    
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
    char dateString[50];
    char dateString2[50];
    
    modified = NULL;
    /* Date Format: Weekday, date month year time GMT. */
    stat(filePath, &attr);
    strftime(dateString, 50, "%a, %d %b %Y %X GMT\r\n", gmtime(&attr.st_ctime));
    modified = attr.st_ctime;
    strftime(dateString2, 50, "%a, %d %b %Y %X GMT\r\n", gmtime(&modified));
    printf("\nGOT THE LAST MODIFIED DATE: %s\n",dateString2);
    
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
    /* jpg extention */
    typePairs[4].extention = "jpg";
    typePairs[4].type = "image";
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
    // printf("Extention: %s\n\n", removeDot);
    
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
    char *content = (char *)malloc(sizeof(char)*5000);
    
    /* Instantiate char pointer for all headers */
    char *header = statusResponse(code, option);
    char *nowTime = timeResponse();
    char *modified = modifiedResponse(file);
    char *length = lengthResponse(file);
    char *fileType = typeResponse(file);
    //char *filebuffer = fileResponse(file);
    
    /* Store all the headers into content buffer */
    strcpy(content, header);
    strcat(content, nowTime);
    strcat(content, modified);
    strcat(content, length);
    strcat(content, fileType);
    //strcat(content, "Connection: close\r\n");
    strcat(content, "\r\n");
    printf("Response Header:\n%s", content);
    //strcat(content, filebuffer);
    
    /* Free everything since its all stored in content */
    free(header);
    free(nowTime);
    free(modified);
    free(length);
    free(fileType);
    //free(filebuffer);
    
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
    //    printf("Method: %s\n", method);
    current++;
    
    startParsing = 0;
    char name[50];
    int count = 0;
    while(request[current] != ' '){
        //memcpy(&name[startParsing], &request[current], 1);
        name[startParsing] = request[current];
        count++;
        startParsing++;
        current++;
    }
    char *fileName = malloc(sizeof(char)*50);
    
    memcpy(&fileName[0],&name[0],count);
    fileName[count] = '\0';
    //    printf("File name Parsed: %s\n\n", fileName);
    
    char *filePath = (char *)malloc(sizeof(char)*500);
    strcpy(filePath, path);
    strncat(filePath, fileName, count);  // Adding file name to path
    printf("Path %s", filePath);
    
    free(fileName);
    
    return filePath;
}

/**********************************************
 * Finding file and determining OPcode
 ********************************************/
char* fileNotFound(char *filename, char *dateParsed){
    FILE *op = fopen(filename, "rb");
    char *content;
    if(op==NULL){
        content = httpResponse(notFound, "404", "Not Found");
        printf("file was not found\n"); //
        return content;
    }else{
        double diff = 0;
        modifiedResponse(filename);
        char timeFormat[500];
        char timeFormat2[500];
        struct tm tm;
        strptime(dateParsed,"%a, %d %b %Y %X GMT",&tm);
        tm.tm_hour -= 5;
        printf("parsed if modified since: %s\n", dateParsed);
        time_t ifModifiedRequest = mktime(&tm);
        if (modifiedRequest){
            char timeFormat[500];
            char timeFormat2[500];
            struct tm tm;
            strptime(dateParsed,"%a, %d %b %Y %X GMT",&tm);
            tm.tm_hour -= 4;
            printf("parsed if modified since: %s\n", dateParsed);
            time_t ifModifiedRequest = mktime(&tm);
            strftime(timeFormat, 50, "%a, %d %b %Y %X GMT\r\n", gmtime(&modified));
            strftime(timeFormat2, 50, "%a, %d %b %Y %X GMT\r\n", gmtime(&ifModifiedRequest));
            printf("COMPARING:\n%s\nTo: \n%s\n", timeFormat2,timeFormat);
            
            
            diff = difftime(ifModifiedRequest,modified);
            printf("difference in time: %f\n", diff);
            //        diff = difftime(ifModifiedRequest,ifModifiedRequest);
            if (diff < 0) {
                content = httpResponse(filename, "200", "OK");
            }
            else {
                content = httpResponse(filename, "304", "Not Modified");
            }
            modifiedRequest = 0;
        }
        
        else {
            content = httpResponse(filename, "200", "OK");
        }
        
        return content;
    }
}

void sendingFileChunks(char *filename, int clientsocket){
    /************************************
     *Testing method for sending parts of file
     **************************************/
    FILE *op = fopen(filename, "rb");
    int SIZE = 10000;
    if(op==NULL){
        op = fopen(notFound, "rb");
        printf("File not found");
    }
    
    fseek(op, 0, SEEK_END);
    long fsize = ftell(op);
    fseek(op, 0, SEEK_SET);
    char *buffer = (char*)malloc(sizeof(char)*SIZE);
    
    int readFile = 0;
    while(readFile <= fsize){
        //        fgets(buffer, SIZE, op);
        fread(buffer, SIZE, 1, op);
        send(clientsocket,buffer,SIZE,0);
        if (fsize - readFile < SIZE){
            //            fgets(buffer, fsize - readFile + 1, op);
            fread(buffer, fsize - readFile, 1, op);
            send(clientsocket,buffer,fsize -readFile,0);
            printf("Bytes sent: %d\n",(fsize -readFile));
            break;
        }
        readFile += SIZE;
        
        printf("Bytes sent: %d\n",readFile);
    }
    free(buffer);
    fclose(op);
}

/* Handle receiving operation */
void * handleclient(void * arg){
    int clientsocket = *(int *)arg;
    printf("Client %d Connected\n\n",clientList[clientsocket-positionApart].socket);
    
    struct timeval timeout;
    timeout.tv_sec=20000;
    timeout.tv_usec=0;
    
    while (1) {
        char *chatRecv;
        chatRecv = (char *)malloc(sizeof(char*)*5000);
        if(recv(clientsocket,chatRecv, 5000,0)>0){
            
            printf("\n\nSecond request \n\n%s\n\n", chatRecv);
            
            char *filename;
            char *content;
            filename = "";
            content = "";
            // Initial test response
            if(strstr(chatRecv, "GET")){
                time_t ifModifiedRequest;
                char *ifModified = (char *)malloc(sizeof(char*)*100);
                char *dateParsed = (char *)malloc(sizeof(char*)*100);
                if((ifModified = strstr(chatRecv, "If-Modified-Since")) != NULL){
                    modifiedRequest = 1;
                    int startOfDate = 19;
                    while(*(ifModified + startOfDate)!='\n' && *(ifModified + startOfDate)!='\r'){
                        *(dateParsed + startOfDate - 19) = *(ifModified + startOfDate);
                        startOfDate++;
                    }
                    struct tm tm;
                    strptime(dateParsed,"%a, %d %b %Y %X",&tm);
                    ifModifiedRequest = mktime(&tm);
                    
                }
                filename = parseFile(chatRecv);
                content = fileNotFound(filename,dateParsed);
                printf("Client %d \n", clientsocket);
                
                if (loggingFile) {
                    logFile = fopen(logFilename, "a+");
                    fwrite(chatRecv, 1, strlen(chatRecv), logFile);
                    printf("Writing Entry in log file: %s\n",logFilename);
                    fwrite(content, 1, strlen(content), logFile);
                    fclose(logFile);
                }
                
                send(clientsocket,content,strlen(content),0);
                sendingFileChunks(filename, clientsocket);
                
                if(checkClose(chatRecv)){
                    free(content);
                    free(chatRecv);
                    free(filename);
                    close(clientsocket);
                    break;
                }
                
                free(chatRecv);
                free(content);
                free(filename);
            }else{
                
                if (loggingFile) {
                    logFile = fopen(logFilename, "a+");
                    fwrite(chatRecv, 1, strlen(chatRecv), logFile);
                    printf("Writing Entry in log file: %s\n",logFilename);
                    fclose(logFile);
                }
                
                printf("\nSecond Request: INVALID\n");
                send501(clientsocket); // sending 501 if not a GET request
                close(clientsocket);
            }
            
        }else{
            printf("Waiting for new connection time out.");
            close(clientsocket);
            break;
        }
    }
}

int checkClose(char *message){
    char *connect = strstr(message, "Connection:");
    if(!strncmp(connect, "Connection: Close", 17) || !strncmp(connect, "Connection: close", 17)){
        return 1;
    }
    return 0;
}

void send501(int socket){
    char *response501 = (char *)malloc(sizeof(char*)*500);
    strcpy(response501,"HTTP/1.1 501 Not Implemented\r\nConnection: close\r\n\r\n");
    send(socket,response501,strlen(response501),0);
    printf("\n501 Not Found\n%s", response501);
    if (loggingFile){
        logFile = fopen(logFilename, "a+");
        printf("Writing Entry in log file: %s\n",logFilename);
        fwrite(response501, 1, strlen(response501), logFile);
        fclose(logFile);
    }
    free(response501);
}

int main(int argc, char **argv){
    notFound = (char *)malloc(sizeof(char*)*50);
    int port = 8080;
    path = (char *)malloc(sizeof(char*)*50);
    logFilename=(char *)malloc(sizeof(char*)*50);
    loggingFile=0;
    
    int i;
    for (i = 1; i < argc; i++) {
        if (!strncmp(argv[i], "-p", 2)){
            char portString[5];
            strcpy(portString,*(argv + i + 1));
            int portNum = atoi(portString);
            if (portNum >= 0 && portNum <= 65535)
                port = portNum;
            else
                printf("Not a valid port number.\n Port number set to 8080");
        }
        if (!strncmp(*(argv + i), "-docroot", 8)){
            strcat(path,*(argv + i + 1));
            printf("Path: %s\n", path);
            
        }
        if (!strncmp(*(argv + i), "-log", 4)){
            strcpy(logFilename, path);
            strcat(logFilename,*(argv + i + 1));
            loggingFile = 1;
            printf("Storing logs in file: %s\n",logFilename);
        }
    }
    
    strcpy(notFound, path);
    strcat(notFound, "notFound.html");
    
    printf("Port num: %d", port);
    
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
    serveraddr.sin_port=htons(port);
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
        printf("New CLient Socket: %d\n", clientsocket[count]);
        
        // Receive a usr name from client
        char *initialInfo;
        initialInfo=(char *)malloc(sizeof(char*)*5000);
        recv(clientsocket[count],initialInfo, 5000,0);
        
        // store usr name and socket into a struct
        clientList[count].socket=clientsocket[count];
        
        // position apart from socket to array
        positionApart=clientsocket[count]-count;
        printf("\t\t\tNEW HTTP GET REQUEST \n\n%s\n", initialInfo);
        char *filename;
        char *content;
        filename = "";
        content = "";
        // Initial test response
        if(!strncmp(initialInfo, "GET", 3)){
            time_t ifModifiedRequest;
            char *ifModified = (char *)malloc(sizeof(char*)*100);
            char *dateParsed = (char *)malloc(sizeof(char*)*100);
            if((ifModified = strstr(initialInfo, "If-Modified-Since")) != NULL){
                modifiedRequest = 1;
                int startOfDate = 19;
                while(*(ifModified + startOfDate)!='\n' && *(ifModified + startOfDate)!='\r'){
                    *(dateParsed + startOfDate - 19) = *(ifModified + startOfDate);
                    startOfDate++;
                }
            }
            filename = parseFile(initialInfo);
            content = fileNotFound(filename,dateParsed);
            
            if (loggingFile){
                logFile = fopen(logFilename, "a+");
                fprintf(logFile,"%s",initialInfo);
                fwrite(initialInfo, 1, strlen(initialInfo), logFile);
                printf("Writing Entry in log file: %s\n",logFilename);
                fwrite(content, 1, strlen(initialInfo), logFile);
                fclose(logFile);
            }
            
            send(clientsocket[count],content,strlen(content),0);
            sendingFileChunks(filename, clientsocket[count]);
            
            if(checkClose(initialInfo)){
                free(content);
                free(initialInfo);
                free(filename);
                close(clientsocket[count]);
                continue;
            }
            
            free(content);
            free(initialInfo);
            free(filename);
            // Create a thread for both send and receive
            pthread_create(&child[count],NULL,handleclient,&clientsocket[count]);
            // Detach thread after done
            pthread_detach(child[count]);
        }else{
            if (loggingFile){
                logFile = fopen(logFilename, "a+");
                fwrite(initialInfo, 1, strlen(initialInfo), logFile);
                printf("Writing Entry in log file: %s\n",logFilename);
                fclose(logFile);
            }
            printf("Does not recognize this request./n");
            send501(clientsocket[count]); // sending 501 if not a GET request
            close(clientsocket[count]);
        }
        
        // increase count for next connection
        count++;
    }
}
