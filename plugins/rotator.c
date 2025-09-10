#include "plugin_common.h"
#include <string.h>

// Plugin-specific processing function
static const char* rotator_process(const char* str) 
{
    if (!str) return NULL;

    size_t len = strlen(str);
    if (len == 0) return strdup("");

    char* result = malloc(len + 1);
    if (!result) return NULL;

    
    for (size_t i = 1; i < len -1 ; i++) 
    {
        result[i] = str[i +1];
    }
    result[len -1] = str[0];
    result[len] = '\0';
    return result;
}

// Plugin initialization (new API)
plugin_context_t* plugin_init(int queue_size) 
{
    return common_plugin_init(rotator_process, "rotator", queue_size);
}