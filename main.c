#define D_GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <link.h>
#include <pthread.h>

// Plugin function type definitions 
typedef const char* (*plugin_init_func_t)(int);
typedef const char* (*plugin_fini_func_t)(void);
typedef const char* (*plugin_place_work_func_t)(const char*);
typedef void        (*plugin_attach_func_t)(plugin_place_work_func_t);
typedef const char* (*plugin_wait_finished_func_t)(void);

// Plugin handle structure
typedef struct 
{
    plugin_init_func_t init;
    plugin_fini_func_t fini;
    plugin_place_work_func_t place_work;
    plugin_attach_func_t attach;
    plugin_wait_finished_func_t wait_finished;
    char* name;
    void* handle;
} plugin_handle_t;

// Function to print usage information
void print_usage(void) 
{
    printf("Usage: ./analyzer <queue_size> <plugin1> <plugin2> ... <pluginN>\n\n");
    printf("Arguments:\n");
    printf("  queue_size    Maximum number of items in each plugin's queue\n");
    printf("  plugin1..N    Names of plugins to load (without .so extension)\n\n");
    printf("Available plugins:\n");
    printf("  logger        - Logs all strings that pass through\n");
    printf("  typewriter    - Simulates typewriter effect with delays\n");
    printf("  uppercaser    - Converts strings to uppercase\n");
    printf("  rotator       - Move every character to the right. Last character moves to the beginning.\n");
    printf("  flipper       - Reverses the order of characters\n");
    printf("  expander      - Expands each character with spaces\n\n");
    printf("Example:\n");
    printf("  ./analyzer 20 uppercaser rotator logger\n");
    printf("  echo 'hello' | ./analyzer 20 uppercaser rotator logger\n");
    printf("  echo '<END>' | ./analyzer 20 uppercaser rotator logger\n");
}

// Function to load a plugin
static plugin_handle_t* load_plugin(const char* plugin_name) 
{
    char filename[256];
    snprintf(filename, sizeof(filename), "output/%s.so", plugin_name);
    void* handle = NULL;
    
    // Open the shared object
    #ifdef LM_ID_NEWLM
    handle = dlmopen(LM_ID_NEWLM, filename, RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        fprintf(stderr, "dlmopen failed for %s: %s\n", filename, dlerror());
    }
    #endif

    if (!handle) {
        handle = dlopen(filename, RTLD_NOW | RTLD_LOCAL);
        if (!handle) {
            fprintf(stderr, "dlopen failed for %s: %s\n", filename, dlerror());
            return NULL;
        }
    }
    
    // Allocate plugin handle
    plugin_handle_t* plugin = malloc(sizeof(plugin_handle_t));
    if (!plugin) 
    {
        dlclose(handle);
         return NULL;
    }
    
    // Clear dlerror
    dlerror();
    
    // Resolve function symbols
    plugin->init          = (plugin_init_func_t)         dlsym(handle, "plugin_init");
    plugin->fini          = (plugin_fini_func_t)         dlsym(handle, "plugin_fini");
    plugin->place_work    = (plugin_place_work_func_t)   dlsym(handle, "plugin_place_work");
    plugin->attach        = (plugin_attach_func_t)       dlsym(handle, "plugin_attach");
    plugin->wait_finished = (plugin_wait_finished_func_t)dlsym(handle, "plugin_wait_finished");
    
    // Check for errors
    char* error = dlerror();
    if (error) 
    {
        fprintf(stderr, "Error resolving symbols for plugin %s: %s\n", plugin_name, error);
        dlclose(handle);
        free(plugin);
        return NULL;
    }
    
    // Store plugin info
    plugin->name = strdup(plugin_name);
    plugin->handle = handle;
    
    return plugin;
}

// Function to free a plugin handle
void free_plugin(plugin_handle_t* plugin) 
{
    if (plugin) 
    {
        if (plugin->handle) dlclose(plugin->handle);
        if (plugin->name) free(plugin->name);
        free(plugin);
    }
}

int main(int argc, char* argv[]) 
{
    // Check command line arguments
    if (argc < 3) 
    {
        fprintf(stderr, "Error: Insufficient arguments\n");
        print_usage();
        return 1;
    }
    
    // Parse queue size
    int queue_size = atoi(argv[1]);
    if (queue_size <= 0) 
    {
        fprintf(stderr, "Error: Queue size must be a positive integer\n");
        print_usage();
        return 1;
    }
    
    // Calculate number of plugins
    int num_plugins = argc - 2;
    plugin_handle_t** plugins = malloc(num_plugins * sizeof(plugin_handle_t*));
    if (!plugins) 
    {
        fprintf(stderr, "Error: Memory allocation failed\n");
        return 1;
    }
    
    // Load all plugins
    for (int i = 0; i < num_plugins; i++) 
    {
        plugins[i] = load_plugin(argv[i + 2]);
        if (!plugins[i]) 
        {
            fprintf(stderr, "Error: Failed to load plugin %s\n", argv[i + 2]);
            // Clean up already loaded plugins
            for (int j = 0; j < i; j++) free_plugin(plugins[j]);
            free(plugins);
            print_usage();
            return 1;
        }
    }
    
    // Initialize all plugins
    for (int i = 0; i < num_plugins; i++) 
    {
        const char* error = plugins[i]->init(queue_size);
        
        if (error) 
        {
            fprintf(stderr, "Error initializing plugin %s: %s\n", plugins[i]->name, error ? error : "Unknown error");
            // Clean up
            for (int j = 0; j <= i; j++) 
            {
                if (j < i) plugins[j]->fini();
                free_plugin(plugins[j]);
            }
            free(plugins);
            return 2;
        }
    }
    
    // Attach plugins together
    for (int i = 0; i < num_plugins - 1; i++) 
    {
        plugins[i]->attach(plugins[i+1]->place_work);
    }
    
    // Detach last plugin from any next plugin
    if (num_plugins > 0) plugins[num_plugins - 1]->attach(NULL);
    
    // Read input from STDIN and feed to first plugin
    char line[1025]; // 1024 chars + null terminator
    while (fgets(line, sizeof(line), stdin)) 
    {
        // Remove trailing newline
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') line[len - 1] = '\0';

        // Send to first plugin
        const char* error = plugins[0]->place_work(line);
        if (error) 
        {
            fprintf(stderr, "Error placing work: %s\n", error);
            break;
        }
        // Check for END signal and exit loop
        if (strcmp(line, "<END>") == 0) break;
    }
    
    
    for (int i = 0; i < num_plugins; i++) 
    {
        const char* error = plugins[i]->wait_finished();
        if (error) fprintf(stderr, "Error waiting for plugin %s to finish: %s\n", plugins[i]->name, error);
    }
    
    // Finalize all plugins - this will wait for their threads to complete
    for (int i = 0; i < num_plugins; i++) 
    {
        const char* error = plugins[i]->fini();
        if (error) fprintf(stderr, "Error finalizing plugin %s: %s\n", plugins[i]->name, error);
    }
    
    // Clean up
    for (int i = 0; i < num_plugins; i++) free_plugin(plugins[i]);
    free(plugins);
    
    printf("Pipeline shutdown complete\n");
    return 0;
} 