#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/mm_types.h>
#include <linux/kprobes.h>

#include "hooks/hooks.h"


vg_hooks_t *vg_hooks = NULL;

/* EXEC -----------------------------------------------------------------------*/

static const char EXEC_SYM[] = "__x64_sys_execve";

static int
vg_exec_pre_handler(struct kprobe *p, struct pt_regs *regs) {
    struct task_struct *task = current;
    struct mm_struct *mm = task->mm;

    if (mm == NULL || mm->pgd == NULL) {
        printk(KERN_WARNING "mm_stuct is NULL)\n");
        return 0;
    }

    printk(KERN_INFO "vg_exec_pre_handler hooks");

    return 0;
}

static int
vg_exec_ret_handler(struct kretprobe_instance *, struct pt_regs *) {
    struct task_struct *task = current;
    struct mm_struct *mm = task->mm;

    if (mm == NULL || mm->pgd == NULL) {
        printk(KERN_WARNING "mm_stuct is NULL)\n");
        return 0;
    }

    printk(KERN_INFO "vg_exec_ret_handler hooks");

    return 0;
}

static bool
vg_register_exec_hooks(void) {
    int ret;

    vg_hooks->kp_exec.symbol_name = EXEC_SYM;
    vg_hooks->kp_exec.pre_handler = vg_exec_pre_handler;

    ret = register_kprobe(&vg_hooks->kp_exec);
    if (ret != 0) {
        printk(KERN_WARNING "kprobe can't registered on \n");
        return false;
    }

    vg_hooks->kretp_exec.kp.symbol_name = EXEC_SYM;
    vg_hooks->kretp_exec.handler = vg_exec_ret_handler;

    ret = register_kretprobe(&vg_hooks->kretp_exec);
    if (ret != 0) {
        unregister_kprobe(&vg_hooks->kp_exec);
        printk(KERN_WARNING "kretprobe can't registered on \n");
        return false;
    }

    return true;
}

static void
vg_unregister_exec_hooks(void) {
    unregister_kprobe(&vg_hooks->kp_exec);
    unregister_kretprobe(&vg_hooks->kretp_exec);
}

/*-----------------------------------------------------------------------------*/

extern bool
vg_register_hooks(void) {
    vg_hooks =  kmalloc(sizeof(vg_hooks_t), GFP_KERNEL);
    if (vg_hooks == NULL)
        return false;

    if (!vg_register_exec_hooks())
        goto fail;

    return true;

fail:
    kfree(vg_hooks);
    return false;
}

extern void
vg_unregister_hooks(void) {
    vg_unregister_exec_hooks();
    kfree(vg_hooks);
}