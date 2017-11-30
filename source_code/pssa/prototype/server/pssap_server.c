/**
 * @file    pssap_server.c
 * @author  SecuritySchool  
 * @brief   Airplane Prototype - Server
 */
 
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h> // socket
#include <arpa/inet.h>  // inet_addr
#include <unistd.h>     // write
#include <pthread.h>    // thread
#include <signal.h>     // signal

///////////////////////////////////////////////////////////////////////////////

// Message Buffer Size
#define PSSAPS_MAX_MSG   1024

///////////////////////////////////////////////////////////////////////////////

// Client handler function
void *pssaps_client_handler(void *); 

///////////////////////////////////////////////////////////////////////////////

// Main function
int main(int argc, char *argv[])
{
    int sock_desc       = 0;
    int cli_sock        = 0;
    int sock_opt        = 1;
    int sock_len        = 0;
    struct sockaddr_in serv_addr;
    struct sockaddr_in cli_addr;
    pthread_t handler_thread;
     
    // Create socket to listen
    sock_desc = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_desc == -1)
    {
        
        perror("[pssaps] Failed to create socket!");
        return 1;
    }

    // Security School A/S
    sock_opt = 1;
    setsockopt( sock_desc, SOL_SOCKET, SO_REUSEADDR, &sock_opt, sizeof(sock_opt) );

    // Security School A/S
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        perror("[pssaps] Failed to signal()");
        return 1;
    }
     
    // Set address 
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons( 8888 );
    sock_len = sizeof(struct sockaddr_in);
     
    // Bind socket and address
    if( bind(sock_desc,(struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("[pssaps] Failed to bind socket!");
        return 1;
    }
     
    // Listen incoming connection
#define PSSAPS_ADDR_BACKLOG    100
    listen(sock_desc, PSSAPS_ADDR_BACKLOG);   
    printf("[pssaps] Waiting for client ...\n");
     
    // Accept connection from client
    while( (cli_sock = accept(sock_desc, (struct sockaddr *)&cli_addr, (socklen_t*)&sock_len)) )
    {
        printf("[pssaps][%d client] Accepted\n", cli_sock);        
        // Create thread to handler client
        if( pthread_create( &handler_thread, NULL,  pssaps_client_handler, (void*)(intptr_t)cli_sock) < 0)
        {
            perror("[pssaps] Failed to create thread!");
            return 1;
        }
    }
     
    printf("[pssaps] exit\n");  
    return 0;
}


// Client handler function
void * pssaps_client_handler(void *sock_desc)
{
    // Get the socket descriptor
    int sock        = (int)(intptr_t)sock_desc;
    int read_size   = 0;
    int reply_cnt   = 0;
    char cli_msg[PSSAPS_MAX_MSG]   = {0x00, };
    char serv_msg[PSSAPS_MAX_MSG]   = {0x00, };
 
    // Receive a message from client
    read_size = recv(sock, cli_msg, PSSAPS_MAX_MSG-1, 0);
    if(0 < read_size)
    {
        cli_msg[PSSAPS_MAX_MSG-1] = 0;
        printf("[pssaps][%d client] Received : \"%s\"\n", sock, cli_msg);
        // Send a reply continually
        while(1)
        {
            // Sleep for 2 sec
            sleep(2);
            // Make reply
            sprintf(serv_msg, "Reply #%d", ++reply_cnt);
            // Send reply to client
            if(0 < send(sock, serv_msg, strlen(serv_msg), 0))
            {
                printf("[pssaps][%d client] Sent : \"%s\"\n", sock, serv_msg);
            }
            else
            {
                perror("[pssaps] Failed to send!");
                break;
            }
        }
        // In this prototype, never reach here except error
    }
    else if(0 == read_size)
    {
        printf("[pssaps][%d client] Disconnected\n", sock);
    }
    else // read_size < 0
    {
        perror("[pssaps] Failed to recevie");
    }
         
    // Free socket
    close(sock);

    printf("[pssaps][%d client] Exit\n", sock);    
    return 0;
}

///////////////////////////////////////////////////////////////////////////////