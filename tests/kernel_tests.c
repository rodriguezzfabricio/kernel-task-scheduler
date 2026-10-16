#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>        // For syscall()
#include <sys/syscall.h>   // For SYS_* definitions
#include <errno.h>
#include <string.h>

// Adjust these to match the syscall numbers used in your kernel
#ifndef __NR_sys_add_task
#define __NR_sys_add_task 549
#endif

#ifndef __NR_sys_schedule_tasks
#define __NR_sys_schedule_tasks 550
#endif

#ifndef __NR_sys_acquire_resources
#define __NR_sys_acquire_resources 551
#endif

#ifndef __NR_sys_release_resources
#define __NR_sys_release_resources 552
#endif

// Helper wrappers
static inline long my_sys_add_task(int task_id, int duration, int priority) {
    return syscall(__NR_sys_add_task, task_id, duration, priority);
}

static inline long my_sys_schedule_tasks(void) {
    return syscall(__NR_sys_schedule_tasks);
}

static inline long my_sys_acquire_resources(int task_id) {
    return syscall(__NR_sys_acquire_resources, task_id);
}

static inline long my_sys_release_resources(int task_id) {
    return syscall(__NR_sys_release_resources, task_id);
}

// Utility
static void check_syscall(const char* desc, long ret) {
    if (ret < 0)
        fprintf(stderr, "[FAIL] %s => %ld, errno=%d (%s)\n", desc, ret, errno, strerror(errno));
    else
        printf("[ OK ] %s => %ld\n", desc, ret);
}

// Test: Add tasks
static void test_add_multiple_tasks(void) {
    long ret;
    printf("\n=== test_add_multiple_tasks ===\n");

    ret = my_sys_add_task(1, 2, 0);  // High
    check_syscall("sys_add_task(1,2,High)", ret);

    ret = my_sys_add_task(2, 3, 1);  // Medium
    check_syscall("sys_add_task(2,3,Medium)", ret);

    ret = my_sys_add_task(3, 1, 2);  // Low
    check_syscall("sys_add_task(3,1,Low)", ret);

    ret = my_sys_add_task(99, 10, 99);  // Invalid
    check_syscall("sys_add_task(99,10,priority=99) [expect fail]", ret);
}

// Test: Acquire
static void test_acquire_resources(void) {
    long ret;
    printf("\n=== test_acquire_resources ===\n");

    ret = my_sys_acquire_resources(1);
    check_syscall("sys_acquire_resources(1)", ret);

    ret = my_sys_acquire_resources(999);
    check_syscall("sys_acquire_resources(999) [expect fail]", ret);
}

// Test: Release
static void test_release_resources(void) {
    long ret;
    printf("\n=== test_release_resources ===\n");

    ret = my_sys_release_resources(1);
    check_syscall("sys_release_resources(1)", ret);

    ret = my_sys_release_resources(999);
    check_syscall("sys_release_resources(999) [expect fail]", ret);
}

// Test: Scheduler behavior
static void test_schedule_logic(void) {
    long ret;
    printf("\n=== test_schedule_logic ===\n");

    ret = my_sys_add_task(4, 2, 0);
    check_syscall("sys_add_task(4,2,High)", ret);

    ret = my_sys_acquire_resources(4);
    check_syscall("sys_acquire_resources(4)", ret);

    ret = my_sys_schedule_tasks();
    check_syscall("sys_schedule_tasks()", ret);

    ret = my_sys_schedule_tasks();
    check_syscall("sys_schedule_tasks() [2nd call]", ret);

    ret = my_sys_release_resources(4);
    check_syscall("sys_release_resources(4)", ret);
}

// Test: Integration case
static void test_full_integration(void) {
    long ret;
    printf("\n=== test_full_integration ===\n");

    ret = my_sys_add_task(10, 2, 0);
    check_syscall("sys_add_task(10,2,High)", ret);

    ret = my_sys_add_task(11, 5, 1);
    check_syscall("sys_add_task(11,5,Medium)", ret);

    ret = my_sys_add_task(12, 5, 2);
    check_syscall("sys_add_task(12,5,Low)", ret);

    ret = my_sys_schedule_tasks();
    check_syscall("sys_schedule_tasks() [1st pass]", ret);

    ret = my_sys_add_task(13, 3, 0);
    check_syscall("sys_add_task(13,3,High)", ret);

    ret = my_sys_acquire_resources(13);
    if (ret < 0)
        printf("Task #13 couldn't acquire resources, may be waiting.\n");
    else
        printf("Task #13 acquired resources.\n");

    ret = my_sys_schedule_tasks();
    check_syscall("sys_schedule_tasks() [2nd pass]", ret);

    check_syscall("release_resources(10)", my_sys_release_resources(10));
    check_syscall("release_resources(11)", my_sys_release_resources(11));
    check_syscall("release_resources(12)", my_sys_release_resources(12));
    check_syscall("release_resources(13)", my_sys_release_resources(13));

    ret = my_sys_schedule_tasks();
    check_syscall("sys_schedule_tasks() [final pass]", ret);
}

int main(void) {
    printf("=== KERNEL TESTS ===\n");

    test_add_multiple_tasks();
    test_acquire_resources();
    test_release_resources();
    test_schedule_logic();
    test_full_integration();

    printf("=== ALL TESTS DONE ===\n");
    return 0;
}

