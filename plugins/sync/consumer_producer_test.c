#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include "consumer_producer.h"
#include "monitor.h"

#define PRODUCERS 4
#define CONSUMERS 4
#define ITEMS_TOTAL 200
#define LOST_WAKEUP_RUNS 500

// === Structs for Thread Info ===
typedef struct {
    consumer_producer_t *cp;
    int id;
    int items_to_produce;
    int items_produced;
} producer_info_t;

typedef struct {
    consumer_producer_t *cp;
    int id;
    int items_consumed;
    bool released;
} consumer_info_t;

// === Utility Testing Macro ===
#define TEST_ASSERT(condition, message) \
    do { \
        if (condition) { \
            printf("\033[0;32m\u2713 PASS:\033[0m %s\n", message); \
        } else { \
            printf("\033[0;31m\u2717 FAIL:\033[0m %s\n", message); \
        } \
    } while(0)

// === Blocking Behavior Test ===
void test_blocking_behavior() {
    printf("\n=== Blocking Behavior Test ===\n");
    consumer_producer_t cp;
    consumer_producer_init(&cp, 1);

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    pthread_t t;
    pthread_create(&t, NULL, (void*(*)(void*))consumer_producer_get, &cp);

    usleep(50000); // consumer should be blocked here
    consumer_producer_put(&cp, "hello");
    pthread_join(t, NULL);

    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed = (end.tv_sec - start.tv_sec) +
                     (end.tv_nsec - start.tv_nsec) / 1e9;
    TEST_ASSERT(elapsed >= 0.05, "Consumer was truly blocked (not busy-wait)");

    consumer_producer_destroy(&cp);
}

// === Lost Wakeup Simulation ===
void* sleepy_consumer(void* arg) {
    consumer_info_t* info = (consumer_info_t*)arg;
    usleep((rand() % 2000) + 1000); // random delay before starting
    while (true) {
        char* item = consumer_producer_get(info->cp);
        if (!item) break;
        free(item);
        info->items_consumed++;
    }
    return NULL;
}

void test_lost_wakeup_protection() {
    printf("\n=== Lost Wakeup Protection (Aggressive) ===\n");
    int failures = 0;

    for (int run = 0; run < LOST_WAKEUP_RUNS; run++) {
        consumer_producer_t cp;
        consumer_producer_init(&cp, 1);

        pthread_t t1;
        consumer_info_t ci = { .cp = &cp, .id = 1, .items_consumed = 0 };
        pthread_create(&t1, NULL, sleepy_consumer, &ci);

        usleep((rand() % 2000) + 1000); // race condition window
        consumer_producer_put(&cp, "X");
        consumer_producer_signal_finished(&cp);
        pthread_join(t1, NULL);

        if (ci.items_consumed != 1) failures++;
        consumer_producer_destroy(&cp);
    }
    TEST_ASSERT(failures == 0, "No lost wakeup detected across multiple runs");
}

// === Broadcast vs Signal Test ===
void* wait_and_report(void* arg) {
    consumer_info_t* info = (consumer_info_t*)arg;
    while (true) {
        char* item = consumer_producer_get(info->cp);
        if (!item) break;
        free(item);
        info->items_consumed++;
    }
    info->released = true; // mark that thread was released
    return NULL;
}

void test_broadcast_needed() {
    printf("\n=== Broadcast vs Signal ===\n");
    consumer_producer_t cp;
    consumer_producer_init(&cp, 2);

    pthread_t c[3];
    consumer_info_t infos[3] = {0};
    for (int i = 0; i < 3; i++) {
        infos[i].cp = &cp;
        infos[i].id = i;
        infos[i].released = false;
        pthread_create(&c[i], NULL, wait_and_report, &infos[i]);
    }
    printf("Consumers created, now blocking...\n");
    consumer_producer_put(&cp, "X");
    usleep(5000); // give time for consumers to block
    printf("Now signaling finished...\n");
    consumer_producer_signal_finished(&cp);
    printf("Waiting for consumers to finish...\n");
    for (int i = 0; i < 3; i++) pthread_join(c[i], NULL);

    int released_count = 0;
    int total_consumed = 0;
    for (int i = 0; i < 3; i++) {
        total_consumed += infos[i].items_consumed;
        if (infos[i].released) released_count++;
    }

    TEST_ASSERT(total_consumed == 1, "Only one item consumed");
    TEST_ASSERT(released_count == 3, "All consumers were released after finished");
    consumer_producer_destroy(&cp);
}

// === Memory Leak Check (manual) ===
void test_memory_leak_manual_check() {
    printf("\n=== Memory Leak Manual Check ===\n");
    consumer_producer_t cp;
    consumer_producer_init(&cp, 10);
    for (int i = 0; i < 10; i++) {
        consumer_producer_put(&cp, strdup("hello"));
    }
    consumer_producer_signal_finished(&cp);
    char* msg;
    while ((msg = consumer_producer_get(&cp))) {
        free(msg);
    }
    consumer_producer_destroy(&cp);
    printf("\u2713 Run under Valgrind to confirm no leaks\n");
}

int main() {
    srand(time(NULL));
    test_blocking_behavior();
    test_lost_wakeup_protection();
    test_broadcast_needed();
    test_memory_leak_manual_check();
    return 0;
}
