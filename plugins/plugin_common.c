#include "plugin_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static plugin_context_t* g_ctx = NULL;


void* plugin_consumer_thread(void* arg) 
{
    plugin_context_t* context = (plugin_context_t*)arg;    
    log_info(context, "Consumer thread started");
    
    while (1) 
    {
        // Get item from queue
        char* current_item = consumer_producer_get(context->queue);
        if (!current_item) break;
        
        // Process the item
        const char* processed = context->process_func(current_item);
        // Free the current item
        free(current_item);
        // Handle the processed result
        if (processed && context->next_place_work) 
        {
            // Forward to next plugin if available
            const char* error = context->next_place_work(processed);
            if (error) log_error(context, error);
        }
        
    }
    
    // After processing all items, propagate <END> to next plugin if we have one
    if (context->next_place_work ) 
    {
         const char* err = context->next_place_work("<END>");
        if (err) log_error(context, err);
    }
    
    log_info(context, "Consumer thread finished");
    return NULL;
}

void log_error(plugin_context_t* context, const char* message) 
{
    fprintf(stderr,"[ERROR][%s] - %s\n", context->name, message);
}

void log_info(plugin_context_t* context, const char* message) 
{
    fprintf(stderr, "[INFO][%s] - %s\n", context->name, message);
}


const char* plugin_get_name(void) 
{
    return g_ctx ? g_ctx->name : NULL;
}


const char* common_plugin_init(const char* (*process_function)(const char*), const char* name, int queue_size) 
{
    if (g_ctx) return "Plugin already initialized";
    if (!process_function || !name || queue_size <= 0) return "Invalid parameters";
    
    plugin_context_t* context = (plugin_context_t*)malloc(sizeof(plugin_context_t));
    if (!context) return "Memory allocation failed";

    char* copy = strdup(name);
    if (!copy) free(context); return "Memory allocation failed";

    context->name = copy;
    context->process_func = process_function;
    context->next_place_work = NULL;
    context->next_context = NULL;
    context->initialized = 0;
    context->queue = (consumer_producer_t*)malloc(sizeof(consumer_producer_t));
    if (!context->queue)
    {
        free(context->name);
        free(context);
        return "Memory allocation failed";
    }
    const char* error = consumer_producer_init(context->queue, queue_size);
    if (error)
    {
        free(context->queue);
        free(context->name);
        free(context);
        return error;
    }

    //start the worker thread
    int process = pthread_create(&context->thread, NULL, plugin_consumer_thread, context);
    if (process != 0)
    {
        consumer_producer_destroy(context->queue);
        free(context->queue);
        free(context->name);
        free(context);
        return "Failed to create consumer thread";
    }

    context->initialized = 1;
    g_ctx = context;
    log_info(context, "Plugin initialized successfully");
    return NULL;
}


const char* plugin_place_work(const char* str) 
{
    if (!g_ctx || !g_ctx->initialized) return "Plugin not initialized";
    if (!str) return "Invalid string parameter";

    if (strcmp(str, "<END>") == 0) 
    {
        consumer_producer_signal_finished(g_ctx->queue);
        // Don't propagate <END> here - let consumer thread do it after processing all items
        return NULL;
    }
    const char* result = consumer_producer_put(g_ctx->queue, str);
    if (result) log_error(g_ctx, result);

    return result;
}


void plugin_attach(const char* (*next_place_work)(const char*))
{
    if (!g_ctx) return;
    g_ctx->next_place_work = next_place_work;
}


const char* plugin_wait_finished(void) 
{
    if (!g_ctx || !g_ctx->initialized) return "Plugin not initialized";
    return consumer_producer_wait_finished(g_ctx->queue) == 0 ? NULL : "Wait failed";
}

const char* plugin_fini(void) 
{
    if(!g_ctx || !g_ctx->initialized) return "Plugin not initialized";
    log_info(g_ctx, "Finalizing plugin");
    g_ctx->next_place_work = NULL; // Disconnect from next plugin

    // Signal finished to stop accepting new items
    consumer_producer_signal_finished(g_ctx->queue);

    int join_res = pthread_join(g_ctx->thread, NULL);
    if (join_res != 0) log_error(g_ctx, "Failed to join consumer thread");

    // Clean up resources
    consumer_producer_destroy(g_ctx->queue);
    free(g_ctx->queue);
    free(g_ctx->name);
    g_ctx->queue = NULL;
    g_ctx->name = NULL;
    g_ctx->initialized = 0;
    free(g_ctx);
    g_ctx = NULL;

   return (join_res == 0) ? NULL : "Failed to finalize plugin properly";
}
