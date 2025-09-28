#include "plugin_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define SENTINEL_END "<END>"

static plugin_context_t* g_ctx = NULL;

static inline const char* safe_name(plugin_context_t* ctx){
    return (ctx && ctx->name) ? ctx->name : "unknown";
}

void log_error(plugin_context_t* context, const char* message){
    fprintf(stderr, "[ERROR][%s] - %s\n", safe_name(context), message ? message : "(null)");
}

void log_info(plugin_context_t* context, const char* message){
    fprintf(stderr, "[INFO][%s] - %s\n", safe_name(context), message ? message : "(null)");
}

const char* plugin_get_name(void) {
    return (g_ctx && g_ctx->name) ? g_ctx->name : "unknown";
}

/* Consumer thread: drains queue, processes items, forwards to next stage (if any).
 * Contract:
 * - Items returned by consumer_producer_take are heap pointers we must free.
 * - Processed strings returned by process_func are heap pointers that we own and must free
 *   if there is no next stage.
 * - Sentinel handling:
 *   plugin_place_work() does not enqueue SENTINEL_END; it only signals 'finished' on the queue.
 *   After we drain the queue here, we propagate SENTINEL_END downstream (if any).
 */
void* plugin_consumer_thread(void* arg){
    plugin_context_t* context = (plugin_context_t*)arg;
    //log_info(context, "Consumer thread started");

    for(;;){
        char* item = consumer_producer_get(context->queue);
        if (!item) {
            // Queue is finished and empty.
            break;
        }

        // Process normally
        const char* processed = context->process_func(item);
        free(item); // queue item always freed here

        if (processed){
            if (context->next_place_work){
                const char* err = context->next_place_work(processed);
                if (err) log_error(context, err);
                free((void*)processed); // always free processed result after forwarding
            } else {
                // No next stage: we must free the processed result.
                free((void*)processed);
            }
        }
    }

    // Propagate sentinel to the next stage after draining
    if (context->next_place_work){
        const char* err = context->next_place_work(SENTINEL_END);
        if (err) log_error(context, err);
    }

    //log_info(context, "Consumer thread exiting");
    return NULL;
}

const char* common_plugin_init(const char* (*process_function)(const char*),
                               const char* name,
                               int queue_size)
{
    if (g_ctx) return "Plugin already initialized";
    if (!process_function || !name || queue_size <= 0) return "Invalid parameters";

    plugin_context_t* ctx = (plugin_context_t*)calloc(1, sizeof(*ctx));
    if (!ctx) return "Memory allocation failed";

    ctx->name = strdup(name);
    if (!ctx->name){
        free(ctx);
        return "Memory allocation failed";
    }
    ctx->queue = malloc(sizeof(consumer_producer_t));
    const char* err = consumer_producer_init(ctx->queue ,queue_size);
    if (err){
        free(ctx->name);
        free(ctx->queue);
        free(ctx);
        return err;
    }

    ctx->process_func = process_function;
    ctx->next_place_work = NULL;
    ctx->initialized = 0;

    int rc = pthread_create(&ctx->thread, NULL, plugin_consumer_thread, ctx);
    if (rc != 0){
        consumer_producer_destroy(ctx->queue);
        free(ctx->name);
        free(ctx);
        return "Failed to create consumer thread";
    }

    ctx->initialized = 1;
    g_ctx = ctx;
    //log_info(g_ctx, "Plugin initialized successfully");
    return NULL;
}

const char* plugin_place_work(const char* str){
    if (!g_ctx || !g_ctx->initialized) return "Plugin not initialized";
    if (!str) return "Invalid string parameter";

    if (strcmp(str, SENTINEL_END) == 0){
        // Signal queue completion; consumer will forward SENTINEL after draining
        consumer_producer_signal_finished(g_ctx->queue);
        return NULL;
    }

    const char* err = consumer_producer_put(g_ctx->queue, str);
    if (err) log_error(g_ctx, err);
    return err;
}

void plugin_attach(const char* (*next_place_work)(const char*)){
    if (!g_ctx) return;
    g_ctx->next_place_work = next_place_work;
}

const char* plugin_wait_finished(void){
    if (!g_ctx || !g_ctx->initialized) return "Plugin not initialized";
    int rc = pthread_join(g_ctx->thread, NULL);
    if (rc != 0) return "pthread_join failed";
    g_ctx->initialized = 0;
    return NULL;
}

const char* plugin_fini(void){
    if (!g_ctx) return "Plugin not initialized";

    // If still running, attempt to join
    if (g_ctx->initialized){
        const char* err = plugin_wait_finished();
        if (err) return err;
    }

    if (g_ctx->queue){
        consumer_producer_destroy(g_ctx->queue);
        free(g_ctx->queue);
        g_ctx->queue = NULL;
    }

    free(g_ctx->name);
    g_ctx->name = NULL;

    free(g_ctx);
    g_ctx = NULL;
    return NULL;
}
