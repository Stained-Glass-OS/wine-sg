/* taskviewmove-hook: a WH_GETMESSAGE hook the taskviewmove probe puts into the taskbar's thread
 * (patches/sg/6529). For each WM_PAINT of the watched window it records how much of the window
 * was to be painted, in shared memory "Local\sg_taskviewmove".
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>

struct shared
{
    HWND watch;
    LONG paints;
    RECT last;          /* the last WM_PAINT's update rectangle, client coordinates */
    LONG width, height; /* the watched window's client size then */
};

static struct shared *shared;

__declspec(dllexport) LRESULT CALLBACK hook_proc( int code, WPARAM wparam, LPARAM lparam )
{
    MSG *msg = (MSG *)lparam;

    if (!shared)
    {
        HANDLE map = OpenFileMappingW( FILE_MAP_ALL_ACCESS, FALSE, L"Local\\sg_taskviewmove" );
        if (map) shared = MapViewOfFile( map, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(*shared) );
    }
    if (code >= 0 && shared && wparam == PM_REMOVE && msg->message == WM_PAINT && msg->hwnd == shared->watch)
    {
        RECT client;
        GetUpdateRect( msg->hwnd, &shared->last, FALSE );
        GetClientRect( msg->hwnd, &client );
        shared->width = client.right;
        shared->height = client.bottom;
        InterlockedIncrement( &shared->paints );
    }
    return CallNextHookEx( NULL, code, wparam, lparam );
}

BOOL WINAPI DllMain( HINSTANCE inst, DWORD reason, void *reserved )
{
    return TRUE;
}
