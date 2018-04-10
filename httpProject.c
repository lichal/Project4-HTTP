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

int broadcast(int socketNum,char *chatBroad){
    int broadcast;
    for (broadcast=0;broadcast<LIST_SIZE;broadcast++){
        printf("Broadcasting!\n");
        int socket = clientList[broadcast].socket;
        send(socket,chatBroad,strlen(chatBroad)+1,0);
    }
    return 1;
}

char *constructlist(int socketNum){
    int  listing;
    char *charList;
    charList=(char *)malloc(sizeof(char*)*5000);
    strcat(charList,"Client List\n");
    for(listing=0;listing<LIST_SIZE;listing++){
        if(clientList[listing].socket>0){
            strcat(charList,"\t\t");
            strcat(charList,clientList[listing].name);
            strcat(charList,"\n");
        }
    }
    return charList;
}

int individual(char *clientname){
    int find;
    int found = 0;
    for (find=0;find<LIST_SIZE;find++){
        if(clientList[find].socket>0){
            int len=strlen(clientList[find].name);
            int match =strncmp(clientname,clientList[find].name,len);
            if(!match){
                found=clientList[find].socket;
            }
        }
    }
    return found;
}

char* getName(char *clientname){
    int find;
    char* found = "";
    for(find=0;find<LIST_SIZE;find++){
        if(clientList[find].socket>0){
            int len=strlen(clientList[find].name);
            int match =strncmp(clientname,clientList[find].name,len);
            if(!match){
                return clientList[find].name;
            }
        }
    }
    return found;
}

int removeclient(char *clientname){
    int find;
    int found = 0;
    for(find=0;find<LIST_SIZE;find++){
        if(clientList[find].socket>0){
            int len=strlen(clientList[find].name);
            int match =strncmp(clientname,clientList[find].name,len);
            if(!match){
                found=clientList[find].socket;
                clientList[find].socket = 0;
            }
        }
    }
    return found;
}

/* Handle receiving operation */
void * handleclient(void * arg){
    int clientsocket = *(int *)arg;
    printf("Client %d Connected\n",clientList[clientsocket-positionApart].socket-positionApart);
    
    while (1) {
        char *chatRecv;
        chatRecv = (char *)malloc(sizeof(char*)*5000);
        if(recv(clientsocket,chatRecv, 5000,0)>0){
            printf("From %s: %s\n",clientList[clientsocket-positionApart].name,chatRecv);
            
            char * name = clientList[clientsocket-positionApart].name;
            // Client requested a broadcast
            // two part broadcast
            if(!strncmp(chatRecv,"chat -b",7)){
                
                /* setting up messages */
                char  sender[50];
                char  response[5000];
                char  message[5000];
                strcpy(response,"");
                strcpy(message,"");
                memcpy(message,"Broadcast From ", 15);
                strcat(message,name);
                strcat(message,": ");
                strcat(response,"What would you like to Broadcast?");
                strcpy(chatRecv,"");
                printf("%s\n", response);
                send(clientsocket,response,strlen(response),0);//asking sender what they want to broadcast
                recv(clientsocket,chatRecv, 5000,0);
                strcat(message,chatRecv);
                
                
                broadcast(clientsocket,message);//sending broadcast
                strcpy(chatRecv,"");
            }
            // Client requested a list of all client
            if(!strncmp(chatRecv,"chat -l",7)){
                char *tempChar=constructlist(clientsocket);
                printf("Send List %s",tempChar);
                send(clientsocket,tempChar,strlen(tempChar)+1,0);
                strcpy(chatRecv,"");
            }
            // Client requested to send to another people individually
            // two part message send
            if(!strncmp(chatRecv,"chat -i",7)){
                printf("Send to an individual");
                char *usr=(char *)malloc(sizeof(char*)*5000);
                strcpy(usr,chatRecv);
                usr +=8;
                int indsocket=individual(usr);
                if(indsocket==0){
                    printf("No such client\n");
                }else{
                    /* setting up messages */
                    char  sender[50];
                    char  response[5000];
                    char  message[5000];
                    strcpy(response,"");
                    strcpy(message,"");
                    memcpy(message,"From ", 5);
                    strcat(message,name);
                    strcat(message,": ");
                    char * recvName = getName(usr);
                    strcat(response,"What would you like to send ");
                    strcat(response, recvName);
                    strcpy(chatRecv,"");
                    printf("%s\n", response);
                    send(clientsocket,response,strlen(response),0); // asking sender what they want to send
                    recv(clientsocket,chatRecv, 5000,0);
                    strcat(message,chatRecv);
                    send(indsocket,message,strlen(message),0); // sending message to receiver
                }
            }
            if(!strncmp(chatRecv,"chat -t",7)){ // 2 step chat kick
                char *usr=(char *)malloc(sizeof(char*)*5000);
                strcpy(usr,chatRecv);
                usr +=8;
                char * recvName = getName(usr);
                char  response[5000];
                while(strncmp(chatRecv,"7777",4)){
                    
                    strcpy(response,"");
                    memcpy(response,"Enter Admin Password\n", 20); //prompting for password
                    send(clientsocket,response,strlen(response),0);
                    strcpy(chatRecv,"");
                    recv(clientsocket,chatRecv, 5000,0);
                }
                int closesocket=removeclient(usr);
                send(closesocket,"Kick",5,0);
                strcpy(response,"Successfully kicked ");
                strcat(response, recvName);
                send(clientsocket,response, strlen(response),0);
                
                int updateUsers;
                strcpy(chatRecv,"");
                close(closesocket);
            }
            
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

int main(int argc, char **argv){
    int sockfd = socket(AF_INET,SOCK_STREAM,0);
    
    printf("\nTHIS IS A HTTP SERVER\n");


	/**************************
	 * Header testing
	 ***************************/
    char content[1500];
	char *header = statusRequest("200", "OK");
	char *nowTime = timeRequest();
	char *modified = modifiedRequest("index.html");
	strcpy(content, header);
	strcat(content, nowTime);
	strcat(content, modified);
	strcat(content, "\r\n");
	printf("Content \n%s", content);
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
