#include <linux/debugfs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <asm/pgtable.h>
#include <linux/pgtable.h>

MODULE_LICENSE("GPL");

static struct dentry *dir, *output;
static struct task_struct *task;

// struct packet : to store data from the argument 'user_buffer' in the read_output function
struct packet {
        pid_t pid;
        unsigned long vaddr;
        unsigned long paddr;
};

// read_output : to read virtual address from user_buffer and convert it to physical address doing page walk
static ssize_t read_output(struct file *fp,
                        char __user *user_buffer,
                        size_t length,
                        loff_t *position)
{
        struct packet pack;
        unsigned long ppn, ppo;
        
        pgd_t *pgd;
        p4d_t *p4d;
        pud_t *pud;
        pmd_t *pmd;
        pte_t *pte;

        // to copy the data from user_buffer to 'pack'
        copy_from_user(&pack, user_buffer, length);

        // to find the task_struct corresponding to the given pid
        task = pid_task(find_vpid(pack.pid), PIDTYPE_PID);
        if (!task){
                printk("Process with the typed pid does not exist\n");
                return -1;
        }

        // to get pgd from mm_struct and the virtual address - page walk step 1
        pgd = pgd_offset(task->mm, pack.vaddr);
        if (pgd_none(*pgd) || pgd_bad(*pgd)){
                printk("Error related to pgd\n");
                return -1;
        }

        // to get p4d from pgd and the virtual address - page walk step 2
        p4d = p4d_offset(pgd, pack.vaddr);
        if (p4d_none(*p4d) || p4d_bad(*p4d)){
                printk("Error related to p4d\n");
                return -1;
        }

        // to get pud from p4d and the virtual address - page walk step 3
        pud = pud_offset(p4d, pack.vaddr);
        if (pud_none(*pud) || pud_bad(*pud)){
                printk("Error related to pud\n");
                return -1;
        }

        // to get pmd from pud and the virtual address - page walk step 4
        pmd = pmd_offset(pud, pack.vaddr);
        if (pmd_none(*pmd) || pmd_bad(*pmd)){
                printk("Error related to pmd\n");
                return -1;
        }
        
        // to get pte from pmd and the virtual address - page walk step 5
        pte = pte_offset_map(pmd, pack.vaddr);
        if (pte_none(*pte)){
                printk("Error related to pte\n");
                return -1;
        }

        // to calculate ppn and ppo and concatenate them to get the physical address
        ppn = pte_pfn(*pte) << PAGE_SHIFT;
        ppo = pack.vaddr & (~PAGE_MASK);
        pack.paddr = ppn | ppo;

        // to copy the data from 'pack to user_buffer
        copy_to_user(user_buffer, &pack, length);
        
        return length;
}

// stuct file_operations : to map file operations with my function
static const struct file_operations dbfs_fops = {
        .read = read_output
};

// dbfs_module_init : It is running when the module is loaded to the kernel
static int __init dbfs_module_init(void)
{
        // to create the directory 'paddr' in the debugfs system
        dir = debugfs_create_dir("paddr", NULL);

        if (!dir) {
                printk("Cannot create paddr dir\n");
                return -1;
        }

        // to create the file 'output' in the 'paddr' directory
        output = debugfs_create_file("output", 0777, dir, NULL, &dbfs_fops);

        // to print the log that it is initialized successfully
	printk("dbfs_paddr module initialize done\n");

        return 0;
}

// dbfs_module_exit: It is running when the module is unloaded from the kernel
static void __exit dbfs_module_exit(void)
{
        // recursively removes a 'dir' directory
        debugfs_remove_recursive(dir);
        dir = NULL;

        // to print the log when the module is unloaded succesfully
	printk("dbfs_paddr module exit\n");
}

module_init(dbfs_module_init);
module_exit(dbfs_module_exit);
