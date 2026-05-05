#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/mm_types.h>
#include <linux/kprobes.h>
#include <linux/workqueue.h>

#include "hooks/hooks.h"
#include "storage/storage.h"

vg_hooks_t *vg_hooks = NULL;

static atomic_t vg_hooks_cancelled = ATOMIC_INIT(0);
static atomic_t vg_hooks_active    = ATOMIC_INIT(0);
static DECLARE_COMPLETION(vg_hooks_all_done);

typedef struct exec_work_s exec_work;
typedef struct exit_work_s exit_work;
struct exec_work_s {
    struct work_struct work;
    pid_t tgid;
    vg_mm_exec *mm_exec;
};

struct exit_work_s {
    struct work_struct work;
    pid_t tgid;
};

static void
vg_active_done(void) {
    if (atomic_dec_and_test(&vg_hooks_active))
        if (atomic_read(&vg_hooks_cancelled))
            complete(&vg_hooks_all_done);
}

static bool
vg_schedule_work(struct work_struct *work) {

    if (atomic_read(&vg_hooks_cancelled))
        return false;

    atomic_inc(&vg_hooks_active);

    if (atomic_read(&vg_hooks_cancelled)) {
        vg_active_done();
        return false;
    }

    schedule_work(work);
    return true;
}

/* EXEC -----------------------------------------------------------------------*/

static const char EXEC_SYM[] = "__x64_sys_execve";

static void
vg_exec_work_func(struct work_struct *work)
{
    exec_work *ew = container_of(work, exec_work, work);

    vg_st_add_event(ew->tgid, (vg_mm_event *)ew->mm_exec);
    vg_active_done();
    kfree(ew);
}

static int
vg_exec_ret_handler(struct kretprobe_instance *, struct pt_regs *) {
    struct task_struct *task = current;
    struct mm_struct *mm = task->mm;
    exec_work *ew;

    if (mm == NULL) {
        printk(KERN_WARNING "mm_stuct is NULL\n");
        return 0;
    }

    ew = kmalloc(sizeof(*ew), GFP_ATOMIC);
    if (!ew) {
        printk(KERN_WARNING "exec_work: allocation failed\n");
        return 0;
    }

    ew->mm_exec = kmalloc(sizeof(vg_mm_exec), GFP_ATOMIC);
    if (!ew->mm_exec) {
        printk(KERN_WARNING "exec_work: allocation failed\n");
        kfree(ew);
        return 0;
    }

    vg_mm_exec *mm_exec = ew->mm_exec;

    mm_exec->type = VG_MM_EXEC;

    mm_exec->start_code = mm->start_code;
    mm_exec->end_code = mm->end_code;
    mm_exec->start_data = mm->start_data;
    mm_exec->end_data = mm->end_data;
    mm_exec->start_brk = mm->start_brk;
    mm_exec->brk = mm->brk;
    mm_exec->start_stack = mm->start_stack;

    mm_exec->arg_start = mm->arg_start;
    mm_exec->arg_end = mm->arg_end;
    mm_exec->env_start = mm->env_start;
    mm_exec->env_end = mm->env_end;

    mm_exec->task_size = mm->task_size;
    mm_exec->mmap_base = mm->mmap_base;

    mm_exec->total_vm = mm->total_vm;
    mm_exec->data_vm = mm->data_vm;
    mm_exec->exec_vm = mm->exec_vm;
    mm_exec->stack_vm = mm->stack_vm;
    mm_exec->locked_vm = mm->locked_vm;
    mm_exec->map_count = mm->map_count;

    ew->tgid = task->tgid;
    INIT_WORK(&ew->work, vg_exec_work_func);

    if (!vg_schedule_work(&ew->work))
        kfree(ew);

    return 0;
}

static bool
vg_register_exec_hooks(void) {
    int ret;

    vg_hooks->kretp_exec.kp.symbol_name = EXEC_SYM;
    vg_hooks->kretp_exec.handler = vg_exec_ret_handler;

    ret = register_kretprobe(&vg_hooks->kretp_exec);
    if (ret != 0) {
        printk(KERN_WARNING "kretprobe can't registered on \n");
        return false;
    }

    return true;
}

static void
vg_unregister_exec_hooks(void) {
    unregister_kretprobe(&vg_hooks->kretp_exec);
}

/* EXEC END -----------------------------------------------------------------------*/

/* EXIT -----------------------------------------------------------------------------*/
static const char EXIT_SYM[] = "do_exit";

static void
vg_exit_work_func(struct work_struct *work)
{
    exit_work *ew = container_of(work, exit_work, work);

    vg_st_delete_dump(ew->tgid);
    vg_active_done();
    kfree(ew);
}

static int
vg_exit_handler(struct kprobe *kp, struct pt_regs *regs) {
    struct task_struct *task = current;
    exit_work *ew;

    ew = kmalloc(sizeof(*ew), GFP_ATOMIC);
    if (!ew) {
        printk(KERN_WARNING "exit_work: allocation failed\n");
        return 0;
    }

    ew->tgid = task->tgid;
    INIT_WORK(&ew->work, vg_exit_work_func);

    if (!vg_schedule_work(&ew->work))
        kfree(ew);

    return 0;
}

/* EXIT END-----------------------------------------------------------------------------*/

static bool
vg_register_exit_hooks(void) {
    int ret;

    vg_hooks->kp_exit.symbol_name = EXIT_SYM;
    vg_hooks->kp_exit.pre_handler = vg_exit_handler;

    ret = register_kprobe(&vg_hooks->kp_exit);
    if (ret != 0) {
        printk(KERN_WARNING "kprobe can't register on %s\n", EXIT_SYM);
        return false;
    }
    return true;
}

static void
vg_unregister_exit_hooks(void) {
    unregister_kprobe(&vg_hooks->kp_exit);
}

extern bool
vg_register_hooks(void) {
    vg_hooks = kmalloc(sizeof(*vg_hooks), GFP_KERNEL);
    if (vg_hooks == NULL)
        return false;

    if (!vg_register_exec_hooks())
        goto fail;

    if (!vg_register_exit_hooks())
        goto fail_exit;

    return true;

fail_exit:
    vg_unregister_exec_hooks();
fail:
    kfree(vg_hooks);
    return false;
}

extern void
vg_unregister_hooks(void) {
    if (!vg_hooks)
        return;

    atomic_set(&vg_hooks_cancelled, 1);

    if (atomic_read(&vg_hooks_active) == 0)
        complete(&vg_hooks_all_done);

    wait_for_completion(&vg_hooks_all_done);

    vg_unregister_exit_hooks();
    vg_unregister_exec_hooks();

    kfree(vg_hooks);
    vg_hooks = NULL;
}
