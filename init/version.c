/*
 *  linux/init/version.c
 */

#include <generated/compile.h>
#include <linux/module.h>
#include <linux/uts.h>
#include <linux/utsname.h>
#include <generated/utsrelease.h>
#include <linux/version.h>
#include <linux/proc_ns.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/printk.h>

#ifndef CONFIG_KALLSYMS
#define version(a) Version_ ## a
#define version_string(a) version(a)

extern int version_string(LINUX_VERSION_CODE);
int version_string(LINUX_VERSION_CODE);
#endif

struct uts_namespace init_uts_ns = {
	.kref = KREF_INIT(2),
	.name = {
		.sysname	= UTS_SYSNAME,
		.nodename	= UTS_NODENAME,
		.release	= UTS_RELEASE,
		.version	= UTS_VERSION,
		.machine	= UTS_MACHINE,
		.domainname	= UTS_DOMAINNAME,
	},
	.user_ns = &init_user_ns,
	.ns.inum = PROC_UTS_INIT_INO,
#ifdef CONFIG_UTS_NS
	.ns.ops = &utsns_operations,
#endif
};
EXPORT_SYMBOL_GPL(init_uts_ns);

/* 
 * 关键修改点：
 * 这里不再使用 const，且必须明确指定大小 [256] 
 * 这样 sizeof(linux_banner) 在本文件中才有效
 */
char linux_banner[512] =
	"Linux version " UTS_RELEASE " (" LINUX_COMPILE_BY "@"
	LINUX_COMPILE_HOST ") (" LINUX_COMPILER ") " UTS_VERSION "\n";

const char linux_proc_banner[] =
	"%s version %s"
	" (" LINUX_COMPILE_BY "@" LINUX_COMPILE_HOST ")"
	" (" LINUX_COMPILER ") %s\n";

/* 动态控制逻辑 */
static ssize_t fake_version_write(struct file *file, const char __user *buffer,
                                size_t count, loff_t *ppos)
{
    char kbuf[128];
    size_t len = min(count, (size_t)sizeof(kbuf) - 1);
    struct new_utsname *uts;

    if (copy_from_user(kbuf, buffer, len))
        return -EFAULT;

    kbuf[len] = '\0';
    strim(kbuf); 

    // 修改 /proc/version 数据源
    // 这里明确使用 256，避免编译器因为之前的 extern 声明感到困惑
    memset(linux_banner, 0, 256);
    snprintf(linux_banner, 255, 
             "Linux version %s (" LINUX_COMPILE_BY "@" LINUX_COMPILE_HOST ") (" LINUX_COMPILER ") " UTS_VERSION "\n", 
             kbuf);

    // 修改 uname -r 数据源
    uts = utsname();
    if (uts) {
        strncpy(uts->release, kbuf, sizeof(uts->release) - 1);
        uts->release[sizeof(uts->release) - 1] = '\0';
    }

    return count;
}

static const struct file_operations fake_version_fops = {
    .write = fake_version_write,
};

static int __init init_fake_version_interface(void)
{
    proc_create("set_version", 0200, NULL, &fake_version_fops);
    return 0;
}

late_initcall(init_fake_version_interface);
