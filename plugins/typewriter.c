#include "plugin_common.h"
#include <string.h>
#include <stdio.h>
#include <unistd.h>


// Plugin-specific processing function
static const char* typewriter_process(const char* str) 
{
    if (!str) return NULL;

    fputs("[typewriter] ", stdout);
    for (const char* p = str; *p != '\0'; p++) 
    {
        putchar(*p);
        fflush(stdout);
        usleep(100000); // 100ms delay per character
    }

    putchar('\n');
    fflush(stdout);
    return strdup(str);
}

// Plugin initialization (new API)
const char* plugin_init(int queue_size) 
{
    return common_plugin_init(typewriter_process, "typewriter", queue_size);
}