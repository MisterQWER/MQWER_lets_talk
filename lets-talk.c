#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <pthread.h>
#include <stdbool.h>
#include "list.h"

List* sending_list;
List* receive_list;
char** send_strings;
int s_count = 0;
char** recv_strings;
int r_count = 0;
char buff[4098];
int sockfd, n, portno;
struct sockaddr_in serv_addr, cliaddr;
struct hostent *server;
bool isInputDone = false;
bool isRecvDone = false;
bool isPrintDone = false;
pthread_t threads[4];
char STATUS_INTRO[10] = "!@#$^&*(01";
char STATUS_OUTRO[30] = "!@#$^&*(01!@#$^&*(01!@#$^&*(01";
bool isOutReceived = false;





// the function which clears the allocated memory
void free_resources()   {

    if(send_strings != NULL)    {
        for(int i = 0; i < s_count; i++)    {
            free(send_strings[i]);
        }
        free(send_strings);
    }
    
    if(recv_strings != NULL)    {
        for(int i = 0; i < r_count; i++)    {
            free(recv_strings[i]);
        }
        free(recv_strings);
    }
    close(sockfd);
}




// simple encryption function, adds 1 to the current ascii value
void encrypt(char dest[]) {

    for(int i = 0; i < strlen(dest); i++) {
        if(dest[i] != '\0') dest[i] = (char)(((int) dest[i] + 1) % 256);
    }
}


void decrypt(char dest[]) {
    for(int i = 0; i < strlen(dest); i++) {
        if(dest[i] != '\0') dest[i] = (char)(((int) dest[i] - 1) % 256);
    }
}


/**
 * handles if the str is !exit
*/
void processExit(const char* str)    {

    // check for exit
    if(strncmp(str, "!exit", 5) == 0)  {
        if(str[5] == 10){
            isInputDone = true;
            pthread_cancel(threads[2]);
        }
    }
}



/**
 * handles if the str is !status
*/
void processStatus(const char *str) {

    int len = sizeof(cliaddr);
    if(strncmp(str, "!status", 7) == 0 && str[7] == 10) {
        
        sendto(sockfd, STATUS_INTRO, 4098,
            MSG_CONFIRM, (const struct sockaddr *) &cliaddr, 
            len);

        sleep(2);

        if(isOutReceived)  {
            printf("ONLINE\n");
        }
        else if(!isOutReceived)  {
            printf("OFFLINE\n");
        }
        fflush(stdout);
    }
}



// a function for printing errors
void error(const char* msg)   {
    perror(msg);
    perror("Usage: ./lets-talk <localport> <remote IP> <remote port>");
    exit(1);
}



/**
 * start routine of input thread
*/
void *inputThread()  {

    while(!isInputDone)    {
        //input to buff
        bzero(buff, 4098);
        fgets(buff, 4098, stdin);

        char temp[4098];
        for(int i = 0; i < 4098; i++)   {
            temp[i] = buff[i];
        }
        processStatus(temp);
        
        if(strncmp(temp, "!status", 7) != 0)    {
            // copy buff to storage
            send_strings[s_count] = malloc(sizeof(buff));
            send_strings[s_count] = strcpy(send_strings[s_count], buff);
            
            // add new element of storage to the list
            List_add(sending_list, send_strings[s_count]);
            s_count++;
            processExit(buff);
        }
    }
    pthread_exit(NULL);
}



/**
 * start routing of sending thread
*/
void *sendThread()  {
    
    int curr_size = s_count;
    int len;
    len = sizeof(cliaddr); 

    while(1)    {
        if(s_count != curr_size)    {
            encrypt((char *) List_curr(sending_list));
            sendto(sockfd, (char *) List_curr(sending_list), 4098,
            MSG_CONFIRM, (const struct sockaddr *) &cliaddr, 
            len);
            curr_size = s_count;
            if(isInputDone) {
                free_resources();
                exit(EXIT_SUCCESS);
            }
        }
    }
    pthread_exit(NULL);
}




/**
 * start routine of receiveing thread
*/
void *recThread()  {
    
    int len;
    len = sizeof(cliaddr); 

    while(!isRecvDone)    {
        
        bzero(buff, 4098);
        // recieve from server
        n = recvfrom(sockfd, (char *)buff, 4098, 
                MSG_WAITALL, (struct sockaddr *) &cliaddr,
                &len);
        buff[n] = '\0';

        if(strncmp(buff, STATUS_OUTRO, 30) == 0)    {
            isOutReceived = true;
            sleep(5);
            isOutReceived = false;
        }

        //if !status and its key were not recvievd (normal procedure)
        else if(strncmp(buff, STATUS_INTRO, 10) != 0 && 
                    strncmp(buff, STATUS_OUTRO, 30) != 0)  {
            decrypt(buff);
            
            processExit(buff);
            if(isInputDone) isPrintDone = true;

            recv_strings[r_count] = malloc(sizeof(buff));
            recv_strings[r_count] = strcpy(recv_strings[r_count], buff);
            List_add(receive_list, recv_strings[r_count]);
            r_count++;
        }

        // if !status is recvievd
        else if(strncmp(buff, STATUS_INTRO, 10) == 0)   {

            sendto(sockfd, STATUS_OUTRO, 4098,
                MSG_CONFIRM, (const struct sockaddr *) &cliaddr, 
                len);
        }
    }
    pthread_exit(NULL);
}




/**
 * start routine of print thread
*/
void *printThread() {
    int curr_item = r_count;
    while(1)    {
        if(curr_item != r_count)    {

            char * str = (char *) List_curr(receive_list);
            if(strncmp((char *) List_curr(receive_list), "!status", 7) != 0)  {
                printf("%s",(char *) List_curr(receive_list));
            }
            else if(str[7] != 10)   {
                printf("%s",(char *) List_curr(receive_list));

            }
            char* current = (char *) List_remove(receive_list);
            free(current);
            r_count--;
            
            fflush(stdout);
            if(isPrintDone) {
                free_resources();
                exit(EXIT_SUCCESS);
            }
        }
    }
    pthread_exit(NULL);
}




// The main threads handle the initialization of the socket and clinet/server addresses
int main(int argc, char* argv[]) {

        
    if(argc != 4)   {
        error("Incorrect number of arguments\n");
    }
    portno = atoi(argv[1]);
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if(sockfd < 0)  error("ERROR OPENING CLIENT SOCKET\n");

    server = gethostbyname(argv[2]);
    if(server == NULL)  error("CANT connect to server");

    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *) server->h_addr, (char *) &serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port = htons(portno);

    bzero((char *) &cliaddr, sizeof(cliaddr));
    cliaddr.sin_family = AF_INET;
    cliaddr.sin_port = htons(atoi(argv[3]));

    if(bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)  {
        error("Biding failed\n");
    }


    sending_list = List_create();
    receive_list = List_create();
    send_strings = malloc(4000 * sizeof(char *));
    recv_strings = malloc(4000 * sizeof(char *));

    // input thread
    if(pthread_create(&threads[0], NULL, inputThread, NULL))   {
            error("BAD THREAD\n");
    }

    // send thread
    if(pthread_create(&threads[1], NULL, sendThread, NULL))   {
            error("BAD THREAD\n");
    }

    // receive thread
    if(pthread_create(&threads[2], NULL, recThread, NULL))  {
        error("BAD THREAD\n");
    }

    // print thread
    if(pthread_create(&threads[3], NULL, printThread, NULL))    {
        error("BAD THREAD\n");
    }


    pthread_join(threads[0], NULL);
    pthread_join(threads[1], NULL);
    pthread_join(threads[2], NULL);
    pthread_join(threads[3], NULL);

    free_resources();

    return 0;
}