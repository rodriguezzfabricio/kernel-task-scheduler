#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include "queue.h"

// Global queues for each priority, waiting tasks, and completed tasks.
queue_t high_queue, medium_queue, low_queue, waiting_queue, completed_queue;

// Global resource counts (simulate Resource A:2, B:3, C:1)
int resources[NUM_RESOURCES];
pthread_mutex_t resource_mutex = PTHREAD_MUTEX_INITIALIZER;

/*
 * Try to acquire all resources needed by task 't' using polling.
 * If any required resource is not available, no resources are acquired.
 * Returns 0 on success, -1 on failure.
 */
int acquire_resources(task_t *t) {
    int i;
    int can_acquire = 1;
    
    pthread_mutex_lock(&resource_mutex);
    // Check availability of each required resource
    for(i = 0; i < t->resource_count; i++) {
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
    
    // All required resources are available; decrement their counts.
    for(i = 0; i < t->resource_count; i++) {
        int r_id = t->required_resources[i];
        resources[r_id]--;
    }
    
    pthread_mutex_unlock(&resource_mutex);
    return 0;
}

/*
 * Release all resources held by task 't'.
 */
void release_resources(task_t *t) {
    int i;
    pthread_mutex_lock(&resource_mutex);
    for(i = 0; i < t->resource_count; i++) {
        int r_id = t->required_resources[i];
        resources[r_id]++;
    }
    pthread_mutex_unlock(&resource_mutex);
}

/*
 * Consumer thread:
 * Continuously checks the high, medium, then low priority queues.
 * If a task is available, attempt to acquire its resources via polling.
 * If resources cannot be acquired, move the task to the waiting queue.
 * Otherwise, execute the task (simulate with sleep) and then release resources.
 */
void* consumer(void* arg) {
    while (1) {
        task_t *t = NULL;
        
        // Try high priority queue
        pthread_mutex_lock(&high_queue.mutex);
        if (high_queue.head != NULL) {
            node_t *node = high_queue.head;
            high_queue.head = node->next;
            if (high_queue.head == NULL)
                high_queue.tail = NULL;
            t = node->task;
            free(node);
        }
        pthread_mutex_unlock(&high_queue.mutex);
        
        // Then medium priority queue
        if (!t) {
            pthread_mutex_lock(&medium_queue.mutex);
            if (medium_queue.head != NULL) {
                node_t *node = medium_queue.head;
                medium_queue.head = node->next;
                if (medium_queue.head == NULL)
                    medium_queue.tail = NULL;
                t = node->task;
                free(node);
            }
            pthread_mutex_unlock(&medium_queue.mutex);
        }
        
        // Then low priority queue
        if (!t) {
            pthread_mutex_lock(&low_queue.mutex);
            if (low_queue.head != NULL) {
                node_t *node = low_queue.head;
                low_queue.head = node->next;
                if (low_queue.head == NULL)
                    low_queue.tail = NULL;
                t = node->task;
                free(node);
            }
            pthread_mutex_unlock(&low_queue.mutex);
        }
        
        // If no task was found, wait a bit and try again.
        if (!t) {
            sleep(1);
            continue;
        }
        
        // Attempt to acquire resources (polling)
        if (acquire_resources(t) != 0) {
            printf("Task %d cannot acquire resources now. Moving to waiting queue.\n", t->task_id);
            enqueue(&waiting_queue, t);
            continue;
        }
        
        // Execute the task (simulate execution with sleep)
        printf("Executing task %d (priority %d, duration %d sec)\n", t->task_id, t->priority, t->duration);
        sleep(t->duration);
        release_resources(t);
        printf("Task %d finished execution. Resources released.\n", t->task_id);
        
        // Optionally, move the task to the completed queue.
        enqueue(&completed_queue, t);
    }
    return NULL;
}

/*
 * Producer thread:
 * Periodically creates new tasks with random attributes and enqueues them
 * into the appropriate priority queue.
 */
void* producer(void* arg) {
    int task_id = 1;
    while (1) {
        task_t *t = malloc(sizeof(task_t));
        if (!t) {
            perror("malloc");
            exit(EXIT_FAILURE);
        }
        t->task_id = task_id++;
        t->duration = (rand() % 5) + 1;      // Duration between 1 and 5 seconds
        t->priority = rand() % 3;            // 0: High, 1: Medium, 2: Low
        
        // For simulation, randomly require 1 or 2 resources.
        t->resource_count = (rand() % 2) + 1;
        for (int i = 0; i < t->resource_count; i++) {
            t->required_resources[i] = rand() % NUM_RESOURCES;
        }
        
        // Enqueue the task in the corresponding priority queue.
        if (t->priority == HIGH_PRIORITY) {
            enqueue(&high_queue, t);
        } else if (t->priority == MEDIUM_PRIORITY) {
            enqueue(&medium_queue, t);
        } else {
            enqueue(&low_queue, t);
        }
        
        printf("Producer created task %d (priority %d, duration %d sec, requires %d resource(s))\n",
               t->task_id, t->priority, t->duration, t->resource_count);
        sleep(2);  // Produce a new task every 2 seconds
    }
    return NULL;
}

/*
 * Scheduler thread:
 * Periodically polls the waiting queue and attempts to acquire resources
 * for waiting tasks. If successful, the task is executed.
 */
void* scheduler(void* arg) {
    while (1) {
        pthread_mutex_lock(&waiting_queue.mutex);
        node_t *prev = NULL;
        node_t *curr = waiting_queue.head;
        while (curr) {
            task_t *t = curr->task;
            if (acquire_resources(t) == 0) {
                // Remove task from waiting queue.
                if (prev == NULL) {
                    waiting_queue.head = curr->next;
                } else {
                    prev->next = curr->next;
                }
                if (curr == waiting_queue.tail)
                    waiting_queue.tail = prev;
                node_t *to_free = curr;
                curr = curr->next;
                pthread_mutex_unlock(&waiting_queue.mutex);
                
                printf("Executing waiting task %d (duration %d sec)\n", t->task_id, t->duration);
                sleep(t->duration);
                release_resources(t);
                printf("Waiting task %d finished execution. Resources released.\n", t->task_id);
                enqueue(&completed_queue, t);
                
                pthread_mutex_lock(&waiting_queue.mutex);
            } else {
                prev = curr;
                curr = curr->next;
            }
        }
        pthread_mutex_unlock(&waiting_queue.mutex);
        sleep(1);
    }
    return NULL;
}

int main(void) {
    srand(time(NULL));
    
    // Initialize queues.
    init_queue(&high_queue);
    init_queue(&medium_queue);
    init_queue(&low_queue);
    init_queue(&waiting_queue);
    init_queue(&completed_queue);
    
    // Initialize resource counts (without semaphores).
    resources[RESOURCE_A] = 2;
    resources[RESOURCE_B] = 3;
    resources[RESOURCE_C] = 1;
    
    // Create threads: one producer, two consumers, and one scheduler.
    pthread_t prod_thread, cons_thread1, cons_thread2, sched_thread;
    pthread_create(&prod_thread, NULL, producer, NULL);
    pthread_create(&cons_thread1, NULL, consumer, NULL);
    pthread_create(&cons_thread2, NULL, consumer, NULL);
    pthread_create(&sched_thread, NULL, scheduler, NULL);
    
    // In this simulation, threads run indefinitely.
    pthread_join(prod_thread, NULL);
    pthread_join(cons_thread1, NULL);
    pthread_join(cons_thread2, NULL);
    pthread_join(sched_thread, NULL);
    
    // Cleanup (this part may not be reached in an infinite loop simulation).
    destroy_queue(&high_queue);
    destroy_queue(&medium_queue);
    destroy_queue(&low_queue);
    destroy_queue(&waiting_queue);
    destroy_queue(&completed_queue);
    pthread_mutex_destroy(&resource_mutex);
    
    return 0;
}
