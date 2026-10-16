#ifndef QUEUE_H
#define QUEUE_H

#include <pthread.h>

#define MAX_RESOURCES 3

// Resource definitions
#define RESOURCE_A 0
#define RESOURCE_B 1
#define RESOURCE_C 2
#define NUM_RESOURCES 3

// Task priorities
#define HIGH_PRIORITY   0
#define MEDIUM_PRIORITY 1
#define LOW_PRIORITY    2

// Task structure for our scheduler
typedef struct task {
    int task_id;                // Unique task identifier
    int duration;               // Execution duration (in seconds)
    int priority;               // Task priority (0: High, 1: Medium, 2: Low)
    int required_resources[MAX_RESOURCES]; // Array of required resource IDs
    int resource_count;         // Number of required resources
} task_t;

// Node for linked-list implementation of the queue
typedef struct node {
    task_t *task;
    struct node *next;
} node_t;

// Queue structure
typedef struct queue {
    node_t *head;
    node_t *tail;
    pthread_mutex_t mutex;
    pthread_cond_t cond;        // To signal when items are added
} queue_t;

// Function prototypes for queue operations
void init_queue(queue_t *q);
void enqueue(queue_t *q, task_t *task);
task_t *dequeue(queue_t *q); // Blocking dequeue (waits if queue is empty)
int is_empty(queue_t *q);
void destroy_queue(queue_t *q);

#endif /* QUEUE_H */

