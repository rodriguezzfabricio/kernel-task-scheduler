/*
 * tests/user_tests.c
 *
 * User-space test suite for task scheduler
 * and Resource Constraints.
 *
 * This file verifies:
 *   1. Basic Queue Operations (FIFO order)
 *   2. Priority Scheduling: That highest-priority tasks are selected first.
 *   3. Resource Management using polling:
 *         - Successful resource acquisition/release.
 *         - Failure when resources are unavailable.
 *   4. Waiting Queue Processing: Tasks that cannot acquire resources immediately
 *      remain in the waiting queue until resources become available.
 *   5. Additional Edge Cases:
 *         - Tasks with no required resources.
 *         - Tasks that require the same resource more than once.
 *         - is_empty() behavior on an empty queue.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <assert.h>
#include "queue.h"

// ---------------------------------------------------------------------------
// Global Resource Management (Polling-based)
// ---------------------------------------------------------------------------

// Global resource array (simulate: Resource A: 2, Resource B: 3, Resource C: 1)
int resources[NUM_RESOURCES];
pthread_mutex_t resource_mutex = PTHREAD_MUTEX_INITIALIZER;

/*
 * Attempt to acquire all resources needed by task 't' using polling.
 * Checks that every required resource is available; if not, returns -1.
 * If all are available, decrements the counts and returns 0.
 */
int test_acquire_resources(task_t *t) {
    int i, can_acquire = 1;
    
    pthread_mutex_lock(&resource_mutex);
    for (i = 0; i < t->resource_count; i++) {
        int r_id = t->required_resources[i];
        if (resources[r_id] <= 0) {
            can_acquire = 0;
            break;
        }
    }
    if (!can_acquire) {
        pthread_mutex_unlock(&resource_mutex);
        return -1;
    }
    for (i = 0; i < t->resource_count; i++) {
        int r_id = t->required_resources[i];
        resources[r_id]--;
    }
    pthread_mutex_unlock(&resource_mutex);
    return 0;
}

/*
 * Release all resources held by task 't' by incrementing the counts.
 */
void test_release_resources(task_t *t) {
    int i;
    pthread_mutex_lock(&resource_mutex);
    for (i = 0; i < t->resource_count; i++) {
        int r_id = t->required_resources[i];
        resources[r_id]++;
    }
    pthread_mutex_unlock(&resource_mutex);
}

// ---------------------------------------------------------------------------
// Test 1: Queue Operations (FIFO order and is_empty)
// ---------------------------------------------------------------------------
void test_queue_operations() {
    printf("Running test_queue_operations...\n");
    queue_t q;
    init_queue(&q);

    // Verify that a new queue is empty.
    if (is_empty(&q)) {
        printf("PASS: New queue is empty.\n");
    } else {
        printf("FAIL: New queue should be empty.\n");
    }

    // Create 3 tasks with distinct task IDs.
    task_t *t1 = malloc(sizeof(task_t)); t1->task_id = 1;
    task_t *t2 = malloc(sizeof(task_t)); t2->task_id = 2;
    task_t *t3 = malloc(sizeof(task_t)); t3->task_id = 3;

    enqueue(&q, t1);
    enqueue(&q, t2);
    enqueue(&q, t3);

    // The queue should no longer be empty.
    if (!is_empty(&q))
        printf("PASS: Queue is not empty after enqueuing tasks.\n");
    else
        printf("FAIL: Queue is unexpectedly empty.\n");

    task_t *d1 = dequeue(&q);
    task_t *d2 = dequeue(&q);
    task_t *d3 = dequeue(&q);

    if (d1->task_id == 1 && d2->task_id == 2 && d3->task_id == 3) {
        printf("PASS: Queue operations maintain FIFO order.\n");
    } else {
        printf("FAIL: Queue order incorrect. Got: %d, %d, %d\n",
               d1->task_id, d2->task_id, d3->task_id);
    }
    free(d1); free(d2); free(d3);
    destroy_queue(&q);
}

// ---------------------------------------------------------------------------
// Test 2: Priority Scheduling
// ---------------------------------------------------------------------------
void test_priority_scheduling() {
    printf("Running test_priority_scheduling...\n");
    // Simulate separate queues for high, medium, and low priorities.
    queue_t high_q, med_q, low_q;
    init_queue(&high_q);
    init_queue(&med_q);
    init_queue(&low_q);

    // Create tasks with different priorities.
    task_t *t1 = malloc(sizeof(task_t)); t1->task_id = 1; t1->priority = HIGH_PRIORITY;
    task_t *t2 = malloc(sizeof(task_t)); t2->task_id = 2; t2->priority = MEDIUM_PRIORITY;
    task_t *t3 = malloc(sizeof(task_t)); t3->task_id = 3; t3->priority = LOW_PRIORITY;

    // Enqueue tasks.
    enqueue(&low_q, t3);
    enqueue(&med_q, t2);
    enqueue(&high_q, t1);

    // Simulate consumer: High priority should be processed first.
    task_t *selected = NULL;
    pthread_mutex_lock(&high_q.mutex);
    if (high_q.head != NULL) {
        node_t *node = high_q.head;
        high_q.head = node->next;
        if (high_q.head == NULL)
            high_q.tail = NULL;
        selected = node->task;
        free(node);
    }
    pthread_mutex_unlock(&high_q.mutex);

    if (selected && selected->task_id == 1)
        printf("PASS: High-priority task selected first.\n");
    else
        printf("FAIL: High-priority task not selected correctly.\n");
    free(selected);

    // Clean up remaining tasks.
    task_t *temp;
    while (!is_empty(&med_q)) { temp = dequeue(&med_q); free(temp); }
    while (!is_empty(&low_q)) { temp = dequeue(&low_q); free(temp); }
    destroy_queue(&high_q); destroy_queue(&med_q); destroy_queue(&low_q);
}

// ---------------------------------------------------------------------------
// Test 3: Resource Management (Polling-based)
// ---------------------------------------------------------------------------
void test_resource_management() {
    printf("Running test_resource_management...\n");
    // Initialize resources: A:2, B:3, C:1.
    resources[RESOURCE_A] = 2;
    resources[RESOURCE_B] = 3;
    resources[RESOURCE_C] = 1;

    // Create a task that requires Resource A and Resource B.
    task_t *t = malloc(sizeof(task_t));
    t->task_id = 100;
    t->resource_count = 2;
    t->required_resources[0] = RESOURCE_A;
    t->required_resources[1] = RESOURCE_B;

    if (test_acquire_resources(t) == 0)
        printf("PASS: Resources acquired successfully for task %d.\n", t->task_id);
    else
        printf("FAIL: Resources should have been available for task %d.\n", t->task_id);

    // Check counts (should be: A:1, B:2).
    pthread_mutex_lock(&resource_mutex);
    int a = resources[RESOURCE_A], b = resources[RESOURCE_B];
    pthread_mutex_unlock(&resource_mutex);
    if (a == 1 && b == 2)
        printf("PASS: Resource counts updated correctly (A: %d, B: %d).\n", a, b);
    else
        printf("FAIL: Resource counts incorrect (A: %d, B: %d).\n", a, b);

    // Release resources.
    test_release_resources(t);
    pthread_mutex_lock(&resource_mutex);
    a = resources[RESOURCE_A]; b = resources[RESOURCE_B];
    pthread_mutex_unlock(&resource_mutex);
    if (a == 2 && b == 3)
        printf("PASS: Resources released correctly for task %d.\n", t->task_id);
    else
        printf("FAIL: Resource release incorrect (A: %d, B: %d).\n", a, b);

    // Test failure: Make Resource C unavailable.
    resources[RESOURCE_C] = 0;
    task_t *t_fail = malloc(sizeof(task_t));
    t_fail->task_id = 101;
    t_fail->resource_count = 1;
    t_fail->required_resources[0] = RESOURCE_C;

    if (test_acquire_resources(t_fail) != 0)
        printf("PASS: Correctly failed to acquire unavailable resource for task %d.\n", t_fail->task_id);
    else
        printf("FAIL: Should not have acquired resource for task %d.\n", t_fail->task_id);
    free(t); free(t_fail);
}

// ---------------------------------------------------------------------------
// Test 4: Scheduler Waiting Queue Processing
// ---------------------------------------------------------------------------
/*
 * Processes the waiting queue once (simulating a scheduler iteration).
 * For each task that can now acquire its resources, it simulates execution,
 * releases resources, and moves the task to the completed queue.
 *
 * Returns the number of tasks processed.
 */
int process_waiting_queue_once(queue_t *waiting, queue_t *completed) {
    int processed = 0;
    pthread_mutex_lock(&waiting->mutex);
    node_t *prev = NULL;
    node_t *curr = waiting->head;
    while (curr) {
        task_t *t = curr->task;
        if (test_acquire_resources(t) == 0) {
            // Remove task from waiting queue.
            if (prev == NULL)
                waiting->head = curr->next;
            else
                prev->next = curr->next;
            if (curr == waiting->tail)
                waiting->tail = prev;
            node_t *temp = curr;
            curr = curr->next;
            pthread_mutex_unlock(&waiting->mutex);
            
            // Simulate execution.
            sleep(1);
            test_release_resources(t);
            enqueue(completed, t);
            processed++;
            
            pthread_mutex_lock(&waiting->mutex);
        } else {
            prev = curr;
            curr = curr->next;
        }
    }
    pthread_mutex_unlock(&waiting->mutex);
    return processed;
}

void test_scheduler_waiting() {
    printf("Running test_scheduler_waiting...\n");
    queue_t wait_q, comp_q;
    init_queue(&wait_q);
    init_queue(&comp_q);

    // Simulate Resource A unavailable.
    resources[RESOURCE_A] = 0;
    resources[RESOURCE_B] = 3;
    resources[RESOURCE_C] = 1;

    // Create a task requiring Resource A and Resource B.
    task_t *t = malloc(sizeof(task_t));
    t->task_id = 200;
    t->resource_count = 2;
    t->required_resources[0] = RESOURCE_A;
    t->required_resources[1] = RESOURCE_B;

    enqueue(&wait_q, t);

    // Process waiting queue; task should not run since Resource A is unavailable.
    int processed = process_waiting_queue_once(&wait_q, &comp_q);
    if (processed == 0)
        printf("PASS: Waiting task not processed when resources are unavailable.\n");
    else
        printf("FAIL: Task should not have been processed.\n");

    // Make Resource A available.
    pthread_mutex_lock(&resource_mutex);
    resources[RESOURCE_A] = 2;
    pthread_mutex_unlock(&resource_mutex);

    // Process waiting queue again.
    processed = process_waiting_queue_once(&wait_q, &comp_q);
    if (processed == 1 && is_empty(&wait_q))
        printf("PASS: Waiting task processed successfully after resources became available.\n");
    else
        printf("FAIL: Waiting task not processed as expected.\n");

    destroy_queue(&wait_q);
    destroy_queue(&comp_q);
}

// ---------------------------------------------------------------------------
// Test 5: Edge Cases for Task Resource Requirements
// ---------------------------------------------------------------------------
void test_edge_cases() {
    printf("Running test_edge_cases...\n");
    // Edge Case 1: Task with zero required resources.
    task_t *t_zero = malloc(sizeof(task_t));
    t_zero->task_id = 300;
    t_zero->resource_count = 0; // No resources required.
    if (test_acquire_resources(t_zero) == 0)
        printf("PASS: Task with no required resources acquired resources trivially.\n");
    else
        printf("FAIL: Task with no required resources should succeed.\n");
    test_release_resources(t_zero);
    free(t_zero);

    // Edge Case 2: Task requiring the same resource twice.
    // For example, require Resource B twice.
    // If Resource B has count 3, acquiring twice should succeed.
    resources[RESOURCE_B] = 3;
    task_t *t_dup = malloc(sizeof(task_t));
    t_dup->task_id = 301;
    t_dup->resource_count = 2;
    t_dup->required_resources[0] = RESOURCE_B;
    t_dup->required_resources[1] = RESOURCE_B;
    if (test_acquire_resources(t_dup) == 0)
        printf("PASS: Task requiring duplicate resources acquired them (Resource B available: 3 -> 1).\n");
    else
        printf("FAIL: Task requiring duplicate resources should have acquired them.\n");
    // Check that Resource B count is now 1.
    pthread_mutex_lock(&resource_mutex);
    int b = resources[RESOURCE_B];
    pthread_mutex_unlock(&resource_mutex);
    if (b == 1)
        printf("PASS: Resource B count updated correctly after duplicate acquisition (expected 1, got %d).\n", b);
    else
        printf("FAIL: Resource B count incorrect after duplicate acquisition (expected 1, got %d).\n", b);
    test_release_resources(t_dup);
    free(t_dup);
}

// ---------------------------------------------------------------------------
// Main: Run All Tests
// ---------------------------------------------------------------------------
int main(void) {
    printf("==== Running User-Space Tests for Task Scheduler ====\n\n");

    test_queue_operations();
    printf("\n");
    test_priority_scheduling();
    printf("\n");
    test_resource_management();
    printf("\n");
    test_scheduler_waiting();
    printf("\n");
    test_edge_cases();
    printf("\n==== All Tests Completed ====\n");

    return 0;
}

