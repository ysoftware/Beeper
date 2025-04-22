#define HOTRELOADING_ENABLED 1

#include <stdbool.h>
#include <stdio.h>
#include "raylib.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <time.h>
#include <unistd.h>
#include <dlfcn.h>

#if HOTRELOADING_ENABLED
void *plugin_handle;
void (*plug_init)(void);
void (*plug_update)(void);
void (*plug_cleanup)(void);
void* (*plug_pre_reload)(void);
void (*plug_post_reload)(void*);

#ifdef _WIN32
char *library_path = ".\\build\\libplug.dll";
#elif __APPLE__
char *library_path = "./build/libplug.dylib";
#else
char *library_path = "./build/libplug.so";
#endif

char *library_lockfile_path = "./build/libplug.lock";
time_t last_library_load_time = 0;

#define LOAD_LIBRARY() dlopen(library_path, RTLD_LAZY);
#define GET_SYMBOL(name) dlsym(plugin_handle, name);
#define UNLOAD_LIBRARY() dlclose(plugin_handle);
#define PREPARE_ERROR() char *error = dlerror();

void load_library(void) {
    if (plugin_handle) UNLOAD_LIBRARY();

    plugin_handle = LOAD_LIBRARY();
    if (!plugin_handle) goto fail;
    plug_init = (void(*)(void)) GET_SYMBOL("plug_init");
    if (!plug_init) goto fail;
    plug_update = (void(*)(void)) GET_SYMBOL("plug_update");
    if (!plug_update) goto fail;
    plug_pre_reload = (void*(*)(void)) GET_SYMBOL("plug_pre_reload");
    if (!plug_pre_reload) goto fail;
    plug_post_reload = (void(*)(void*)) GET_SYMBOL("plug_post_reload");
    if (!plug_post_reload) goto fail;
    plug_cleanup = (void(*)(void)) GET_SYMBOL("plug_cleanup");
    if (!plug_cleanup) goto fail;

    last_library_load_time = time(NULL);
    return;

fail: {
        PREPARE_ERROR();
        printf("main.c: load_library %s: Error: %s\n", library_path, error);
        if (plugin_handle) UNLOAD_LIBRARY();
        exit(1);
    }
}

bool is_library_file_modified(void) {
    struct stat attributes;
    stat(library_path, &attributes);
    time_t modified_time = attributes.st_mtime;

    // check if existing file is newer than last loaded version
    if (modified_time <= last_library_load_time)  return false;

    // check if lock file exists
    if (access(library_lockfile_path, F_OK) == 0)  return false;

    return true;
}

#else // HOTRELOADING_ENABLED
#include "plug.c"
#endif // HOTRELOADING_ENABLED

void clean_before_exit(void) {
    printf("Received SIGSEV.\n");

#if HOTRELOADING_ENABLED
    if (plug_cleanup != NULL) 
#endif
    {
        plug_cleanup();
    }
    exit(1);
}

#define SCREEN_WIDTH 900
#define SCREEN_HEIGHT 800

int main(void) {
    printf("\033[0m"); // clean colors

#if HOTRELOADING_ENABLED
    load_library();
#endif

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    SetConfigFlags(FLAG_VSYNC_HINT);
    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Beeper");
    SetTargetFPS(0);
    SetExitKey(KEY_NULL);

    plug_init();

    while (!WindowShouldClose()) {

#if HOTRELOADING_ENABLED
        if (is_library_file_modified()) {
            void *state = plug_pre_reload();
            load_library();
            plug_post_reload(state);
            printf("Hotreloading successful\n");
        }
#endif // HOTRELOADING_ENABLED

        plug_update();
    }

    plug_cleanup();

#if HOTRELOADING_ENABLED
    UNLOAD_LIBRARY();
#endif

    CloseWindow();

    printf("Exiting...\n");
    return 0;
}
