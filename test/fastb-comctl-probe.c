/* comctl32/user32: EM_FMTLINES soft breaks (v5 and v6 Edit), PGS_DRAGNDROP pager drop target (patch 3720) */
#define COBJMACROS
#include <windows.h>
#include <commctrl.h>
#include <ole2.h>
#include <stdio.h>
#include <string.h>

static int f;
#define CHECK(c, ...) do { if (!(c)) { f++; printf("FAIL line %d: %s: ", __LINE__, #c); printf(__VA_ARGS__); printf("\n"); } } while (0)

static void pump(DWORD ms)
{
    DWORD end = GetTickCount() + ms;
    MSG msg;
    while (GetTickCount() < end)
    {
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageA(&msg); }
        Sleep(10);
    }
}

static int count_str(const char *s, const char *needle)
{
    int n = 0;
    while ((s = strstr(s, needle))) { n++; s += strlen(needle); }
    return n;
}

static void test_edit(const char *label)
{
    static const char text[] = "aaa bbb ccc ddd eee fff ggg hhh iii jjj kkk lll mmm nnn ooo ppp qqq rrr sss ttt uuu vvv www xxx yyy zzz"
                               "\r\nhard line end\r\nlast aaa bbb ccc ddd eee fff ggg hhh iii jjj kkk lll mmm";
    char buf[1024], orig[1024];
    HWND edit = CreateWindowA("Edit", "", WS_POPUP | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL, 0, 0, 90, 300, NULL, NULL, GetModuleHandleA(NULL), NULL);
    LRESULT r; int lines0, lines1, soft, len0, hard0;
    DWORD s, e;

    CHECK(edit != NULL, "%s: create", label);
    if (!edit) return;
    SendMessageA(edit, WM_SETTEXT, 0, (LPARAM)text);
    SendMessageA(edit, WM_GETTEXT, sizeof(orig), (LPARAM)orig);
    lines0 = SendMessageA(edit, EM_GETLINECOUNT, 0, 0);
    len0 = strlen(orig);
    hard0 = count_str(orig, "\r\n");
    CHECK(lines0 > 4, "%s: wrapped into %d lines", label, lines0);
    CHECK(!strstr(orig, "\r\r\n"), "%s: no soft breaks to begin with", label);

    SendMessageA(edit, EM_SETSEL, 10, 10);
    r = SendMessageA(edit, EM_FMTLINES, TRUE, 0);
    SendMessageA(edit, EM_GETSEL, (WPARAM)&s, (LPARAM)&e);
    CHECK(s == 10 && e == 10, "%s: caret in the first line stays: %lu %lu", label, s, e);
    SendMessageA(edit, EM_FMTLINES, FALSE, 0);
    SendMessageA(edit, EM_SETSEL, len0 - 3, len0 - 1);
    r = SendMessageA(edit, EM_FMTLINES, TRUE, 0);
    CHECK(r == TRUE, "%s: FMTLINES(TRUE) returned %Id", label, r);
    SendMessageA(edit, WM_GETTEXT, sizeof(buf), (LPARAM)buf);
    soft = count_str(buf, "\r\r\n");
    CHECK(soft == lines0 - 1 - hard0, "%s: %d soft breaks, expected %d", label, soft, lines0 - 1 - hard0);
    CHECK((int)strlen(buf) == len0 + 3 * soft, "%s: length %d expected %d", label, (int)strlen(buf), len0 + 3 * soft);
    CHECK((int)SendMessageA(edit, WM_GETTEXTLENGTH, 0, 0) == len0 + 3 * soft, "%s: WM_GETTEXTLENGTH", label);
    CHECK(count_str(buf, "\r\n") == hard0 + soft, "%s: hard breaks kept (%d)", label, count_str(buf, "\r\n"));
    CHECK(strstr(buf, "hard line end\r\nlast"), "%s: hard break text intact", label);
    lines1 = SendMessageA(edit, EM_GETLINECOUNT, 0, 0);
    CHECK(lines1 == lines0, "%s: line count %d -> %d", label, lines0, lines1);
    SendMessageA(edit, EM_GETSEL, (WPARAM)&s, (LPARAM)&e);
    CHECK(s == (DWORD)(len0 - 3 + 3 * soft) && e == s + 2, "%s: selection in the last line moves with its text: %lu %lu", label, s, e);

    r = SendMessageA(edit, EM_FMTLINES, TRUE, 0);
    SendMessageA(edit, WM_GETTEXT, sizeof(buf), (LPARAM)buf);
    CHECK(count_str(buf, "\r\r\n") == soft && (int)strlen(buf) == len0 + 3 * soft, "%s: second TRUE changes nothing", label);

    SendMessageA(edit, EM_SETSEL, 5000, 5000);
    SendMessageA(edit, EM_GETSEL, (WPARAM)&s, (LPARAM)&e);
    CHECK(s == strlen(buf), "%s: caret at the end %lu", label, s);

    r = SendMessageA(edit, EM_FMTLINES, FALSE, 0);
    CHECK(r == FALSE, "%s: FMTLINES(FALSE) returned %Id", label, r);
    SendMessageA(edit, WM_GETTEXT, sizeof(buf), (LPARAM)buf);
    CHECK(!strcmp(buf, orig), "%s: soft breaks removed again", label);
    lines1 = SendMessageA(edit, EM_GETLINECOUNT, 0, 0);
    CHECK(lines1 == lines0, "%s: line count after removal %d", label, lines1);
    SendMessageA(edit, EM_GETSEL, (WPARAM)&s, (LPARAM)&e);
    CHECK(s == (DWORD)len0, "%s: caret at the end after removal %lu (len %d)", label, s, len0);

    /* soft breaks that came with the text are removed too */
    SendMessageA(edit, WM_SETTEXT, 0, (LPARAM)"ab\r\r\ncd\r\nef");
    r = SendMessageA(edit, EM_FMTLINES, FALSE, 0);
    SendMessageA(edit, WM_GETTEXT, sizeof(buf), (LPARAM)buf);
    CHECK(!strcmp(buf, "abcd\r\nef"), "%s: supplied soft break removed: %s", label, buf);

    /* a single-line control has no soft breaks */
    DestroyWindow(edit);
    edit = CreateWindowA("Edit", "one two three four five six", WS_POPUP | WS_VISIBLE, 0, 0, 60, 24, NULL, NULL, GetModuleHandleA(NULL), NULL);
    SendMessageA(edit, EM_FMTLINES, TRUE, 0);
    SendMessageA(edit, WM_GETTEXT, sizeof(buf), (LPARAM)buf);
    CHECK(!strcmp(buf, "one two three four five six"), "%s: single line unchanged", label);
    DestroyWindow(edit);
}

static LRESULT CALLBACK parent_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_NOTIFY)
    {
        NMHDR *h = (NMHDR *)lp;
        if (h->code == PGN_CALCSIZE)
        {
            NMPGCALCSIZE *c = (NMPGCALCSIZE *)lp;
            if (c->dwFlag == PGF_CALCWIDTH) c->iWidth = 700; else c->iHeight = 24;
            return 0;
        }
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

static void test_pager(void)
{
    WNDCLASSA wc = { 0 };
    HWND parent, pager, child, pager2;
    IDropTarget *target;
    POINTL pt;
    DWORD effect;
    RECT rc;
    int pos0, pos1, pos2, pos3;
    HRESULT hr;

    wc.lpfnWndProc = parent_proc; wc.lpszClassName = "fastbpager"; wc.hInstance = GetModuleHandleA(NULL);
    RegisterClassA(&wc);
    parent = CreateWindowA("fastbpager", "p", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 400, 200, NULL, NULL, wc.hInstance, NULL);
    pager = CreateWindowA(WC_PAGESCROLLERA, "", WS_CHILD | WS_VISIBLE | PGS_HORZ | PGS_DRAGNDROP, 10, 10, 150, 30, parent, (HMENU)1, wc.hInstance, NULL);
    child = CreateWindowA("Static", "wide", WS_CHILD | WS_VISIBLE, 0, 0, 700, 24, pager, NULL, wc.hInstance, NULL);
    CHECK(pager && child, "pager created");
    SendMessageA(pager, PGM_SETCHILD, 0, (LPARAM)child);
    SendMessageA(pager, PGM_RECALCSIZE, 0, 0);
    pump(100);

    target = GetPropA(pager, "OleDropTargetInterface");
    CHECK(target != NULL, "the pager registered a drop target");
    pager2 = CreateWindowA(WC_PAGESCROLLERA, "", WS_CHILD | WS_VISIBLE | PGS_HORZ, 10, 60, 150, 30, parent, (HMENU)2, wc.hInstance, NULL);
    CHECK(GetPropA(pager2, "OleDropTargetInterface") == NULL, "no drop target without PGS_DRAGNDROP");
    DestroyWindow(pager2);
    if (!target) { DestroyWindow(parent); return; }

    GetWindowRect(pager, &rc);
    pos0 = SendMessageA(pager, PGM_GETPOS, 0, 0);
    CHECK(pos0 == 0, "start position %d", pos0);

    /* over the middle: nothing happens */
    pt.x = (rc.left + rc.right) / 2; pt.y = (rc.top + rc.bottom) / 2; effect = 7;
    hr = IDropTarget_DragEnter(target, NULL, 0, pt, &effect);
    CHECK(hr == S_OK && effect == DROPEFFECT_NONE, "DragEnter %#lx effect %lu", hr, effect);
    pump(700);
    CHECK(SendMessageA(pager, PGM_GETPOS, 0, 0) == 0, "no scrolling away from the buttons");

    /* over the right button: scrolls, and keeps scrolling */
    pt.x = rc.right - 4;
    hr = IDropTarget_DragOver(target, 0, pt, &effect);
    CHECK(hr == S_OK && effect == DROPEFFECT_NONE, "DragOver %#lx effect %lu", hr, effect);
    pos1 = SendMessageA(pager, PGM_GETPOS, 0, 0);
    CHECK(pos1 > 0, "scrolled at once: %d", pos1);
    pump(900);
    pos2 = SendMessageA(pager, PGM_GETPOS, 0, 0);
    CHECK(pos2 > pos1, "kept scrolling: %d -> %d", pos1, pos2);

    /* off the button: stops */
    pt.x = (rc.left + rc.right) / 2;
    IDropTarget_DragOver(target, 0, pt, &effect);
    pos2 = SendMessageA(pager, PGM_GETPOS, 0, 0);
    pump(400);
    pos3 = SendMessageA(pager, PGM_GETPOS, 0, 0);
    CHECK(pos3 == pos2, "stopped scrolling off the button: %d -> %d", pos2, pos3);

    /* leave and drop also stop */
    pt.x = rc.left + 3;
    IDropTarget_DragOver(target, 0, pt, &effect);
    pump(800);
    IDropTarget_DragLeave(target);
    pos2 = SendMessageA(pager, PGM_GETPOS, 0, 0);
    pump(400);
    CHECK(SendMessageA(pager, PGM_GETPOS, 0, 0) == pos2, "DragLeave stops the scrolling");
    CHECK(pos2 < pos3, "the left button scrolled back (%d < %d)", pos2, pos3);
    IDropTarget_DragEnter(target, NULL, 0, pt, &effect);
    pump(800);
    effect = 7;
    hr = IDropTarget_Drop(target, NULL, 0, pt, &effect);
    CHECK(hr == S_OK && effect == DROPEFFECT_NONE, "Drop %#lx effect %lu", hr, effect);
    pos2 = SendMessageA(pager, PGM_GETPOS, 0, 0);
    pump(400);
    CHECK(SendMessageA(pager, PGM_GETPOS, 0, 0) == pos2, "Drop stops the scrolling");

    DestroyWindow(parent);
    CHECK(GetPropA(pager, "OleDropTargetInterface") == NULL, "gone with the window");
}

int main(void)
{
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_PAGESCROLLER_CLASS | ICC_STANDARD_CLASSES };
    HANDLE act = INVALID_HANDLE_VALUE; ULONG_PTR cookie = 0;
    char path[MAX_PATH], tmp[MAX_PATH];
    ACTCTXA ctx = { sizeof(ctx) };
    FILE *fp;

    OleInitialize(NULL);
    InitCommonControlsEx(&icc);
    test_edit("v5");
    test_pager();

    GetTempPathA(MAX_PATH, tmp);
    sprintf(path, "%sfastbcc.manifest", tmp);
    fp = fopen(path, "w");
    if (fp)
    {
        fputs("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n<assembly xmlns=\"urn:schemas-microsoft-com:asm.v1\" manifestVersion=\"1.0\">\n"
              "<assemblyIdentity version=\"1.0.0.0\" name=\"fastb.probe\" type=\"win32\"/>\n<dependency><dependentAssembly><assemblyIdentity type=\"win32\" "
              "name=\"Microsoft.Windows.Common-Controls\" version=\"6.0.0.0\" processorArchitecture=\"*\" publicKeyToken=\"6595b64144ccf1df\" language=\"*\"/>"
              "</dependentAssembly></dependency></assembly>\n", fp);
        fclose(fp);
        ctx.lpSource = path;
        act = CreateActCtxA(&ctx);
        if (act != INVALID_HANDLE_VALUE && ActivateActCtx(act, &cookie))
        {
            InitCommonControlsEx(&icc);
            test_edit("v6");
            DeactivateActCtx(0, cookie);
        }
        else CHECK(0, "no v6 activation context (%lu)", GetLastError());
        DeleteFileA(path);
    }
    printf("%s\n", f ? "RESULT: FAIL" : "RESULT: PASS");
    return f != 0;
}
