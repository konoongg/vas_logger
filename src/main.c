#include <linux/module.h>
#include <linux/kprobes.h>
#include <linux/sched.h>
#include <linux/mm_types.h>

#include "hooks/hooks.h"
#include "storage/storage.h"

static int __init
kprobe_exec_init(void)
{
    if (!vg_st_create_storage()) {
        printk(KERN_WARNING "VAS_LOGGER: can't init storage\n");
        return -1;
    }

    if (!vg_register_hooks()) {
        printk(KERN_WARNING "VAS_LOGGER: can't init hooks\n");
        goto fail;
    }

    printk(KERN_INFO "VAS_LOGGER: start \n");
    return 0;

fail:
    vg_st_destroy_storage();
    return -1;
}

static void __exit
kprobe_exec_exit(void)
{
    vg_unregister_hooks();
    vg_st_destroy_storage();
    printk(KERN_INFO "VAS_LOGGER: finish \n");
}

module_init(kprobe_exec_init);
module_exit(kprobe_exec_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Monitor process address space using kprobe on do_execve");