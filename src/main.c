#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

#include "hooks/hooks.h"

static int __init vas_logger_init(void) {

    if (!vg_register_hooks()) {
        pr_err("vas_logger: can't register hooks \n");
        return 1;
    }


    pr_info("vas_logger: Initializing module \n");
    return 0;
}

static void __exit vas_logger_exit(void) {
    vg_register_hooks();

    pr_info("vas_logger: deInitializing module \n");
}

module_init(vas_logger_init);
module_exit(vas_logger_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Vladislav Akhmedov");
MODULE_DESCRIPTION("Kernel module, that creates /proc/<pid>/vas_logger and writes VAS creation information to this file");
MODULE_VERSION("1.0");
