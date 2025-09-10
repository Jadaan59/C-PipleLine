#include "plugin_common.h"

// Plugin-specific processing function
static const char* logger_process(const char* str) 
{
    if (!str) return NULL;

    // Log the string
    printf("[LOGGER] %s\n", str);
    // Return the original string unchanged
    return strdup(str);
}

// Plugin initialization (new API)
plugin_context_t* plugin_init(int queue_size) 
{
    return common_plugin_init(logger_process, "logger", queue_size);
}