#include "window.h"

#include <assert.h>

static ATOM game_window_class = 0;

static LRESULT CALLBACK game_window_procedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {

    case WM_KEYUP:
        if (wparam == VK_ESCAPE)
            PostQuitMessage(0);
        else
            break;
    case WM_QUIT:
    case WM_DESTROY:
        PostQuitMessage(0);
    }

    return DefWindowProcA(hwnd, msg, wparam, lparam);
}

/* Retrieves the game window class or registers it, if it has not been registered yet. */
static ATOM get_game_window_class()
{
    if(game_window_class) {
        return game_window_class;
    }


    /* Register window class. */
    {
        WNDCLASSEXA c = {
            .cbSize = sizeof(c),
            .style = 0,
            .lpfnWndProc = game_window_procedure,
            .cbClsExtra = 0,
            .cbWndExtra = 0,
            .hInstance = GetModuleHandleA(0),
            .hIcon = /* FIX: Add icon. */ 0,
            .hCursor = 0,
            .hbrBackground = (HBRUSH)(COLOR_WINDOW + 1),
            .lpszMenuName = 0,
            .lpszClassName = "camellia",
            .hIconSm = /* FIX: Add small icon. */ 0
        };

        game_window_class = RegisterClassExA(&c);
    }

    if(!game_window_class)
        exit(EXIT_FAILURE);

    return game_window_class;
}

/* Creates a window. Returns true on success. */
bool window_init(Window* window, const char* window_name, unsigned short width, unsigned short height)
{
    assert(window);
    assert(window_name);
    assert(width && height);

    /* Calculate actual window size. */
    {
        RECT rect = {
            .top = 0,
            .right = width,
            .left = 0,
            .bottom = height,
        };
        AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

        width = rect.right;
        height = rect.bottom;
    }

    HWND hwnd = CreateWindowExA(
        WS_EX_APPWINDOW | WS_EX_WINDOWEDGE,
        (const char*)(uintptr_t)get_game_window_class(),
        window_name,
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        width,
        height,
        0,
        0,
        GetModuleHandleA(0),
        0
    );

    if(hwnd == 0)
        return false;

    (*window).hwnd = hwnd;

    return true;
}

/*
    Processes all window's messages in queue and yields the control back to the application.
    Returns true if windows has asked the application to exit.
*/
bool windows_process_msg_queue()
{
    MSG msg;
    bool requested_exit = false;

    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
    {
        if (msg.message == WM_QUIT)
            requested_exit = true; // Acknowledge the quit request, but do not yield just yet.

        TranslateMessage(&msg); // Translates virtual-key messages
        DispatchMessage(&msg);  // Sends the message to the window procedure
    }

    return requested_exit;
}
