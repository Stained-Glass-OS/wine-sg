/* comctl32 batch (patches/sg/2050), run by test/cc3-gate.sh: ComboBoxEx
 * CBEM_GETITEM asking the parent for callback fields, HDM_SETORDERARRAY with
 * entries out of range or repeated, and the edit control's WM_SIZE/WM_DESTROY
 * results.
 *
 *   cc3-probe.exe */
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <string.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECK(expr) check((expr), #expr)

static int notifies;
static UINT last_mask;
static BOOL set_item;
static char callback_text[] = "from callback";

static LRESULT CALLBACK parent_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_NOTIFY)
    {
        NMHDR *hdr = (NMHDR *)lp;
        if (hdr->code == CBEN_GETDISPINFOA)
        {
            NMCOMBOBOXEXA *n = (NMCOMBOBOXEXA *)lp;
            notifies++;
            last_mask = n->ceItem.mask;
            if (n->ceItem.mask & CBEIF_IMAGE) n->ceItem.iImage = 123;
            if (n->ceItem.mask & CBEIF_INDENT) n->ceItem.iIndent = 4;
            if (n->ceItem.mask & CBEIF_TEXT) n->ceItem.pszText = callback_text;
            if (set_item) n->ceItem.mask |= CBEIF_DI_SETITEM;
            return 0;
        }
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

static unsigned order_of(HWND h, int count)
{
    INT o[8];
    unsigned v = 0;
    int i;
    SendMessageA(h, HDM_GETORDERARRAY, count, (LPARAM)o);
    for (i = 0; i < count; i++) v = v * 16 + o[i];
    return v;
}

static unsigned set_order(HWND h, int count, unsigned start, unsigned set)
{
    INT o[8];
    int i;
    for (i = 0; i < count; i++) o[i] = (start >> (4 * (count - 1 - i))) & 0xf;
    SendMessageA(h, HDM_SETORDERARRAY, count, (LPARAM)o);
    for (i = 0; i < count; i++) o[i] = (set >> (4 * (count - 1 - i))) & 0xf;
    SendMessageA(h, HDM_SETORDERARRAY, count, (LPARAM)o);
    return order_of(h, count);
}

int main(void)
{
    WNDCLASSA wc = { 0 };
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_USEREX_CLASSES | ICC_WIN95_CLASSES };
    HWND parent, combo, header, edit;
    COMBOBOXEXITEMA item;
    HDITEMA hdi;
    char text[64];
    int i;

    InitCommonControlsEx(&icc);
    wc.lpfnWndProc = parent_proc;
    wc.hInstance = GetModuleHandleA(NULL);
    wc.lpszClassName = "cc3parent";
    RegisterClassA(&wc);
    parent = CreateWindowA("cc3parent", "p", WS_OVERLAPPEDWINDOW, 0, 0, 300, 200, NULL, NULL, wc.hInstance, NULL);

    /* ComboBoxEx: callback fields are asked for when they are read */
    combo = CreateWindowExA(0, WC_COMBOBOXEXA, NULL, WS_CHILD | WS_VISIBLE | CBS_DROPDOWN, 0, 0, 200, 100, parent, (HMENU)1, wc.hInstance, NULL);
    memset(&item, 0, sizeof(item));
    item.mask = CBEIF_TEXT | CBEIF_IMAGE | CBEIF_INDENT | CBEIF_SELECTEDIMAGE;
    item.pszText = LPSTR_TEXTCALLBACKA;
    item.iImage = I_IMAGECALLBACK;
    item.iSelectedImage = I_IMAGECALLBACK;
    item.iIndent = I_INDENTCALLBACK;
    CHECK(SendMessageA(combo, CBEM_INSERTITEMA, 0, (LPARAM)&item) == 0);

    notifies = 0; last_mask = 0;
    memset(&item, 0, sizeof(item));
    item.mask = CBEIF_IMAGE;
    CHECK(SendMessageA(combo, CBEM_GETITEMA, 0, (LPARAM)&item) == 1);
    CHECK(notifies == 1 && last_mask == CBEIF_IMAGE && item.iImage == 123);

    notifies = 0;
    item.mask = CBEIF_IMAGE;
    item.iImage = 0;
    SendMessageA(combo, CBEM_GETITEMA, 0, (LPARAM)&item);
    CHECK(notifies == 1 && item.iImage == 123);                 /* asked again: not stored */

    notifies = 0; last_mask = 0;
    memset(&item, 0, sizeof(item));
    item.mask = CBEIF_IMAGE | CBEIF_INDENT;
    SendMessageA(combo, CBEM_GETITEMA, 0, (LPARAM)&item);
    CHECK(notifies == 1 && last_mask == (CBEIF_IMAGE | CBEIF_INDENT) && item.iImage == 123 && item.iIndent == 4);

    notifies = 0;
    memset(&item, 0, sizeof(item));
    item.mask = CBEIF_LPARAM;
    SendMessageA(combo, CBEM_GETITEMA, 0, (LPARAM)&item);
    CHECK(notifies == 0);                                       /* nothing asked that is a callback */

    notifies = 0;
    memset(&item, 0, sizeof(item));
    text[0] = 0;
    item.mask = CBEIF_TEXT;
    item.pszText = text;
    item.cchTextMax = sizeof(text);
    SendMessageA(combo, CBEM_GETITEMA, 0, (LPARAM)&item);
    CHECK(notifies == 1 && !strcmp(text, "from callback"));

    set_item = TRUE;
    notifies = 0; last_mask = 0;
    memset(&item, 0, sizeof(item));
    item.mask = CBEIF_IMAGE;
    SendMessageA(combo, CBEM_GETITEMA, 0, (LPARAM)&item);
    CHECK(notifies == 1 && item.iImage == 123);
    notifies = 0;
    item.iImage = 0;
    SendMessageA(combo, CBEM_GETITEMA, 0, (LPARAM)&item);
    CHECK(notifies == 0 && item.iImage == 123);                 /* set for good */
    notifies = 0; last_mask = 0;
    item.mask = CBEIF_IMAGE | CBEIF_INDENT;
    SendMessageA(combo, CBEM_GETITEMA, 0, (LPARAM)&item);
    CHECK(notifies == 1 && last_mask == CBEIF_INDENT);          /* only what is still a callback */
    set_item = FALSE;
    DestroyWindow(combo);

    /* the order of a header's items */
    header = CreateWindowA(WC_HEADERA, NULL, WS_CHILD | HDS_BUTTONS, 0, 0, 400, 20, parent, (HMENU)2, wc.hInstance, NULL);
    memset(&hdi, 0, sizeof(hdi));
    hdi.mask = HDI_WIDTH;
    hdi.cxy = 30;
    for (i = 0; i < 4; i++) SendMessageA(header, HDM_INSERTITEMA, i, (LPARAM)&hdi);
    CHECK(set_order(header, 4, 0x0123, 0x0123) == 0x0123);
    CHECK(set_order(header, 4, 0x0123, 0x3210) == 0x3210);
    CHECK(set_order(header, 4, 0x0123, 0x4012) == 0x0132);
    CHECK(set_order(header, 4, 0x0132, 0x4014) == 0x0312);
    CHECK(set_order(header, 4, 0x0123, 0x4102) == 0x1032);
    CHECK(set_order(header, 4, 0x0123, 0x4412) == 0x0132);
    CHECK(set_order(header, 4, 0x0123, 0x4441) == 0x0231);
    CHECK(set_order(header, 4, 0x0123, 0x1444) == 0x1023);
    CHECK(set_order(header, 4, 0x0123, 0x0122) == 0x0132);
    CHECK(set_order(header, 4, 0x0123, 0x4444) == 0x0123);
    DestroyWindow(header);

    /* the edit control */
    edit = CreateWindowA("EDIT", "x", WS_CHILD | WS_VISIBLE, 0, 0, 100, 20, parent, (HMENU)3, wc.hInstance, NULL);
    CHECK(SendMessageA(edit, WM_SIZE, 0, MAKELONG(50, 10)) == 1);
    CHECK(SendMessageA(edit, WM_DESTROY, 0, 0) == 1);
    DestroyWindow(edit);

    DestroyWindow(parent);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
