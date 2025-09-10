#include "plugin_common.h"
#include <ctype.h>


// Plugin-specific processing function
static const char* uppercaser_process(const char* str) 
{
    if (!str) return NULL;

    char* result = strdup(str);
    if (!result) return NULL;
    for (int i = 0; result[i] != '\0'; i++) 
    {
        result[i] = toupper(result[i]);
    }
    return result;
}

// Plugin initialization (new API)
plugin_context_t* plugin_init(int queue_size) 
{
    return common_plugin_init(uppercaser_process, "uppercaser", queue_size);
}