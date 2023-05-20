#include <linux/debugfs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <asm/pgtable.h>

MODULE_LICENSE("GPL");

static struct dentry *dir, *output;
static struct task_struct *task;

struct packet {
        pid_t pid;
        unsigned long vaddr;
        unsigned long paddr;
};


static ssize_t read_output(struct file *fp,
                        char __user *user_buffer,
                        size_t length,
                        loff_t *position)
{
        // Implement read file operation
        struct packet pack;
        unsigned long ppn, ppo;

        copy_from_user((void *)&pack, user_buffer, length);
        task = pid_task(find_vpid(pid), PIDTYPE_PID);
        if (!task){
                printk("Process with the typed pid does not exist.\n");
                return 0;
        }

        pgd_t *pgd = pgd_offset(task->mm, pack.vaddr);
        p4d_t *p4d = p4d_offset(pgd, pack.vaddr); 
        pud_t *pud = pud_offset(p4d, pack.vaddr);
  =     pmd_t *pmd = pmd_offset(pud, pack.vaddr);
        pte_t *pte = pte_offset_kernel(pmd, pack.vaddr);
        ppn = pte_pfn(*pte) << PAGE_SHIFT;
        ppo = pack.vaddr & (~PAGE_MASK);
        pack.paddr = ppn|ppo;
        return 0;
}

static const struct file_operations dbfs_fops = {
        // Mapping file operations with your functions
        .read = read_output
};

static int __init dbfs_module_init(void)
{
        // Implement init module
        dir = debugfs_create_dir("paddr", NULL);

        if (!dir) {
                printk("Cannot create paddr dir\n");
                return -1;
        }

        // Fill in the arguments below
        output = debugfs_create_file("output", 0777, dir, NULL, &dbfs_fops);

	printk("dbfs_paddr module initialize done\n");

        return 0;
}

static void __exit dbfs_module_exit(void)
{
        // recursively removes a 'dir' directory
        debugfs_remove_recursive(dir);
        dir = NULL;

	printk("dbfs_paddr module exit\n");
}

module_init(dbfs_module_init);
module_exit(dbfs_module_exit);
