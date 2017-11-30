#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <sys/inotify.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <openssl/md5.h>
#include "pssa_conf.h"
#include "pss_hash.h"

////////////////////////////////////////////////////////////
#define PSSA_WATCHFILE_MAX  (PSSA_CONF_SOCKBUFF_MAX/MD5_DIGEST_LENGTH)
#define PSSA_INOTIFY_SIZE  (sizeof(struct inotify_event))
#define PSSA_INOTIFY_BUFFLEN    (PSSA_WATCHFILE_MAX*(PSSA_INOTIFY_SIZE+16))

////////////////////////////////////////////////////////////
static char *send_buff = NULL;
static int send_buff_len = 0;
/* Comment #5 */
static pthread_mutex_t send_lock   = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  send_event  = PTHREAD_COND_INITIALIZER;
static const char path_to_watch[] = PSSA_CONF_SERVER_SAMPLEDIR; 
static int update_loop(void);
static void *client_handler(void *sock_desc);
static int make_sendbuff(const char * watch_path);
static void *sample_monitor(void *arg);

////////////////////////////////////////////////////////////
int main(int argc, char *argv[])
{
    pthread_t handler_thread;
    int ret = -1;

    send_buff = malloc(PSSA_CONF_SOCKBUFF_MAX);
    if (NULL != send_buff) {
        /* Comment #1 */
        pthread_mutex_lock(&send_lock);
        if (0 <= make_sendbuff(path_to_watch)) {
            pthread_mutex_unlock(&send_lock);
            /* Comment #2 */
            if (0 >= pthread_create(&handler_thread, NULL,  sample_monitor, 0)) {
                ret = 0;
                /* Comment #3 */
                update_loop();    
            } else {
                perror("[pssa] Failed to pthread_create() to monitor");
            }
        } else {
            pthread_mutex_unlock(&send_lock);    
            perror("[pssa] Failed to make buffer to send");
        }
    } else {
        perror("Failed to malloc");
    }

    return ret;
}

////////////////////////////////////////////////////////////
int update_loop(void)
{
    int sock_desc       = 0;
    int cli_sock        = 0;
    int sock_opt        = 1;
    int sock_len        = 0;
    struct sockaddr_in serv_addr;
    struct sockaddr_in cli_addr;
    pthread_t handler_thread;
    int ret = -1;
   
    sock_desc = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_desc == -1) {
        perror("[pssa] Failed to create socket!");
        goto main_out;
    }

    sock_opt = 1;
    setsockopt( sock_desc, SOL_SOCKET, SO_REUSEADDR, &sock_opt, sizeof(sock_opt) );
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        perror("[pssa] Failed to signal()");
        goto main_out;
    }
     
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(PSSA_CONF_SERVER_PORT);
    sock_len = sizeof(struct sockaddr_in);
    if (bind(sock_desc,(struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("[pssa] Failed to bind socket!");
        goto main_out;
    }
     
    listen(sock_desc, PSSA_CONF_CLIENT_MAX);
    printf("[pssa] Waiting for client ...\n");
    ret = 0;
    while ((cli_sock = accept(sock_desc, (struct sockaddr *)&cli_addr, (socklen_t*)&sock_len))) {
        printf("[pssa][%d client] Accepted\n", cli_sock);
        /* Comment #4 */        
        if (pthread_create( &handler_thread, NULL,  client_handler, (void*)(intptr_t)cli_sock) < 0) {
            perror("[pssa] Failed to create thread!");
            continue;
        }
    }

main_out:
    if (-1 != sock_desc) {
        close(sock_desc);
        sock_desc = -1;
    }

    if (NULL != send_buff) {
        free(send_buff);
        send_buff = NULL;
    }
     
    printf("[pssa] exit\n");  
    return ret;
}

////////////////////////////////////////////////////////////
void * client_handler(void *sock_desc)
{
    int sock        = (int)(intptr_t)sock_desc;
    int read_size   = 0;
    char cli_msg[PSSA_CONF_SOCKBUFF_MAX]   = {0x00, };
 
    read_size = recv(sock, cli_msg, PSSA_CONF_SOCKBUFF_MAX-1, 0);
    if (0 < read_size) {
        cli_msg[PSSA_CONF_SOCKBUFF_MAX-1] = 0;
        printf("[pssa][%d client] Received : \"%s\"\n", sock, cli_msg);
        do {
            pthread_mutex_lock(&send_lock);
            if(0 < send(sock, send_buff, send_buff_len, 0)) {
                printf("[pssa][%d client] Sent signatures\n", sock);
            } else {
                pthread_mutex_unlock(&send_lock);
                perror("[pssa] Failed to send!");
                break;
            }
            pthread_cond_wait(&send_event, &send_lock);
            pthread_mutex_unlock(&send_lock);

        } while(1);
    } else if (0 == read_size) {
        printf("[pssa][%d client] Disconnected\n", sock);
    } else {
        perror("[pssa] Failed to recevie");
    }        

    close(sock);
    printf("[pssa][%d client] Exit\n", sock);    
    return 0;
}

////////////////////////////////////////////////////////////
int make_sendbuff(const char * watch_path)
{
    DIR *dir;
    struct dirent entry;
    struct dirent *result = NULL;
    int ret = 0;
    char file_path[PATH_MAX] = {0,};
    int file_start = strlen(watch_path);
    unsigned char md5[MD5_DIGEST_LENGTH+1];
    int i = 0;

    strcpy(file_path, watch_path);
    file_path[file_start++] = '/';
    file_path[file_start] = 0;
    md5[MD5_DIGEST_LENGTH] = 0;

    dir = opendir (watch_path);
    if (dir != NULL) {
        send_buff_len = 0;
        send_buff[0] = 0;

        while (!readdir_r( dir, &entry, &result ) && ( result != NULL )) {
            if (DT_REG == entry.d_type) {
                ++ret;
                if (send_buff_len + MD5_DIGEST_LENGTH + 2 < PSSA_CONF_SOCKBUFF_MAX) {
                    file_path[file_start] = 0;
                    strncat(file_path+file_start, entry.d_name, strlen(entry.d_name));
                    if (0 == pss_hash_get(file_path, 0, md5)) {
                        strncat(send_buff+send_buff_len, (const char*)md5, MD5_DIGEST_LENGTH);
                        send_buff_len += MD5_DIGEST_LENGTH;

                        printf("[%s] (%d) file:%s, md5:", __FUNCTION__, ret, entry.d_name);
                        for (i=0; i<MD5_DIGEST_LENGTH; i++) {
                            printf("%02x ", md5[i]);
                        }
                        printf("\n");
                    }
                } else {
                    printf("[%s] The count of sample files exceed PSSA_WATCHFILE_MAX(%d)\n",
                            __FUNCTION__, PSSA_WATCHFILE_MAX);
                    break;
                }
            }
        }
        send_buff[send_buff_len] = 0;
        closedir (dir);
    } else {
        perror ("[pssa] Failed to opendir()");
        ret = -1;
    }

    return ret;
}

////////////////////////////////////////////////////////////
void *sample_monitor(void * arg)
{    
    int read_len    = 0;
    int event_index = 0;
    int dir_changed = 0;
    int fd, wd = 0;
    char buffer[PSSA_INOTIFY_BUFFLEN];
    struct inotify_event *event = NULL;
    
    printf("[pssa] monitor thread start\n");

    fd = inotify_init();
    if (fd < 0) {
        perror("[pssa] Failed to inotify_init()");
    }
    wd = inotify_add_watch(fd, path_to_watch, 
            IN_CLOSE_WRITE|IN_DELETE|IN_MODIFY|IN_MOVED_FROM|IN_MOVED_TO);

    do
    {
        event_index = 0;
        dir_changed = 0;

        read_len = read(fd, buffer, PSSA_INOTIFY_BUFFLEN); 
        if (read_len < 0) {
            perror( "[pssa] Failed to read()" );
        }

        while (event_index < read_len) {
            event = (struct inotify_event *) &buffer[event_index];

            /* Hint */
            if (event->len) {
                if (event->mask & IN_CLOSE_WRITE ||
                    event->mask & IN_DELETE ||
                    event->mask & IN_MODIFY ||
                    event->mask & IN_MOVED_FROM ||
                    event->mask & IN_MOVED_TO ) {
                    printf("[pssa] watchor detected some changes.\n");
                    dir_changed = 1;
                }
                event_index += PSSA_INOTIFY_SIZE + event->len;
            } else {
                event_index += PSSA_INOTIFY_SIZE;                
            }
        }

        if( 1 == dir_changed )
        {
            /* Comment #6 */
            pthread_mutex_lock(&send_lock);
            make_sendbuff(path_to_watch);
            pthread_cond_broadcast(&send_event);
            pthread_mutex_unlock(&send_lock);
        }      
    }while(1);

    inotify_rm_watch( fd, wd );
    close( fd );

    printf("[pssa] Stop watching\n");
    return 0;
}