#include "consumer_producer.h"
#include <stdlib.h>
#include <string.h>

const char* consumer_producer_init(consumer_producer_t* queue, int capacity) 
{
    if (!queue || capacity <= 0) return "Invalid parameters";
    
    // Allocate items array
    queue->items = malloc(capacity * sizeof(char*));
    if (!queue->items) return "Memory allocation failed";
    
    // Initialize all items to NULL
    for (int i = 0; i < capacity; i++) 
    {
        queue->items[i] = NULL;
    }
    
    // Initialize queue state
    queue->capacity = capacity;
    queue->count = 0;
    queue->head = 0;
    queue->tail = 0;
    queue->finished = 0;

    // Initialize mutex
    if (pthread_mutex_init(&queue->mutex, NULL) != 0)
    {
        free(queue->items);
        return "Failed to initialize mutex";
    }
    // Initialize monitors
    if (monitor_init(&queue->not_full_monitor) != 0) 
    {
        pthread_mutex_destroy(&queue->mutex);
        free(queue->items);
        return "Failed to initialize not_full_monitor";
    }
    
    if (monitor_init(&queue->not_empty_monitor) != 0) 
    {
        pthread_mutex_destroy(&queue->mutex);
        monitor_destroy(&queue->not_full_monitor);
        free(queue->items);
        return "Failed to initialize not_empty_monitor";
    }
    
    if (monitor_init(&queue->finished_monitor) != 0) 
    {
        pthread_mutex_destroy(&queue->mutex);
        monitor_destroy(&queue->not_full_monitor);
        monitor_destroy(&queue->not_empty_monitor);
        free(queue->items);
        return "Failed to initialize finished_monitor";
    }
    
    return NULL;
}

void consumer_producer_destroy(consumer_producer_t* queue) 
{
    if (!queue) return;
    
    // Free any remaining items
    for (int i = 0; i < queue->count; i++) 
    {
        int index = (queue->head + i) % queue->capacity;
        if (queue->items[index]) 
        {
            free(queue->items[index]);
        }
    }
    
    // Free items array
    if (queue->items) 
    {
        free(queue->items);
        queue->items = NULL;
        queue->count = 0;
        queue->head = 0;
        queue->tail = 0;
    }
    
    // Destroy monitors
    monitor_destroy(&queue->not_full_monitor);
    monitor_destroy(&queue->not_empty_monitor);
    monitor_destroy(&queue->finished_monitor);

    // Destroy mutex
    pthread_mutex_destroy(&queue->mutex);
}

const char* consumer_producer_put(consumer_producer_t* queue, const char* item) 
{
    if (!queue || !item) return "Invalid parameters";
    
    pthread_mutex_lock(&queue->mutex);
    
    // Check if queue is already finished
    if (queue->finished) 
    {
        pthread_mutex_unlock(&queue->mutex);
        return "Queue is finished";
    }
    
    // Wait for space in queue
    while (queue->count >= queue->capacity && !queue->finished)     
    {
        monitor_reset(&queue->not_full_monitor);
        pthread_mutex_unlock(&queue->mutex);
        if (monitor_wait(&queue->not_full_monitor) != 0) {
            return "Monitor wait failed";
        }
        pthread_mutex_lock(&queue->mutex);
        
        // Recheck finished state after reacquiring lock
        if (queue->finished) {
            pthread_mutex_unlock(&queue->mutex);
            return "Queue is finished";
        }
    }
    
    // Allocate and copy the string
    char* item_copy = strdup(item);
    if (!item_copy) 
    {
        pthread_mutex_unlock(&queue->mutex);
        return "Memory allocation failed";
    }
    
    // Add to queue
    queue->items[queue->tail] = item_copy;
    queue->tail = (queue->tail + 1) % queue->capacity;
    queue->count++;
    
    // Signal that queue is not empty
    monitor_signal(&queue->not_empty_monitor);
    pthread_mutex_unlock(&queue->mutex);
    
    return NULL;
}

char* consumer_producer_get(consumer_producer_t* queue) 
{
    if (!queue) return NULL;
    
    pthread_mutex_lock(&queue->mutex);
    
    // Wait for item in queue or finished signal
    while (queue->count == 0 && !queue->finished) 
    {
        monitor_reset(&queue->not_empty_monitor);
        pthread_mutex_unlock(&queue->mutex);
        if (monitor_wait(&queue->not_empty_monitor) != 0) {
            return NULL;
        }
        pthread_mutex_lock(&queue->mutex);
    }
    
    // Return NULL if queue is empty and finished
    if (queue->count == 0) {
        pthread_mutex_unlock(&queue->mutex);
        return NULL;
    }
    
    // Get item from queue
    char* item = queue->items[queue->head];
    queue->items[queue->head] = NULL; // Clear the slot
    queue->head = (queue->head + 1) % queue->capacity;
    queue->count--;
    
    // Signal that queue is not full
    monitor_signal(&queue->not_full_monitor);

    // Transition: non-empty -> empty
    if (queue->count == 0) {
        monitor_reset(&queue->not_empty_monitor);
        if (queue->finished) {
            // If producer closed and we just drained the last item, notify waiters
            monitor_signal(&queue->finished_monitor);
        }
    }

    pthread_mutex_unlock(&queue->mutex);
    
    return item;
}

void consumer_producer_signal_finished(consumer_producer_t* queue) 
{
    if (!queue) return;
    
    pthread_mutex_lock(&queue->mutex);
    queue->finished = 1;  // Set the finished flag
    
    // Signal all monitors to wake up any waiting threads
    monitor_signal(&queue->finished_monitor);
    monitor_signal(&queue->not_full_monitor);
    monitor_signal(&queue->not_empty_monitor);  // Wake up any waiting consumers
    pthread_mutex_unlock(&queue->mutex);
}

int consumer_producer_wait_finished(consumer_producer_t* queue) 
{
    if (!queue) return -1;
    
    pthread_mutex_lock(&queue->mutex);
    // Wait until finished flag is set
    while (!queue->finished && queue->count == 0) 
    {
        monitor_reset(&queue->finished_monitor);
        pthread_mutex_unlock(&queue->mutex);
        if (monitor_wait(&queue->finished_monitor) != 0) return -1;

        pthread_mutex_lock(&queue->mutex);
    }
    
    pthread_mutex_unlock(&queue->mutex);
    return 0;
} 