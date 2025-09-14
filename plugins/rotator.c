#include "plugin_common.h"
#include <string.h>
#include <stdlib.h>

// Plugin-specific processing function
static const char* rotator_process(const char* str) 
{
    if (!str) return NULL;

    size_t len = strlen(str);
    if (len == 0) return strdup("");

    char* result = malloc(len + 1);
    if (!result) return NULL;

    result[0] = str[len -1];
    for (size_t i = 0; i < len -1 ; i++) result[i+1] = str[i];
    
    result[len] = '\0';
    return result;
}

// Plugin initialization (new API)
const char* plugin_init(int queue_size) 
{
    return common_plugin_init(rotator_process, "rotator", queue_size);
}