#include "plugin_common.h"
#include <string.h>
#include <stdio.h>

// Plugin-specific processing function
static const char* logger_process(const char* str) 
{
    if (!str) return NULL;

    // Log the string
    fputs("[logger] ", stdout);
    fputs(str, stdout);
    putchar('\n');
    fflush(stdout);
    // Return the original string unchanged
    return strdup(str);
}

// Plugin initialization (new API)
const char* plugin_init(int queue_size) 
{
    return common_plugin_init(logger_process, "logger", queue_size);
}