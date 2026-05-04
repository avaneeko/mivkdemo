#ifndef WINDOW_H
#define WINDOW_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <windows.h>

typedef struct {
    HWND hwnd;
} Window;

/* Creates a window. Returns true on success. */
bool window_init(Window* window, const char* window_name, unsigned short width, unsigned short height);

/*
    Processes all window's messages in queue and yields the control back to the application. 
    Returns true if windows has asked the application to exit.
*/
bool windows_process_msg_queue();

#ifdef __cplusplus
}
#endif

#endif