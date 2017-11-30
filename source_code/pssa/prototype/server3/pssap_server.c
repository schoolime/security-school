/**
 * @file    pssaps.c
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

#include <sys/inotify.h>   // inotify
#include <dirent.h>         // DIR

#include <sys/stat.h>   // stat
#include <fcntl.h>      // open
#include <sys/mman.h>   // mmap
#include <openssl/md5.h>    // MD5
#include <linux/limits.h>   // PATH_MAX

///////////////////////////////////////////////////////////////////////////////

// Message Buffer Size
#define PSSAPS_MAX_MSG   2048

// Maxinum count of files to watch
#define PSSAPS_MAX_WATCHFILE  512
#define PSSAPS_INOTIFY_SIZE  (sizeof(struct inotify_event))
#define PSSAPS_INOTIFY_BUFFLEN    (PSSAPS_MAX_WATCHFILE*(PSSAPS_INOTIFY_SIZE+16))
// Buffer to save file list in watched directory
#define PSSAPS_WATCHED_BUFFLEN    MD5_DIGEST_LENGTH*PSSAPS_MAX_WATCHFILE

///////////////////////////////////////////////////////////////////////////////

// Message to send
static char serv_msg[PSSAPS_WATCHED_BUFFLEN] = {0x00, };
int serv_msg_len = 0;
// Mutex for condition to send
pthread_mutex_t send_lock   = PTHREAD_MUTEX_INITIALIZER;
// Condition to send
pthread_cond_t  send_event  = PTHREAD_COND_INITIALIZER;
// Path to monitor
static const char path_to_watch[] = "./watched_dir"; 

///////////////////////////////////////////////////////////////////////////////

// Client handler function
void *pssaps_client_handler(void *sock_desc);

// Read file list in watched directory
int pssaps_make_sendmsg(const char * watch_path);
// Directory monitor function
void *pssaps_directory_monitor(void *arg);

// Print buffer as hexidemal
void pssaps_print_hex(const unsigned char * buf, int size);
// Get MD5
int pssaps_get_md5(const char *file_path, unsigned char *md5);


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
    
    // Get lock to make send message
    pthread_mutex_lock(&send_lock);
    // Read file list from watched directory and make reply to send
    pssaps_make_sendmsg(path_to_watch);
    // Unlock
    pthread_mutex_unlock(&send_lock);


    // Create thread to monitor directory
    if( pthread_create( &handler_thread, NULL,  pssaps_directory_monitor, 0) < 0)
    {
        perror("[pssaps] Failed to pthread_create() to monitor");
        return 1;
    }

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
    //Get the socket descriptor
    int sock        = (int)(intptr_t)sock_desc;
    int read_size   = 0;
    char cli_msg[PSSAPS_MAX_MSG]   = {0x00, };
 
    // Receive a message from client
    read_size = recv(sock, cli_msg, PSSAPS_MAX_MSG-1, 0);
    if(0 < read_size)
    {
        cli_msg[PSSAPS_MAX_MSG-1] = 0;
        printf("[pssaps][%d client] Received : \"%s\"\n", sock, cli_msg);
        // Send a reply continually
        do
        {
            // Get lock for send
            pthread_mutex_lock(&send_lock);

            // Send reply to client
            if(0 < send(sock, serv_msg, serv_msg_len, 0))
            {
                printf("[pssaps][%d client] Sent : \"", sock);
                pssaps_print_hex((const unsigned char*)serv_msg, serv_msg_len);
            }
            else
            {
                pthread_mutex_unlock(&send_lock);
                perror("[pssaps] Failed to send!");
                break;
            }

            pthread_cond_wait(&send_event, &send_lock);

            pthread_mutex_unlock(&send_lock);

        }while(1);

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


// Read file list in watched directory
int pssaps_make_sendmsg(const char * watch_path)
{
    DIR *dp;
    struct dirent *ep;     
    int ret = 0;
    char file_path[PATH_MAX] = {0,};
    int file_start = strlen(watch_path);
    unsigned char md5[MD5_DIGEST_LENGTH+1];

    strcpy(file_path, watch_path);
    file_path[file_start++] = '/';
    file_path[file_start] = 0;

    dp = opendir (watch_path);
    if (dp != NULL)
    {
        serv_msg_len = 0;
        serv_msg[0] = 0;
        // Read item in directory
        while(NULL != (ep = readdir (dp)))
        {
            if(DT_REG == ep->d_type)
            {
                ++ret;
                // Append MD5 of file to buffer to send
                if( serv_msg_len + MD5_DIGEST_LENGTH + 2 < PSSAPS_WATCHED_BUFFLEN )
                {
                    file_path[file_start] = 0;
                    strncat(file_path+file_start, ep->d_name, strlen(ep->d_name));
                    if( 0 == pssaps_get_md5(file_path, md5) )
                    {
                        strncat(serv_msg+serv_msg_len, (const char*)md5, MD5_DIGEST_LENGTH);
                        serv_msg_len += MD5_DIGEST_LENGTH;
                        //serv_msg[serv_msg_len++] = ';';
                    }
                }
            }
        }
        serv_msg[serv_msg_len] = 0;
        closedir (dp);
    }
    else
    {
        perror ("[pssaps] Failed to opendir()");    
    }
    
    return ret;
}


// Directory monitor function
void *pssaps_directory_monitor(void * arg)
{    
    int read_len    = 0;
    int event_index = 0;
    int dir_changed = 0;
    int fd, wd = 0;
    char buffer[PSSAPS_INOTIFY_BUFFLEN];
    struct inotify_event *event = NULL;
    
    printf("[pssaps] monitor thread start\n");

    // Init inotify
    fd = inotify_init();
    if (fd < 0)
    {
        perror("[pssaps] Failed to inotify_init()");
    }

    // Add directory to watch.
    wd = inotify_add_watch(fd, path_to_watch, IN_CLOSE_WRITE|IN_DELETE|IN_MODIFY|IN_MOVED_FROM|IN_MOVED_TO);

    // Start to monitor directory
    do
    {
        event_index = 0;
        dir_changed = 0;

        // Read inotify event, when something changed, read() will return.
        read_len = read(fd, buffer, PSSAPS_INOTIFY_BUFFLEN); 
        if (read_len < 0)
        {
            perror( "[pssaps] Failed to read()" );
        }

        // Handle multiple inotify event
        while(event_index < read_len)
        {
            event = (struct inotify_event *) &buffer[event_index];

            // Hint
            // Do something

            // Check wether some file changed
            if(event->len)
            {
                if( event->mask & IN_CLOSE_WRITE ||
                    event->mask & IN_DELETE ||
                    event->mask & IN_MODIFY ||
                    event->mask & IN_MOVED_FROM ||
                    event->mask & IN_MOVED_TO )
                {
                    printf("[pssaps] watchor detected some changes.\n");
                    dir_changed = 1;
                }
                // Get index for next event
                event_index += PSSAPS_INOTIFY_SIZE + event->len;
            }
            else
            {
                // There is no path
                event_index += PSSAPS_INOTIFY_SIZE;                
            }
        }

        // If some file changed,
        if( 1 == dir_changed )
        {
            // Get lock to make send message
            pthread_mutex_lock(&send_lock);
            // Make reply again
            pssaps_make_sendmsg(path_to_watch);
            // Set event to send to all of client thread
            pthread_cond_broadcast(&send_event);
            // Unlock then client thread will be awaken
            pthread_mutex_unlock(&send_lock);
        }      
    }while(1);

    // Never reach here
    // To close descriptor gracefully, Remove watch like below
    inotify_rm_watch( fd, wd );

    // Close fd for watch
    close( fd );

    printf("[pssaps] Stop watching\n");

    return 0;
}


// Print buffer as hexidemal
void pssaps_print_hex(const unsigned char * buf, int size)
{
   int i=0;

    for(i=0; i < size ; i++)
    {
        printf("%02x",buf[i]);
    }
    printf("\n");
}


// Get MD5
int pssaps_get_md5(const char *file_path, unsigned char *md5)
{
    int file_desc;
    char* file_buff;
    struct stat stat_buff;

    // Open file to calculate MD5
    file_desc = open(file_path, O_RDONLY);
    if(file_desc < 0)
    {
        printf("file:%s\n", file_path);
        perror("[pssaps] Failed to open()");
        return -1;
    }

    // Get file size
    if(fstat(file_desc, &stat_buff)<0)
    {
        perror("[pssaps] Failed to fstat()");
        return -1;
    }

    // Reset MD5
    memset(md5, 0, MD5_DIGEST_LENGTH);

    // Get MD5 from mmap
    file_buff = mmap(0, stat_buff.st_size, PROT_READ, MAP_SHARED, file_desc, 0);
    MD5((unsigned char*) file_buff, stat_buff.st_size, md5);
    munmap(file_buff, stat_buff.st_size); 

    md5[MD5_DIGEST_LENGTH] = 0;

    // Print MD5
    printf("[pssaps] file:%s md5:", file_path);
    pssaps_print_hex((const unsigned char *)md5, MD5_DIGEST_LENGTH);

    return 0;
}

///////////////////////////////////////////////////////////////////////////////