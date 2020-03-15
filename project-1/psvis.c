#include <linux/list.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/pid.h>
#include <linux/moduleparam.h>
#include <linux/stat.h>
#include <linux/time.h>

static int pid = -1;

module_param(pid, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP );


int pid_init(void){

    if(pid == -1){
        printk(KERN_ALERT "No input entered!\n");
	    return 0;
    }

    struct pid *pid_struct = find_get_pid(pid);
    struct task_struct *parent = pid_task(pid_struct, PIDTYPE_PID);


    if(parent == NULL){
        printk(KERN_ALERT "No process found!\n");
	    return 0;
    }

    struct task_struct *task;
    struct list_head *list;

    printk(KERN_INFO "%d", parent->pid);

    list_for_each(list, &parent->children) {
        task = list_entry(list, struct task_struct, sibling);
        printk(KERN_INFO "%d", task->pid);
    }
   
   return 0;
}


void pid_exit(void){
}


module_init(pid_init);
module_exit(pid_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Harun and Firat");
MODULE_DESCRIPTION("PID MODULE");
