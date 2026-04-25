#include <linux/module.h>
#include <linux/kprobes.h>
#include <linux/sched.h>
#include <linux/mm_types.h>

#include "hooks/hooks.h"
#include "vg_fs/vg_fs.h"

static int __init kprobe_exec_init(void)
{

    if (!vg_fs_register()) {
        printk(KERN_WARNING "VAS_LOGGER: can't create VAS_FS \n");
        return -1;
    }

    if (!vg_register_hooks()) {

        printk(KERN_WARNING "VAS_LOGGER: can't start \n");
        vg_fs_unregister();
        return -1;
    }

    printk(KERN_INFO "VAS_LOGGER: start \n");
    return 0;
}

static void __exit kprobe_exec_exit(void)
{
    vg_unregister_hooks();
    vg_fs_unregister();
    printk(KERN_INFO "VAS_LOGGER: finish \n");
}

module_init(kprobe_exec_init);
module_exit(kprobe_exec_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Monitor process address space using kprobe on do_execve");