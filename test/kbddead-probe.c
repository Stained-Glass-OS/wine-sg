/* kbddead-probe: a dead key composes with the next character (wine-sg 8204), for test/kbddead-gate.sh.
 * French and German layouts (not the X server's): ToUnicodeEx and TranslateMessage. Prints KEY=VALUE.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <stdio.h>

static HKL fr, de;

/* one key down through ToUnicodeEx: "ret chars" (chars as hex) */
static void key(const char *tag, HKL hkl, UINT vk, UINT scan, UINT flags)
{
    BYTE state[256] = {0};
    WCHAR buf[8] = {0};
    int r = ToUnicodeEx(vk, scan, state, buf, 8, flags, hkl), i;
    printf("%s=%d", tag, r);
    for (i = 0; i < (r < 0 ? 1 : r); i++) printf(" %04x", buf[i]);
    printf("\n");
}

static void translate(HWND hwnd, UINT vk, UINT scan)
{
    MSG msg = { hwnd, WM_KEYDOWN, vk, 1 | (scan << 16) };
    TranslateMessage(&msg);
}

int main(void)
{
    HWND hwnd;
    MSG msg;
    char seen[128] = "";

    if (!(fr = LoadKeyboardLayoutA("0000040c", 0)) || !(de = LoadKeyboardLayoutA("00000407", 0)))
    { printf("layouts=0\n"); return 1; }
    printf("layouts=1\n");
    /* French: ^ (VK_OEM_6, scan 0x1a) is dead */
    key("fr.dead", fr, VK_OEM_6, 0x1a, 0);
    key("fr.e", fr, 'E', 0x12, 0);           /* e with ^: U+00EA */
    key("fr.dead2", fr, VK_OEM_6, 0x1a, 0);
    key("fr.x", fr, 'X', 0x2d, 0);           /* no x with ^: both */
    key("fr.dead3", fr, VK_OEM_6, 0x1a, 0);
    key("fr.space", fr, VK_SPACE, 0x39, 0);  /* the accent itself */
    key("fr.dead4", fr, VK_OEM_6, 0x1a, 4);  /* flag 4: the state is kept as it is */
    key("fr.e4", fr, 'E', 0x12, 0);
    key("fr.plain", fr, 'E', 0x12, 0);       /* nothing waiting: e */
    /* German: the key right of 0 (scan 0x0d) is the dead acute */
    key("de.dead", de, VK_OEM_6, 0x0d, 0);
    key("de.a", de, 'A', 0x1e, 0);           /* U+00E1 */

    /* TranslateMessage with French active: WM_DEADCHAR ^, then WM_CHAR e-circumflex */
    hwnd = CreateWindowA("static", "kbddead", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, NULL, NULL, NULL, NULL);
    ActivateKeyboardLayout(fr, 0);
    translate(hwnd, VK_OEM_6, 0x1a);
    translate(hwnd, 'E', 0x12);
    while (PeekMessageA(&msg, hwnd, 0, 0, PM_REMOVE))
    {
        if (msg.message == WM_DEADCHAR || msg.message == WM_CHAR)
            sprintf(seen + strlen(seen), "%s%s %04x", seen[0] ? "," : "", msg.message == WM_DEADCHAR ? "dead" : "char",
                    (unsigned)msg.wParam);
    }
    printf("translate=%s\n", seen);
    printf("done=1\n");
    return 0;
}
