#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/init.h>

#define VAS_LOG_FS_NAME "vas_log"
#define VAS_LOG_FS_VERSION "1.0"

#define MAX_LOG_SIZE 10 * 1024
#define MAX_FILES_COUNT 1000

typedef struct vas_log_entry vas_log_entry;

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
vas_log_write(struct file *file, const char __user *buf,
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

extern bool
vg_fs_register(void) {
    int ret = register_filesystem(&vas_fs_type)
    if (ret != 0) {
        printk(KERN_ERR "can't register fs /vas_log)\n");
        return false;
    }
    return true;
}

extern void
vg_fs_unregister(void) {
    unregister_filesystem(&vas_fs_type);
}