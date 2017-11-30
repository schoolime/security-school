/**
 * @file    pssap_client.c
 * @author  SecuritySchool  
 * @brief   Airplane Prototype - Client
 */

#include <stdio.h>      //printf
#include <string.h>     //strlen
#include <unistd.h>     // io
#include <sys/socket.h> //socket
#include <arpa/inet.h>  //inet_addr
 

///////////////////////////////////////////////////////////////////////////////

int main(int argc , char *argv[])
{
    int sock = -1;
    struct sockaddr_in serv_addr;
    static const char * cli_msg = "Hello Server?";
#define PSSAPC_MAX_MSG  1024
    char serv_msg[PSSAPC_MAX_MSG] = {0x00, };
    int ret = -1;
     
    // Create Socket
    sock = socket(AF_INET , SOCK_STREAM , 0);
    if (sock != -1)
    {
        // TODO: change IP address as your updat server's
        // serv_addr.sin_addr.s_addr = inet_addr("192.168.177.100");
        serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons( 8888 );

        do{     
            // Connect to server
            ret = connect(sock , (struct sockaddr *)&serv_addr , sizeof(serv_addr));
            if (0 <= ret)
            {
                // Send a message to the server
                ret = send(sock , cli_msg , strlen(cli_msg) , 0);
                if(0 >= ret)
                {
                    perror("[pssapc] Send failed");
                    break;
                }
                printf("[pssapc] Sent : \"%s\"\n", cli_msg); 
                 
                while(1)
                {
                    // Receive a reply from the server
                    ret = recv(sock , serv_msg , PSSAPC_MAX_MSG-1 , 0);
                    if(0 >= ret)
                    {
                        printf("[pssapc] Failed to recev()\n");
                        break;
                    }
                    serv_msg[ret] = 0;
                
                    // Print reply from the server    
                    printf("[pssapc] Received : \"%s\"\n", serv_msg);
                }

                // In this prototype, never reach here except error
            }
            else
            {
                perror("[pssapc] Failed to connect");
                break;
            }
        }while(0);
            
        // Close socket
        close(sock);

    }
    else
    {
        printf("[pssapc] Could not create socket");
        return -1;
    }

    printf("[pssapc] Exit\n");
    
    return 0;
}

///////////////////////////////////////////////////////////////////////////////