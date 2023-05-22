#include <linux/debugfs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/slab.h>

MODULE_LICENSE("GPL");

static struct dentry *dir, *inputdir, *ptreedir;
static struct task_struct *curr;
static struct debugfs_blob_wrapper *blob;

// write_pid_to_input : trace process from the process corresponding to the given pid to the init process
static ssize_t write_pid_to_input(struct file *fp, 
                                const char __user *user_buffer, 
                                size_t length, 
                                loff_t *position)
{
        pid_t input_pid;
        char tp[5000];

        // to get the pid 'input_pid' from the input
        sscanf(user_buffer, "%u", &input_pid);

        // to find task_struct 'curr' using input_pid
        curr = pid_task(find_vpid(input_pid), PIDTYPE_PID);
        if (!curr){
                printk("Process with the typed pid does not exist.");
                return -1;
        }

        // to empty the blob->data before tracing the process tree
        ((char *)(blob->data))[0] = NULL;

        // Tracing process tree from input_pid to init(1) process and store "process_command (pid)" in blob->data
        while (curr){
                sprintf(tp, "%s (%d)\n%s", curr->comm, curr->pid, (char *)(blob->data));
                sprintf(blob->data, "%s", tp);
                if (curr->pid == 1)
                        break;
                curr = curr -> parent;
        }
        return length;
}

// stuct file_operations : to map file operations with my function
static const struct file_operations dbfs_fops = {
        .write = write_pid_to_input,
};

// dbfs_module_init : It is running when the module is loaded to the kernel
static int __init dbfs_module_init(void)
{
        // to create the directory 'ptree' in the debug file system
        dir = debugfs_create_dir("ptree", NULL);
        if (!dir) {
                printk("Cannot create ptree dir\n");
                return -1;
        }

        // to allocate and initialize 'struct debugfs_blob_wrapper *'
        blob = (struct debugfs_blob_wrapper *)kmalloc(sizeof(struct debugfs_blob_wrapper), GFP_KERNEL);
        blob -> data = (void *)kmalloc(5000, GFP_KERNEL);
        blob -> size = 5000;
 
        // create files in the debug file system
        inputdir = debugfs_create_file("input", 0777, dir, NULL, &dbfs_fops);
        ptreedir = debugfs_create_blob("ptree", 0777, dir, blob);

        //to print the log message when the module is loaded successfully
	printk("dbfs_ptree module initialize done\n");
        return 0;
}

// dbfs_module_exit: It is running when the module is unloaded from the kernel
static void __exit dbfs_module_exit(void)
{
        // recursively removes a 'dir' directory
        debugfs_remove_recursive(dir);
        
        // to deallocate the 'blob -> data'
        if (blob && blob->data){
                kfree(blob->data);
                blob->data = NULL;
        }

        // to deallocate the 'blob'
        if (blob){
                kfree(blob);
                blob = NULL;
        }
	
        // to print the log when the module is unloaded succesfully
        printk("dbfs_ptree module exit\n");
}

module_init(dbfs_module_init);
module_exit(dbfs_module_exit);
