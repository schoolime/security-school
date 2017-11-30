/**
 * @file 	pssap_lkm.c
 * @author 	SecuritySchool	
 * @brief	Airplane Prototype - LKM
 */
 
#include <linux/init.h>             // For init, exit macro
#include <linux/module.h>           // Core header for LKM
#include <linux/kernel.h>           // Core header for kernel
#include <linux/binfmts.h>			// For bprm

///////////////////////////////////////////////////////////////////////////////

MODULE_LICENSE("GPL");
MODULE_AUTHOR("SecuritySchool");
MODULE_DESCRIPTION("Airplane Prototype - LKM");
MODULE_VERSION("0.2");	// 0.2: add blocking pssap_eicar by name 

///////////////////////////////////////////////////////////////////////////////

// Module Init function	
static int __init pssapl_init(void);
// Module Exit function
static void __exit pssapl_exit(void); 

// Set init function to kernel
module_init(pssapl_init);
// Set exit function to kernel
module_exit(pssapl_exit);

// symbols from kernel
typedef int (*security_bprm_check_func)(struct linux_binprm *bprm);
extern void security_bprm_check_set_pss_filter( security_bprm_check_func pfunc);
extern void security_bprm_check_unset_pss_filter(void);
// filter function 
static int pssapl_filter_func(struct linux_binprm *bprm);

///////////////////////////////////////////////////////////////////////////////

// Module Init function	
static int __init pssapl_init(void){
	printk(KERN_INFO "[pssapl] Hello LKM!\n" );
	// set filter 	
	security_bprm_check_set_pss_filter(pssapl_filter_func);

	return 0;
}

 
// Module Exit function
static void __exit pssapl_exit(void){
	// unset filter before exit
	security_bprm_check_unset_pss_filter();
	printk(KERN_INFO "[pssapl] Goodbye LKM!\n" );
}


// filter function 
static int pssapl_filter_func(struct linux_binprm *bprm)
{
	printk(KERN_INFO "[pssapl] New process (file:%s)\n", bprm->filename);

	if( NULL != bprm->filename && NULL != strstr(bprm->filename, "pssap_eicar") )
	{
		printk(KERN_INFO "[pssapl] blocked\n");
		return -EACCES;
	}
	return 0;
} 

///////////////////////////////////////////////////////////////////////////////