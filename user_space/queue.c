#include <stdio.h>
#include <stdlib.h>
#include "queue.h"

void init_queue(queue_t *q) {
    q->head = NULL;
    q->tail = NULL;
    pthread_mutex_init(&q->mutex, NULL);
    pthread_cond_init(&q->cond, NULL);
}

void enqueue(queue_t *q, task_t *task) {
    node_t *new_node = malloc(sizeof(node_t));
    if (!new_node) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    new_node->task = task;
    new_node->next = NULL;
    
    pthread_mutex_lock(&q->mutex);
    if (q->tail == NULL) {
        q->head = new_node;
        q->tail = new_node;
    } else {
        q->tail->next = new_node;
        q->tail = new_node;
    }
    pthread_cond_signal(&q->cond); // Signal that a new item is available
    pthread_mutex_unlock(&q->mutex);
}

task_t *dequeue(queue_t *q) {
    pthread_mutex_lock(&q->mutex);
    // Wait until there is an item available
    while (q->head == NULL) {
        pthread_cond_wait(&q->cond, &q->mutex);
    }
    node_t *node = q->head;
    q->head = node->next;
    if (q->head == NULL)
        q->tail = NULL;
    pthread_mutex_unlock(&q->mutex);
    
    task_t *task = node->task;
    free(node);
    return task;
}

int is_empty(queue_t *q) {
    pthread_mutex_lock(&q->mutex);
    int empty = (q->head == NULL);
    pthread_mutex_unlock(&q->mutex);
    return empty;
}

void destroy_queue(queue_t *q) {
    pthread_mutex_lock(&q->mutex);
    node_t *current = q->head;
    while (current) {
        node_t *next = current->next;
        free(current->task); // Free the task if still allocated
        free(current);
        current = next;
    }
    q->head = q->tail = NULL;
    pthread_mutex_unlock(&q->mutex);
    
    pthread_mutex_destroy(&q->mutex);
    pthread_cond_destroy(&q->cond);
}
