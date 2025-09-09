#include "plugin_common.h"
#include <string.h>
#include <stdlib.h>


// Plugin-specific processing function
static const char* flipper_process(const char* str) 
{
    if (!str) return NULL;

    size_t len = strlen(str);
    if (len == 0) return strdup("");

    char* result = malloc(len + 1);
    if (!result) return NULL;
    
    for (size_t i = 0; i < len; i++) 
    {
        result[i] = str[len - 1 - i];
    }
    result[len] = '\0';
    return result;
}

// Plugin initialization (new API)
plugin_context_t* plugin_init(int queue_size) 
{
    return common_plugin_init(flipper_process, "flipper", queue_size);
}