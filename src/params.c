#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/moduleparam.h>
#include <linux/stat.h>
#include <linux/slab.h>

#include "storage/storage.h"

static pid_t target_pid;

static int
param_set_pid(const char *val, const struct kernel_param *kp) {
    long pid_val;
    if (kstrtol(val, 10, &pid_val) != 0)
        return -EINVAL;
    if (pid_val <= 0)
        return -EINVAL;

    target_pid = (pid_t)pid_val;
    pr_info("vg_storage: target PID set to %d\n", target_pid);
    return 0;
}

static int param_get_dump_stat(char *buf, const struct kernel_param *kp) {
    struct vg_mm_dump_s *dump;
    struct vg_mm_event_s *event;
    int len = 0;

    if (target_pid == 0)
        return scnprintf(buf, PAGE_SIZE, "No PID set (use 'pid' parameter)\n");

    dump = vg_st_get_dump_copy(target_pid);
    if (!dump)
        return scnprintf(buf, PAGE_SIZE, "Dump for PID %d not found\n", target_pid);

    len += scnprintf(buf + len, PAGE_SIZE - len,
                     "=== Memory dump for PID %d ===\n", target_pid);
    len += scnprintf(buf + len, PAGE_SIZE - len,
                     "Number of events: %u\n\n", dump->nr_events);

    list_for_each_entry(event, &dump->events, node) {
        if (event->type == VG_MM_EXEC) {
            struct vg_mm_exec_s *exec = (struct vg_mm_exec_s *)event;
            len += scnprintf(buf + len, PAGE_SIZE - len,
                "[exec] code: 0x%lx-0x%lx, data: 0x%lx-0x%lx, "
                "brk: 0x%lx-0x%lx, stack: 0x%lx, mmap_base: 0x%lx\n",
                exec->start_code, exec->end_code,
                exec->start_data, exec->end_data,
                exec->start_brk, exec->brk,
                exec->start_stack, exec->mmap_base);
            len += scnprintf(buf + len, PAGE_SIZE - len,
                "[exec] vm: total=%lu, data=%lu, exec=%lu, stack=%lu, locked=%lu, map_count=%d\n",
                exec->total_vm, exec->data_vm, exec->exec_vm,
                exec->stack_vm, exec->locked_vm, exec->map_count);
        }
    }

    vg_mm_dump_destroy(dump);
    return len;
}

/* ---------- Регистрация параметров ---------- */
static const struct kernel_param_ops pid_param_ops = {
    .set = param_set_pid,
};

static const struct kernel_param_ops dump_stat_param_ops = {
    .get = param_get_dump_stat,
};

module_param_cb(pid, &pid_param_ops, NULL, S_IWUSR);
module_param_cb(dump_stat, &dump_stat_param_ops, NULL, S_IRUSR);