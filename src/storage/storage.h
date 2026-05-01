#ifndef VG_STORAGE_H
#define VG_STORAGE_H

#include <linux/list.h>
#include <linux/types.h>
#include <linux/hashtable.h>

#define VG_ST_HASH_BITS 10

typedef enum vg_mm_type_event_e vg_mm_type_event;
typedef struct vg_mm_event_s vg_mm_event;
typedef struct vg_mm_exec_s vg_mm_exec;
typedef struct vg_mm_dump_s vg_mm_dump;
typedef struct vg_st_storage_s vg_st_storage;

enum vg_mm_type_event_e {
    VG_MM_EXEC,
};

struct vg_mm_exec_s {
    vg_mm_type_event type;
    struct list_head node;

    unsigned long start_code;
    unsigned long end_code;
    unsigned long start_data;
    unsigned long end_data;
    unsigned long start_brk;
    unsigned long brk;
    unsigned long start_stack;

    unsigned long arg_start;
    unsigned long arg_end;
    unsigned long env_start;
    unsigned long env_end;

    unsigned long task_size;
    unsigned long mmap_base;

    unsigned long total_vm;
    unsigned long data_vm;
    unsigned long exec_vm;
    unsigned long stack_vm;
    unsigned long locked_vm;

    int map_count;
};

struct vg_mm_event_s {
    vg_mm_type_event type;
    struct list_head node;
};

struct vg_mm_dump_s {
    pid_t pid;
    struct list_head events;
    unsigned int nr_events;
    struct hlist_node hnode;
};


struct vg_st_storage_s {
    DECLARE_HASHTABLE(htable, VG_ST_HASH_BITS);
};

extern bool vg_st_create_storage(void);
extern void vg_st_destroy_storage(void);
extern bool vg_st_add_event(pid_t pid, struct vg_mm_event_s *event);
extern void vg_st_delete_dump(pid_t pid);
extern struct vg_mm_dump_s *vg_st_get_dump_copy(pid_t pid);
extern void vg_mm_dump_destroy(struct vg_mm_dump_s *dump);

#endif