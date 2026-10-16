#ifndef KQUEUE_H
#define KQUEUE_H

#include <linux/types.h>


asmlinkage long sys_add_task(int task_id, int duration, int priority);

asmlinkage long sys_schedule_tasks(void);

asmlinkage long sys_acquire_resources(int task_id);

asmlinkage long sys_release_resources(int task_id);

#endif /* KQUEUE_H */
