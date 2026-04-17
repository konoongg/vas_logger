#ifndef VG_HOOKS_H
#define VG_HOOKS_H

#include <linux/kprobes.h>

typedef struct vg_hooks_s vg_hooks_t;

struct vg_hooks_s {
    struct kprobe kp_exec;
    struct kretprobe kretp_exec;
};

extern bool vg_register_hooks(void);
extern void vg_unregister_hooks(void);

#endif