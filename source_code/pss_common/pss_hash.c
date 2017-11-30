#ifdef __KERNEL__
////////////////////////////////////////////////////////////
#include <linux/fs.h>
#include <linux/crypto.h>
#include <crypto/md5.h>
#include <linux/scatterlist.h>
#include "pss_hash.h"

////////////////////////////////////////////////////////////
int pss_hash_get(struct file* file, unsigned int left_size, unsigned char *md5)
{
    struct hash_desc hashCtx;
    const unsigned int buff_max = 16 * 1024;
    unsigned char *buff = NULL;
    unsigned int buff_size = 0;
    unsigned int file_pos = 0;
    unsigned int file_max = 0;
    unsigned int read_size = 0;
    struct scatterlist sg;
    int ret = -1;
    struct kstat stat;

    /* Comment #1 */
    ret = vfs_getattr(&(file->f_path), &stat);
    if (ret) {
        pr_info("[%s] Failed to vfs_getattr\n", __FUNCTION__);
        return ret;
    }
    file_max = stat.size - left_size;

    /* Comment #2 */
    hashCtx.flags = 0;
    hashCtx.tfm   = crypto_alloc_hash("md5", 0, CRYPTO_ALG_ASYNC);
    if (!IS_ERR(hashCtx.tfm)) {
        crypto_hash_init(&hashCtx);
        buff = (unsigned char*)kmalloc(buff_max, GFP_KERNEL);
        if (NULL != buff) {
            while (1) {
                buff_size = buff_max < file_max - file_pos ? buff_max : file_max - file_pos;
                read_size = kernel_read(file, file_pos, buff, buff_size);
                if (0 < read_size) {
                    file_pos+=read_size;
                    sg_init_one(&sg, buff, read_size);
                    /* Comment #3 */
                    crypto_hash_update(&hashCtx, &sg, read_size);
                } else if (0 > read_size) {
                    pr_info("[%s] Failed to kernel_read\n", __FUNCTION__);
                    break;
                }

                if (file_pos >= file_max) {
                    /* Comment #4 */
                    crypto_hash_final(&hashCtx, (u8 *)md5);
                    ret = 0;
                    break;
                }
            }
            kfree(buff);
            buff = NULL;
        } else {
            pr_info("[%s] Failed to kmalloc\n", __FUNCTION__);     
        }
    } else {
        pr_info("[%s] Failed to crypto_alloc_hash\n", __FUNCTION__);
    }

    return ret;
}

#else // __KERNEL__

////////////////////////////////////////////////////////////
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <openssl/md5.h>

////////////////////////////////////////////////////////////
int pss_hash_get(const char *file_path, unsigned int left_size, unsigned char *md5)
{
    int file_desc;
    char* file_buff;
    struct stat stat_buff;
    int i = 0;

    file_desc = open(file_path, O_RDONLY);
    if (file_desc < 0) {
        printf("file:%s\n", file_path);
        perror("[pssa] Failed to open()");
        return -1;
    }
    if (fstat(file_desc, &stat_buff)<0) {
        perror("[pssa] Failed to fstat()");
        return -1;
    }

    /* Comment #5 */
    file_buff = mmap(0, stat_buff.st_size, PROT_READ, MAP_SHARED, file_desc, 0);
    MD5((unsigned char*) file_buff, stat_buff.st_size-left_size, md5);
    munmap(file_buff, stat_buff.st_size); 
    close(file_desc);

    return 0;
}

#endif // __KERNEL__