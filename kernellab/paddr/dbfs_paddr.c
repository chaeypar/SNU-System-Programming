#include <linux/debugfs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <asm/pgtable.h>
#include <linux/pgtable.h>

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

        copy_from_user(&pack, user_buffer, length);
        task = pid_task(find_vpid(pack.pid), PIDTYPE_PID);
        if (!task){
                printk("Process with the typed pid does not exist\n");
                return -1;
        }

        pgd_t *pgd = pgd_offset(task->mm, pack.vaddr);
        if (pgd_none(*pgd) || pgd_bad(*pgd)){
                printk("Error related to pgd\n");
                return -1;
        }

        p4d_t *p4d = p4d_offset(pgd, pack.vaddr);
        if (p4d_none(*p4d) || p4d_bad(*p4d)){
                printk("Error related to p4d\n");
                return -1;
        }

        pud_t *pud = pud_offset(p4d, pack.vaddr);
        if (pud_none(*pud) || pud_bad(*pud)){
                printk("Error related to pud\n");
                return -1;
        }

        pmd_t *pmd = pmd_offset(pud, pack.vaddr);
        if (pmd_none(*pmd) || pmd_bad(*pmd)){
                printk("Error related to pmd\n");
                return -1;
        }
        
        pte_t *pte = pte_offset_map(pmd, pack.vaddr);
        if (pte_none(*pte)){
                printk("Error related to pte\n");
                return -1;
        }
        
        ppn = pte_pfn(*pte) << PAGE_SHIFT;
        ppo = pack.vaddr & (~PAGE_MASK);
        pack.paddr = ppn | ppo;

        copy_to_user(user_buffer, &pack, length);
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
