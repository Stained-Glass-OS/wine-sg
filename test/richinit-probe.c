/* A new RichEdit has no placeholder text (patches/sg/1610). 1511 added the
 * placeholder to the editor but never set it when an editor was made: it
 * held whatever the heap held, and an empty control drew from it and freed
 * it (Word crashed on exit, in editor_draw and in the heap). Run with the
 * heap's fill-on-allocate on (GlobalFlag FLG_HEAP_ENABLE_FREE_CHECK), so the
 * leftover is never accidentally NULL. */
#include <windows.h>
#include <richedit.h>
#include <stdio.h>

static LONG faults;

/* a fault inside a window procedure is caught for it, so watch them all */
static LONG CALLBACK count_faults(EXCEPTION_POINTERS *info)
{
    if (info->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) InterlockedIncrement(&faults);
    return EXCEPTION_CONTINUE_SEARCH;
}

int main(void)
{
    static const WCHAR *classes[] = { L"RICHEDIT50W", L"RichEdit20W" };
    MSG msg;
    int i, n, bad = 0;
    COLORREF window = GetSysColor(COLOR_WINDOW), px;
    HDC dc;

    AddVectoredExceptionHandler(1, count_faults);
    LoadLibraryA("msftedit.dll");
    LoadLibraryA("riched20.dll");
    for (n = 0; n < 2; n++)
    {
        for (i = 0; i < 10; i++)
        {
            HWND hwnd = CreateWindowExW(0, classes[n], NULL, WS_OVERLAPPEDWINDOW | WS_VISIBLE | ES_MULTILINE,
                                        10, 10, 300, 200, 0, 0, 0, 0);
            if (!hwnd) { printf("FAIL  %ls not created\nRESULT: FAIL\n", classes[n]); return 1; }
            UpdateWindow(hwnd);
            RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ERASE);
            while (PeekMessageW(&msg, 0, 0, 0, PM_REMOVE)) DispatchMessageW(&msg);
            /* an empty control is all window colour: drawing stopped short
             * when the leftover placeholder was read */
            dc = GetDC(hwnd);
            px = GetPixel(dc, 150, 120);
            ReleaseDC(hwnd, dc);
            if (px != window) bad++;
            if (i == 0) printf("%ls: pixel %06lx, window colour %06lx\n", classes[n], px, window);
            DestroyWindow(hwnd);
        }
        if (faults) { printf("FAIL  %ld access violations drawing or destroying empty %ls controls\nRESULT: FAIL\n", faults, classes[n]); return 1; }
        if (bad) { printf("FAIL  empty %ls controls were not drawn (%d of 10)\nRESULT: FAIL\n", classes[n], bad); return 1; }
        printf("PASS  empty %ls controls are drawn and destroyed\n", classes[n]);
    }
    printf("RESULT: PASS\n");
    return 0;
}
