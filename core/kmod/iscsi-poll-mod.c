#include <linux/module.h>
#include <linux/kernel.h>

int init_module(void)
{
    printk(KERN_INFO "iscsi-poll initialized.");
    return 0;
}

void exit_module(void)
{
    printk(KERN_INFO "iscsi-poll exiting.");
}

