#include "plugin_common.h"
#include <string.h>
#include <stdlib.h>

// Plugin-specific processing function
static const char* expander_process(const char* str) 
{
    if (!str) return NULL;

    size_t len = strlen(str);
    if (len == 0) return strdup("");

    size_t new_len = len * 2;
    char* result = malloc(new_len + 1);
    if (!result) return NULL;

    for (size_t i = 0; i < len; i++) 
    {
        result[i * 2] = str[i]; //even index
        result[i * 2 + 1] = ' '; //odd index
    }
    result[new_len] = '\0';
    return result;
}

// Plugin initialization (new API)
plugin_context_t* plugin_init(int queue_size) 
{
    return common_plugin_init(expander_process, "expander", queue_size);
}