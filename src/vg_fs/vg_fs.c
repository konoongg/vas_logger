#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/seq_file.h>
#include <linux/fs.h>
#include <linux/fs_context.h>
#include <linux/mm.h>
#include <linux/init.h>

#include "vg_fs/vg_fs.h"

#define VAS_LOG_FS_NAME "vas_log"
#define VAS_LOG_FS_VERSION "1.0"

#define MAX_LOG_SIZE 10 * 1024
#define MAX_FILES_COUNT 1024

typedef struct vas_log_entry vas_log_entry;

/* ENTRY -------------------------------------------------------------------------*/
struct vas_log_entry {
    char *buffer;
    size_t size;
    struct mutex lock;
    struct pid *pid;
    struct dentry *dentry;
    int active;
};

static int
vg_file_show(struct seq_file *m, void *v)
{
    vas_log_entry *entry = m->private;

    if (!entry || !entry->active) {
        seq_puts(m, "Error: Log entry not available\n");
        return 0;
    }

    mutex_lock(&entry->lock);
    if (entry->buffer && entry->size > 0) {
        seq_write(m, entry->buffer, entry->size);
    } else {
        seq_puts(m, "No log entries\n");
    }
    mutex_unlock(&entry->lock);

    return 0;
}

static int
vg_file_open(struct inode *inode, struct file *file) {
    vas_log_entry *entry = inode->i_private;

    if (!entry || !entry->active)
        return -ENOENT;

    if (file->f_mode & FMODE_WRITE) {
        return -EPERM;
    }

    return single_open(file, vg_file_show, entry);
}

static ssize_t
vg_file_write(struct file *file, const char __user *buf,
                              size_t len, loff_t *off)
{
    return -EPERM;
}

static const struct file_operations vas_log_fops = {
    .owner   = THIS_MODULE,
    .open    = vg_file_open,
    .read    = seq_read,
    .write   = vg_file_write,
    .llseek  = seq_lseek,
    .release = single_release,
};

/*------------------------------------------------------------------------------------------------------*/


static struct inode *
vg_fs_super_alloc_inode(struct super_block *sb) {
    return new_inode(sb);
}

static void
vg_fs_super_free_inode(struct inode *inode) {
    free_inode_nonrcu(inode);
}

static void
vg_fs_super_destroy_inode(struct inode *inode) {
    return;
}

static void
vg_fs_super_evict_inode(struct inode *inode) {
    truncate_inode_pages_final(&inode->i_data);
    clear_inode(inode);
}


static const struct super_operations vas_log_super_ops = {
    .alloc_inode    = vg_fs_super_alloc_inode,
    .free_inode     = vg_fs_super_free_inode,
    .destroy_inode  = vg_fs_super_destroy_inode,
    .evict_inode    = vg_fs_super_evict_inode,
    .statfs         = simple_statfs,
};

static int
vg_fs_fill_super(struct super_block *sb, struct fs_context *fc) {
    struct inode *inode;
    struct dentry *root;
    struct timespec64 now;

    sb->s_magic = 0x5641534c; // "VASL"
    sb->s_maxbytes = MAX_LOG_SIZE * MAX_FILES_COUNT;
    sb->s_op = &vas_log_super_ops;

    inode = new_inode(sb);
    if (!inode)
        return -ENOMEM;

    inode->i_ino = 1;
    inode->i_mode = S_IFDIR | 0755;
    inode->i_op = &simple_dir_inode_operations;
    inode->i_fop = &simple_dir_operations;

    now = current_time(inode);
    inode_set_atime(inode, now.tv_sec, now.tv_nsec);
    inode_set_mtime(inode, now.tv_sec, now.tv_nsec);
    inode_set_ctime(inode, now.tv_sec, now.tv_nsec);

    root = d_make_root(inode);
    if (!root) {
        iput(inode);
        return -ENOMEM;
    }
    sb->s_root = root;

    return 0;
}

static int
vg_fc_get_tree(struct fs_context *fc) {
    return get_tree_nodev(fc, vg_fs_fill_super);
}

static void
vg_fc_free(struct fs_context *fc) {
    return;
}

static const struct fs_context_operations vg_context_ops = {
    .get_tree   = vg_fc_get_tree,
    .free       = vg_fc_free,
};

static int
vg_fs_init_fs_context(struct fs_context *fc)
{
    fc->fs_private = NULL;
    fc->ops = &vg_context_ops;
    return 0;
}

static struct file_system_type vas_fs_type = {
    .owner             = THIS_MODULE,
    .name              = VAS_LOG_FS_NAME,
    .init_fs_context   = vg_fs_init_fs_context,
    .kill_sb           = kill_litter_super,
    .fs_flags          = 0,
};

static struct vfsmount *mount_point = NULL;
static const char *MOUNT_PATH = "/vas_log";

extern bool
vg_fs_register(void) {
    struct fs_context *fc;
    struct vfsmount *mnt;
    int ret;

    ret = register_filesystem(&vas_fs_type);
    if (ret != 0) {
        printk(KERN_ERR "can't register fs /vas_log)\n");
        return false;
    }

    fc = fs_context_for_mount(&vas_fs_type, 0);
    if (IS_ERR(fc)) {
        printk(KERN_ERR "vas_log: can't create fs context\n");
        goto fail1;
    }

    ret = vfs_get_tree(fc);
    if (ret) {
        printk(KERN_ERR "vas_log: can't get tree\n");
        goto fail2;
    }

    mnt = vfs_create_mount(fc);

    if (IS_ERR(mnt)) {
        printk(KERN_ERR "vas_log: can't create mount\n");
        goto fail2;
    }

    put_fs_context(fc);

    mount_point = mnt;
    printk(KERN_INFO "vas_log: filesystem mounted at %s\n", MOUNT_PATH);
    return true;


fail2:
    put_fs_context(fc);
fail1:
    unregister_filesystem(&vas_fs_type);
    return false;
}

extern void
vg_fs_unregister(void) {
    if (mount_point && !IS_ERR(mount_point)) {
        kern_unmount(mount_point);
        printk(KERN_INFO "vas_log: filesystem unmounted\n");
    }

    unregister_filesystem(&vas_fs_type);

    printk(KERN_INFO "vas_log: filesystem unregistered\n");
}
