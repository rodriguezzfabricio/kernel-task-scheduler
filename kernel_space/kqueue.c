#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/module.h>
#include <linux/syscalls.h>
#include <linux/slab.h>       // For kmalloc/kfree
#include <linux/delay.h>      // For msleep
#include <linux/uaccess.h>    // For copy_from_user (if extending to pass resource arrays)
#include "kqueue.h"           // Our header with syscall prototypes

// Define maximum number of resources a task may require.
#define MAX_TASK_RESOURCES 3

// Global task queues
static LIST_HEAD(high_queue);
static LIST_HEAD(medium_queue);
static LIST_HEAD(low_queue);
static LIST_HEAD(waiting_queue);
static LIST_HEAD(completed_queue);   // Holds tasks that have finished execution
static DEFINE_MUTEX(scheduler_lock);   // Global lock for scheduler queues

// Resource structure and definitions
struct my_resource {
    int count;           // Number of available instances
    struct mutex lock;   // Protect count
};

// Use an enum for resource types (for clarity)
enum resource_type {
    RESOURCE_A = 0,
    RESOURCE_B,
    RESOURCE_C,
    NUM_RESOURCES
};

static struct my_resource resources[NUM_RESOURCES];

// This function initializes our resource pool.
static int __init scheduler_init(void)
{
    // Resource A: For example, 2 instances available
    resources[RESOURCE_A].count = 2;
    mutex_init(&resources[RESOURCE_A].lock);

    // Resource B: For example, 3 instances available
    resources[RESOURCE_B].count = 3;
    mutex_init(&resources[RESOURCE_B].lock);

    // Resource C: For example, 1 instance available
    resources[RESOURCE_C].count = 1;
    mutex_init(&resources[RESOURCE_C].lock);

    return 0;
}
module_init(scheduler_init);

// Task structure
struct my_task {
    int m_task_id;
    int m_duration;
    int m_priority;
    int required_resources[MAX_TASK_RESOURCES];  // Array of resource IDs needed
    int resource_count; // How many resources are required
    int resources_acquired; // 0 = not acquired, 1 = acquired
    struct list_head list; // Links to next/prev task in a queue
};

/* Helper Functions for Resources */
// Attempts to acquire a resource; returns 0 on success, -EBUSY if unavailable.
static int getResource(int r_id)
{
    int ret = 0;
    mutex_lock(&resources[r_id].lock);
    if (resources[r_id].count > 0) {
        resources[r_id].count--;
    } else {
        ret = -EBUSY;
    }
    mutex_unlock(&resources[r_id].lock);
    return ret;
}

// Releases a resource by incrementing its count.
static void releaseResource(int r_id)
{
    mutex_lock(&resources[r_id].lock);
    resources[r_id].count++;
    mutex_unlock(&resources[r_id].lock);
}

/* Forward Declarations */
// Searches all queues (including completed) for a task with the given task_id.
static struct my_task *find_task_in_queue(int task_id)
{
    struct my_task *temp;
    list_for_each_entry(temp, &high_queue, list) {
        if (temp->m_task_id == task_id)
            return temp;
    }
    list_for_each_entry(temp, &medium_queue, list) {
        if (temp->m_task_id == task_id)
            return temp;
    }
    list_for_each_entry(temp, &low_queue, list) {
        if (temp->m_task_id == task_id)
            return temp;
    }
    list_for_each_entry(temp, &waiting_queue, list) {
        if (temp->m_task_id == task_id)
            return temp;
    }
    list_for_each_entry(temp, &completed_queue, list) {
        if (temp->m_task_id == task_id)
            return temp;
    }
    return NULL;
}

// Searches only the completed_queue for a task.
static struct my_task *find_completed_task(int task_id)
{
    struct my_task *temp;
    list_for_each_entry(temp, &completed_queue, list) {
        if (temp->m_task_id == task_id)
            return temp;
    }
    return NULL;
}

/* System Call: sys_add_task */
// Populates the required_resources[] based on the task priority.
// High priority (0) requires RESOURCE_A and RESOURCE_B;
// Medium (1) requires RESOURCE_B;
// Low (2) requires RESOURCE_C.
SYSCALL_DEFINE3(add_task, int, task_id, int, duration, int, priority)
{
    struct my_task *taskAtHand;
    taskAtHand = kmalloc(sizeof(struct my_task), GFP_KERNEL);
    if (!taskAtHand)
        return -ENOMEM;
    
    INIT_LIST_HEAD(&taskAtHand->list);
    taskAtHand->m_task_id = task_id;
    taskAtHand->m_duration = duration;
    taskAtHand->m_priority = priority;
    taskAtHand->resources_acquired = 0;  // Initially, resources are not acquired.
    
    // Populate required_resources and resource_count based on priority.
    switch (priority) {
        case 0: // High priority: require RESOURCE_A and RESOURCE_B.
            taskAtHand->required_resources[0] = RESOURCE_A;
            taskAtHand->required_resources[1] = RESOURCE_B;
            taskAtHand->resource_count = 2;
            break;
        case 1: // Medium priority: require RESOURCE_B.
            taskAtHand->required_resources[0] = RESOURCE_B;
            taskAtHand->resource_count = 1;
            break;
        case 2: // Low priority: require RESOURCE_C.
            taskAtHand->required_resources[0] = RESOURCE_C;
            taskAtHand->resource_count = 1;
            break;
        default:
            kfree(taskAtHand);
            return -EINVAL;
    }
    
    mutex_lock(&scheduler_lock);
    if (priority == 0)
        list_add_tail(&taskAtHand->list, &high_queue);
    else if (priority == 1)
        list_add_tail(&taskAtHand->list, &medium_queue);
    else if (priority == 2)
        list_add_tail(&taskAtHand->list, &low_queue);
    else {
        mutex_unlock(&scheduler_lock);
        kfree(taskAtHand);
        return -EINVAL;
    }
    mutex_unlock(&scheduler_lock);
    
    printk(KERN_INFO "sys_add_task: Adding task id=%d, duration=%d, priority=%d\n",
           task_id, duration, priority);
    return 0;
}

/* System Call: sys_acquire_resources */
// Attempts to acquire all required resources for a task.
// If successful, marks the task as having acquired its resources.
SYSCALL_DEFINE1(acquire_resources, int, task_id)
{
    struct my_task *tempTask;
    int ret, counter = 0, i;
    printk(KERN_INFO "sys_acquire_resources: Attempting to acquire resources for task %d\n", task_id);
    mutex_lock(&scheduler_lock);
    tempTask = find_task_in_queue(task_id);
    if (!tempTask) {
        mutex_unlock(&scheduler_lock);
        printk(KERN_INFO "sys_acquire_resources: Task %d not found.\n", task_id);
        return -ENOENT;
    }
    // If already acquired, do nothing.
    if (tempTask->resources_acquired) {
        mutex_unlock(&scheduler_lock);
        printk(KERN_INFO "sys_acquire_resources: Resources already acquired for task %d\n", task_id);
        return 0;
    }
    for (i = 0; i < tempTask->resource_count; i++) {
        int r_id = tempTask->required_resources[i];
        ret = getResource(r_id);
        if (ret < 0) {
            int j;
            for (j = 0; j < counter; j++)
                releaseResource(tempTask->required_resources[j]);
            mutex_unlock(&scheduler_lock);
            printk(KERN_INFO "sys_acquire_resources: Could not acquire resource %d for task %d\n", r_id, task_id);
            return -EBUSY;
        }
        counter++;
    }
    tempTask->resources_acquired = 1;
    mutex_unlock(&scheduler_lock);
    printk(KERN_INFO "sys_acquire_resources: Successfully acquired resources for task %d\n", task_id);
    return 0;
}

/* System Call: sys_schedule_tasks */
// Processes tasks in waiting, high, medium, and low queues.
// If a task has already acquired its resources (resources_acquired==1),
// then simply simulate execution (msleep) and move it to completed_queue.
// Otherwise, attempt to acquire the needed resources.
SYSCALL_DEFINE0(schedule_tasks)
{
    struct my_task *temp, *tmp;
    int i, ret, counter;
    
    printk(KERN_INFO "sys_schedule_tasks: Scheduling tasks\n");
    mutex_lock(&scheduler_lock);
    
    /* Process waiting_queue */
    list_for_each_entry_safe(temp, tmp, &waiting_queue, list) {
        if (temp->resources_acquired) {
            list_del(&temp->list);
            mutex_unlock(&scheduler_lock); // Unlock during msleep
            msleep(temp->m_duration * 1000);
            mutex_lock(&scheduler_lock);
            list_add_tail(&temp->list, &completed_queue);
            printk(KERN_INFO "Task %d executed (waiting_queue, pre-acquired) and moved to completed_queue.\n", temp->m_task_id);
        } else {
            bool checker = true;
            counter = 0;
            for (i = 0; i < temp->resource_count; i++) {
                int r_id = temp->required_resources[i];
                ret = getResource(r_id);
                if (ret < 0) {
                    checker = false;
                    break;
                }
                counter++;
            }
            if (!checker) {
                for (i = 0; i < counter; i++)
                    releaseResource(temp->required_resources[i]);
                /* Leave task in waiting_queue */
            } else {
                list_del(&temp->list);
                mutex_unlock(&scheduler_lock); // Unlock during msleep
                msleep(temp->m_duration * 1000);
                mutex_lock(&scheduler_lock);
                list_add_tail(&temp->list, &completed_queue);
                printk(KERN_INFO "Task %d executed (waiting_queue) and moved to completed_queue.\n", temp->m_task_id);
            }
        }
    }
    
    /* Process high_queue */
    list_for_each_entry_safe(temp, tmp, &high_queue, list) {
        if (temp->resources_acquired) {
            list_del(&temp->list);
            mutex_unlock(&scheduler_lock);
            msleep(temp->m_duration * 1000);
            mutex_lock(&scheduler_lock);
            list_add_tail(&temp->list, &completed_queue);
            printk(KERN_INFO "Task %d executed (high_queue, pre-acquired) and moved to completed_queue.\n", temp->m_task_id);
        } else {
            bool can_run = true;
            counter = 0;
            for (i = 0; i < temp->resource_count; i++) {
                int r_id = temp->required_resources[i];
                ret = getResource(r_id);
                if (ret < 0) {
                    can_run = false;
                    break;
                }
                counter++;
            }
            if (!can_run) {
                for (i = 0; i < counter; i++)
                    releaseResource(temp->required_resources[i]);
                list_move_tail(&temp->list, &waiting_queue);
                printk(KERN_INFO "Task %d moved from high_queue to waiting_queue.\n", temp->m_task_id);
            } else {
                list_del(&temp->list);
                mutex_unlock(&scheduler_lock);
                msleep(temp->m_duration * 1000);
                mutex_lock(&scheduler_lock);
                list_add_tail(&temp->list, &completed_queue);
                printk(KERN_INFO "Task %d executed (high_queue) and moved to completed_queue.\n", temp->m_task_id);
            }
        }
    }
    
    /* Process medium_queue */
    list_for_each_entry_safe(temp, tmp, &medium_queue, list) {
        if (temp->resources_acquired) {
            list_del(&temp->list);
            mutex_unlock(&scheduler_lock);
            msleep(temp->m_duration * 1000);
            mutex_lock(&scheduler_lock);
            list_add_tail(&temp->list, &completed_queue);
            printk(KERN_INFO "Task %d executed (medium_queue, pre-acquired) and moved to completed_queue.\n", temp->m_task_id);
        } else {
            bool can_run = true;
            counter = 0;
            for (i = 0; i < temp->resource_count; i++) {
                int r_id = temp->required_resources[i];
                ret = getResource(r_id);
                if (ret < 0) {
                    can_run = false;
                    break;
                }
                counter++;
            }
            if (!can_run) {
                for (i = 0; i < counter; i++)
                    releaseResource(temp->required_resources[i]);
                list_move_tail(&temp->list, &waiting_queue);
                printk(KERN_INFO "Task %d moved from medium_queue to waiting_queue.\n", temp->m_task_id);
            } else {
                list_del(&temp->list);
                mutex_unlock(&scheduler_lock);
                msleep(temp->m_duration * 1000);
                mutex_lock(&scheduler_lock);
                list_add_tail(&temp->list, &completed_queue);
                printk(KERN_INFO "Task %d executed (medium_queue) and moved to completed_queue.\n", temp->m_task_id);
            }
        }
    }
    
    /* Process low_queue */
    list_for_each_entry_safe(temp, tmp, &low_queue, list) {
        if (temp->resources_acquired) {
            list_del(&temp->list);
            mutex_unlock(&scheduler_lock);
            msleep(temp->m_duration * 1000);
            mutex_lock(&scheduler_lock);
            list_add_tail(&temp->list, &completed_queue);
            printk(KERN_INFO "Task %d executed (low_queue, pre-acquired) and moved to completed_queue.\n", temp->m_task_id);
        } else {
            bool can_run = true;
            counter = 0;
            for (i = 0; i < temp->resource_count; i++) {
                int r_id = temp->required_resources[i];
                ret = getResource(r_id);
                if (ret < 0) {
                    can_run = false;
                    break;
                }
                counter++;
            }
            if (!can_run) {
                for (i = 0; i < counter; i++)
                    releaseResource(temp->required_resources[i]);
                list_move_tail(&temp->list, &waiting_queue);
                printk(KERN_INFO "Task %d moved from low_queue to waiting_queue.\n", temp->m_task_id);
            } else {
                list_del(&temp->list);
                mutex_unlock(&scheduler_lock);
                msleep(temp->m_duration * 1000);
                mutex_lock(&scheduler_lock);
                list_add_tail(&temp->list, &completed_queue);
                printk(KERN_INFO "Task %d executed (low_queue) and moved to completed_queue.\n", temp->m_task_id);
            }
        }
    }
    
    mutex_unlock(&scheduler_lock);
    return 0;
}

/* System Call: sys_release_resources */
// Releases resources for a task that has already finished (i.e. is in the completed queue)
// and frees the task structure.
SYSCALL_DEFINE1(release_resources, int, task_id)
{
    struct my_task *task;
    int i;
    printk(KERN_INFO "sys_release_resources: Releasing resources for task %d\n", task_id);
    mutex_lock(&scheduler_lock);
    task = find_completed_task(task_id);
    if (!task) {
        mutex_unlock(&scheduler_lock);
        printk(KERN_INFO "sys_release_resources: Task %d not found in completed queue.\n", task_id);
        return -ENOENT;
    }
    // Release held resources
    for (i = 0; i < task->resource_count; i++)
        releaseResource(task->required_resources[i]);
    list_del(&task->list);
    mutex_unlock(&scheduler_lock);
    
    printk(KERN_INFO "sys_release_resources: Successfully released resources for task %d\n", task_id);
    kfree(task);
    return 0;
}

struct trace_event_functions enter_syscall_print_funcs = { 0 };
struct trace_event_class event_class_syscall_enter = { 0 };
struct trace_event_functions exit_syscall_print_funcs = { 0 };
struct trace_event_class event_class_syscall_exit = { 0 };

MODULE_LICENSE("GPL");
MODULE_AUTHOR("");
MODULE_DESCRIPTION("");

