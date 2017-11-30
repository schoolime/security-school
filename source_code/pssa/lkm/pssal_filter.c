#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/binfmts.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/spinlock_types.h>
#include <linux/rwsem.h>
#include <crypto/md5.h>
#include <linux/list.h>
#include "pss_hash.h"
#include "pss_ksock.h"
#include "pssa_conf.h"

////////////////////////////////////////////////////////////
MODULE_LICENSE("GPL");
MODULE_AUTHOR("SecuritySchool");
MODULE_DESCRIPTION("AirPlane Filter");
MODULE_VERSION("0.1");

////////////////////////////////////////////////////////////
/* Comment #6 */
typedef struct
{
    struct list_head node;
    u8 md5[MD5_DIGEST_SIZE];
} pssal_hash_item;

////////////////////////////////////////////////////////////
static struct task_struct * cli_thread = 0;
struct socket * cli_sock = NULL;
static DEFINE_RWLOCK(sig_lock);
static LIST_HEAD(sig_list);

////////////////////////////////////////////////////////////
static int __init pssal_filter_init(void);
static void __exit pssal_filter_exit(void);
typedef int (*security_bprm_check_func)(struct linux_binprm *bprm);
extern void security_bprm_check_set_pss_filter( security_bprm_check_func pfunc);
extern void security_bprm_check_unset_pss_filter(void);
static int pssal_filter_callback(struct linux_binprm *bprm);
static int cli_thread_func(void *arg);
int pssal_filter_hash_add(const char *buff, int buff_len);
int pssal_filter_hash_match(struct file* file);
void pssal_filter_hash_remove(void);

////////////////////////////////////////////////////////////
static int __init pssal_filter_init(void)
{
	pr_info("[%s] Initialized\n", __FUNCTION__);
	/* Comment #1 */
	security_bprm_check_set_pss_filter(pssal_filter_callback);
	cli_thread = kthread_run(cli_thread_func,NULL,"pss_filter_thread");

	return 0;
}

module_init(pssal_filter_init);

////////////////////////////////////////////////////////////
static void __exit pssal_filter_exit(void)
{
	/* Comment #9 */
	if (NULL != cli_sock) {
		pss_ksock_shutdown(cli_sock, SHUT_RDWR);
	}
	if(cli_thread) {
		send_sig(SIGTERM, cli_thread, 1);
		kthread_stop(cli_thread);
		cli_thread = 0;
	}
	
	if(NULL != cli_sock) {
		pss_ksock_close(cli_sock);
		cli_sock = NULL;
	}

	security_bprm_check_unset_pss_filter();
	pssal_filter_hash_remove();

	pr_info("[%s] Finished\n", __FUNCTION__);
}

module_exit(pssal_filter_exit);

////////////////////////////////////////////////////////////
static int pssal_filter_callback(struct linux_binprm *bprm)
{
	int i = 0;
	int ret = 0;
#define PSSA_PATH_MIN	512
	char path_buffer[PSSA_PATH_MIN] = {0,};
	char *path_abs = NULL;
	struct path *path = NULL;

	if (bprm && bprm->filename) {
		/* Comment #5 */
		path = &(bprm->file->f_path);
		path_get(path);
		path_abs = d_path(path, path_buffer, PSSA_PATH_MIN);
		if (!IS_ERR(path_abs) ) {
			for ( ;0 != path_abs[i] && 0 != pssa_conf_filter_prefix[i] && 
				path_abs[i] == pssa_conf_filter_prefix[i] ; i++ );
			if ( 0 == pssa_conf_filter_prefix[i] )
			{
				if (0 == pssal_filter_hash_match(bprm->file)) {
					pr_info("[%s] Blocked (file:%s)\n", __FUNCTION__, path_abs);
					ret = -EACCES;
				} else {
					pr_info("[%s] Passed (file:%s)\n", __FUNCTION__, path_abs);
				}
			}
		}
		path_put(path);
	}
	
	return ret;
}

////////////////////////////////////////////////////////////
static int cli_thread_func(void *arg)
{
	struct sockaddr_in serv_addr;
	static const char * cli_msg = "Hello Server?";
	char *buff = NULL;
	int ret = 0;

	buff = kmalloc(PSSA_CONF_SOCKBUFF_MAX, GFP_KERNEL);
	if (NULL == buff) {
		pr_info("[%s] Not enough memory\n", __FUNCTION__);
		return -1;
	}

	/* Comment #2 */
	allow_signal(SIGTERM);
	while(1) {
		/* Comment #3 */
		cli_sock = pss_ksock_create(AF_INET , SOCK_STREAM , 0);
		if (NULL != cli_sock) {
			serv_addr.sin_addr.s_addr = in_aton(PSSA_CONF_SERVER_IP);
			serv_addr.sin_family = AF_INET;
			serv_addr.sin_port = htons(PSSA_CONF_SERVER_PORT);
			pr_debug("[%s] Trying to connect..\n", __FUNCTION__); 
			ret = pss_ksock_connect(cli_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
			if(0 <= ret) {
				ret = pss_ksock_send(cli_sock , cli_msg , strlen(cli_msg) , 0);
				if(0 >= ret) {
					pr_info("[%s] Send failed\n", __FUNCTION__);
					break;
				}
				pr_debug("[%s] Sent : %s\n", __FUNCTION__, cli_msg); 
				while(1) {
					/* Comment #4 */
					ret = pss_ksock_recv(cli_sock , buff , PSSA_CONF_SOCKBUFF_MAX , 0);
					if(0 >= ret) {
						pr_info("[%s] recv failed\n", __FUNCTION__);
						break;
					}
					pr_info("[%s] %d bytes received from Update Server\n", __FUNCTION__, ret);
					pssal_filter_hash_remove();
					pssal_filter_hash_add(buff, ret);
				}
			} else {
				pr_info("[%s] connect failed. retry it after 30 sec\n", __FUNCTION__);
				/* Comment #3 */
				msleep_interruptible(15*1000);
			}
			if(NULL != cli_sock) {
				pss_ksock_close(cli_sock);
				cli_sock = NULL;
			}
			/* Comment #2 */
			if (signal_pending(current)) {
				pr_debug("[%s] SIGTERM is received\n", __FUNCTION__);
				break;
			}
			if (kthread_should_stop()) {
				break;
			}
		} else {
			pr_info("[%s] Failed to create socket", __FUNCTION__);
			ret = -1;
			break;
		}
	} 

	if (NULL != buff){
		kfree(buff);
		buff = NULL;
	}
	cli_thread = NULL;

	return ret;
}

////////////////////////////////////////////////////////////
int pssal_filter_hash_add(const char *buff, int buff_len)
{
	pssal_hash_item *item = NULL;
	char * md5 = NULL;
	int len = 0;
	int ret = 0;

	if (NULL == buff || buff_len == 0 || buff_len%MD5_DIGEST_SIZE != 0) {
		pr_info("[%s] Invalid buffer size:%d\n", __FUNCTION__, buff_len);
		return -1;
	}
	/* Comment #7 */
  write_lock( &sig_lock );
  pr_debug("[%s] -- Start of MD5 list -- \n", __FUNCTION__);
  for (md5 = (char *)buff, len=0; len<buff_len; len+=MD5_DIGEST_SIZE) {
  	item = (pssal_hash_item *)kmalloc(sizeof(pssal_hash_item), GFP_KERNEL);
  	if (item) {
  		memcpy(item->md5, md5, MD5_DIGEST_SIZE);
  		list_add_tail( &item->node, &sig_list);
      print_hex_dump(KERN_DEBUG,"[pssal_filter_hash_add] MD5:", 0, 16, 0,
                      item->md5, MD5_DIGEST_SIZE, false);
  	}
  	else {
  		pr_info("[%s] Not enough mem\n", __FUNCTION__);
  		ret = -1;
  		break;
  	}
  	md5 += MD5_DIGEST_SIZE;
  }
  pr_debug("[%s] -- End of MD5 list -- \n", __FUNCTION__);
  write_unlock( &sig_lock );

  return ret;
}

////////////////////////////////////////////////////////////
int pssal_filter_hash_match(struct file* file)
{
  u8 md5[MD5_DIGEST_SIZE];
  pssal_hash_item *item = NULL;
  int ret = -1;

  ret = pss_hash_get(file, 0, md5);
  if (!ret) {
    ret = 1;
    /* Comment #8 */
    read_lock( &sig_lock );
    list_for_each_entry( item, &sig_list, node ) {
      if (0 == strncmp(item->md5, md5, MD5_DIGEST_SIZE)) {
        pr_debug( "[%s]: signature is machted, ", __FUNCTION__);
        ret =0;
        break;
      }
    }
    read_unlock( &sig_lock );
  } else {
      pr_info("[%s] Failed to pss_hash_get\n", __FUNCTION__);
  }

  return ret;
}

////////////////////////////////////////////////////////////
void pssal_filter_hash_remove(void)
{
	pssal_hash_item *item = NULL;
  pssal_hash_item *next = NULL;

  /* Comment #10 */
  write_lock( &sig_lock );
  list_for_each_entry_safe( item, next, &sig_list, node )
  {
    list_del( &item->node );
    kfree( item );
  }
  write_unlock( &sig_lock );

  pr_debug("[%s] signatures are removed\n", __FUNCTION__);
}

