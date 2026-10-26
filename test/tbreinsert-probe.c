/* tbreinsert-probe: a toolbar button read with TB_GETBUTTON, deleted and inserted again with
 * TB_INSERTBUTTONA keeps its text (patches/sg/7007) -- MFC's CToolBar::_SetButton, and so
 * WinMerge's menu bar. Prints key=1|0 lines.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>

static BOOL text_is(HWND tb, int id, const WCHAR *want)
{
    WCHAR buf[64] = {0};
    LRESULT r = SendMessageW(tb, TB_GETBUTTONTEXTW, id, (LPARAM)buf);
    if (getenv("TB_DEBUG")) printf("  text %d -> %ld \"%ls\"\n", id, (long)r, buf);
    return !lstrcmpW(buf, want);
}

int main(void)
{
    INITCOMMONCONTROLSEX ic = { sizeof(ic), ICC_BAR_CLASSES };
    WCHAR *names[3];
    TBBUTTON b[3], got;
    HWND parent, tb;
    int i;
    BOOL ok;

    InitCommonControlsEx(&ic);
    parent = CreateWindowW(L"static", L"p", WS_OVERLAPPEDWINDOW, 0, 0, 300, 100, NULL, NULL, NULL, NULL);
    tb = CreateWindowExW(0, TOOLBARCLASSNAMEW, NULL, WS_CHILD | TBSTYLE_FLAT | TBSTYLE_LIST, 0, 0, 300, 30, parent, (HMENU)1, NULL, NULL);
    SendMessageW(tb, TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0);

    /* the texts live in buffers the program frees right after adding them, as WinMerge's do */
    for (i = 0; i < 3; i++)
    {
        names[i] = HeapAlloc(GetProcessHeap(), 0, 32 * sizeof(WCHAR));
        wsprintfW(names[i], L" Menu%d ", i);
        memset(&b[i], 0, sizeof(b[i]));
        b[i].iBitmap = I_IMAGENONE; b[i].idCommand = 100 + i; b[i].fsState = TBSTATE_ENABLED;
        b[i].fsStyle = BTNS_BUTTON | BTNS_AUTOSIZE; b[i].iString = (INT_PTR)names[i];
    }
    SendMessageW(tb, TB_ADDBUTTONSW, 3, (LPARAM)b);
    for (i = 0; i < 3; i++) { memset(names[i], 0xcc, 32 * sizeof(WCHAR)); HeapFree(GetProcessHeap(), 0, names[i]); }
    printf("added_texts=%d\n", text_is(tb, 100, L" Menu0 ") && text_is(tb, 102, L" Menu2 "));

    /* MFC: read, delete, insert what was read -- the ANSI message */
    for (i = 0; i < 3; i++)
    {
        SendMessageW(tb, TB_GETBUTTON, i, (LPARAM)&got);
        got.fsState |= TBSTATE_CHECKED;
        SendMessageW(tb, TB_DELETEBUTTON, i, 0);
        SendMessageA(tb, TB_INSERTBUTTONA, i, (LPARAM)&got);
    }
    ok = text_is(tb, 100, L" Menu0 ") && text_is(tb, 101, L" Menu1 ") && text_is(tb, 102, L" Menu2 ");
    printf("reinserted_texts=%d\n", ok);
    printf("state_changed=%d\n", (SendMessageW(tb, TB_GETSTATE, 101, 0) & TBSTATE_CHECKED) != 0);

    /* twice more, as idle updates do */
    for (i = 0; i < 2; i++)
    {
        SendMessageW(tb, TB_GETBUTTON, 1, (LPARAM)&got);
        SendMessageW(tb, TB_DELETEBUTTON, 1, 0);
        SendMessageA(tb, TB_INSERTBUTTONA, 1, (LPARAM)&got);
    }
    printf("reinserted_again=%d\n", text_is(tb, 101, L" Menu1 "));

    /* a program's own ANSI text is still read as ANSI */
    memset(&got, 0, sizeof(got));
    got.iBitmap = I_IMAGENONE; got.idCommand = 200; got.fsState = TBSTATE_ENABLED; got.fsStyle = BTNS_BUTTON;
    got.iString = (INT_PTR)"Ansi text";
    SendMessageA(tb, TB_INSERTBUTTONA, 0, (LPARAM)&got);
    printf("ansi_text=%d\n", text_is(tb, 200, L"Ansi text"));

    DestroyWindow(parent);
    printf("done=1\n");
    return 0;
}
