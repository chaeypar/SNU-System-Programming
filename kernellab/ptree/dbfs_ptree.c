#include <linux/debugfs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/uaccess.h>

MODULE_LICENSE("GPL");

static struct dentry *dir, *inputdir, *ptreedir;
static struct task_struct *curr;
static struct debugfs_blob_wrapper *blob;

static ssize_t write_pid_to_input(struct file *fp, 
                                const char __user *user_buffer, 
                                size_t length, 
                                loff_t *position)
{
        pid_t input_pid;
        char tp[5000];

        sscanf(user_buffer, "%u", &input_pid);

        // Find task_struct using input_pid. Hint: pid_task
        curr = pid_task(find_vpid(input_pid), PIDTYPE_PID);
        if (!curr){
                printk("Process with the typed pid does not exist.");
                return -1;
        }
        // Tracing process tree from input_pid to init(1) process
        // Make Output Format string: process_command (process_id)
        ((char *)(blob->data))[0] = NULL;

        while (curr){
                sprintf(tp, "%s (%d)\n%s", curr->comm, curr->pid, (char *)(blob->data));
                sprintf(blob->data, "%s", tp);
                if (curr->pid == 1)
                        break;
                curr = curr -> parent;
        }
        return length;
}

static const struct file_operations dbfs_fops = {
        .write = write_pid_to_input,
};

static int __init dbfs_module_init(void)
{
        // Implement init module code
        dir = debugfs_create_dir("ptree", NULL);
        
        if (!dir) {
                printk("Cannot create ptree dir\n");
                return -1;
        }

        blob = (struct debugfs_blob_wrapper *)kmalloc(sizeof(struct debugfs_blob_wrapper), GFP_KERNEL);
        blob -> data = (void *)kmalloc(5000, GFP_KERNEL);
        blob -> size = 5000;
        tot_len = 0;
 
        inputdir = debugfs_create_file("input", 0777, dir, NULL, &dbfs_fops);
        ptreedir = debugfs_create_blob("ptree", 0777, dir, blob); // Find suitable debugfs API	

	printk("dbfs_ptree module initialize done\n");
        return 0;
}

static void __exit dbfs_module_exit(void)
{
        // Implement exit module code
        debugfs_remove_recursive(dir);
        
        if (blob && blob->data){
                kfree(blob->data);
                blob->data = NULL;
        }

        if (blob){
                kfree(blob);
                blob = NULL;
        }
	
        printk("dbfs_ptree module exit\n");
}

module_init(dbfs_module_init);
module_exit(dbfs_module_exit);
