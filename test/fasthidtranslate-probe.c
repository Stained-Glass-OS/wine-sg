/* HidP_TranslateUsagesToI8042ScanCodes (patch 3301), run by test/fasthidtranslate-gate.sh */
#include <windows.h>
#include <stdio.h>
#include <string.h>

typedef LONG NTSTATUS_;
typedef enum { Break, Make } DIR_;
typedef BOOLEAN (*INSERT)(void *ctx, char *codes, ULONG len);
typedef LONG (WINAPI *TRANSLATE)(USHORT *list, ULONG len, int dir, ULONG *state, INSERT insert, void *ctx);

#define STATUS_OK ((LONG)0x00110000)
#define STATUS_UNKNOWN ((LONG)0xC0110009)
#define STATUS_NULLPTR ((LONG)0x80110001)
#define STATUS_SMALL ((LONG)0xC0110007)

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECKF(ok, ...) do { char _b[300]; snprintf(_b, sizeof(_b), __VA_ARGS__); check(ok, _b); } while (0)

struct ctx { unsigned char bytes[64]; int len; int calls; int max_call; int fail_after; };

static BOOLEAN insert(void *p, char *codes, ULONG len)
{
    struct ctx *c = p;
    if (c->fail_after >= 0 && c->calls >= c->fail_after) return FALSE;
    c->calls++;
    if ((int)len > c->max_call) c->max_call = len;
    memcpy(c->bytes + c->len, codes, len);
    c->len += len;
    return TRUE;
}

static TRANSLATE translate;

static LONG run(USHORT *list, ULONG n, int dir, ULONG *state, struct ctx *c)
{
    memset(c, 0, sizeof(*c));
    c->fail_after = -1;
    return translate(list, n, dir, state, insert, c);
}

static int is(struct ctx *c, const char *hex)
{
    char buf[200] = "";
    int i;
    for (i = 0; i < c->len; ++i) sprintf(buf + strlen(buf), "%02x", c->bytes[i]);
    return !strcmp(buf, hex);
}

static void key(USHORT usage, const char *make, const char *brk, const char *name)
{
    struct ctx c;
    ULONG state = 0;
    USHORT list[2] = { usage, 0 };
    LONG st = run(list, 2, Make, &state, &c);
    CHECKF(st == STATUS_OK && is(&c, make), "%s make is %s (st %#lx, %d bytes)", name, make, st, c.len);
    st = run(list, 2, Break, &state, &c);
    CHECKF(st == STATUS_OK && is(&c, brk), "%s break is %s (st %#lx)", name, brk, st);
}

int main(void)
{
    HMODULE hid = LoadLibraryA("hid.dll");
    struct ctx c;
    ULONG state = 0;
    USHORT list[8];
    LONG st;

    translate = (TRANSLATE)GetProcAddress(hid, "HidP_TranslateUsagesToI8042ScanCodes");
    if (!translate) { check(0, "hid exports HidP_TranslateUsagesToI8042ScanCodes"); return 1; }

    key(0x04, "1e", "9e", "A");
    key(0x1d, "2c", "ac", "Z");
    key(0x1e, "02", "82", "1");
    key(0x27, "0b", "8b", "0");
    key(0x28, "1c", "9c", "Enter");
    key(0x29, "01", "81", "Escape");
    key(0x2c, "39", "b9", "Space");
    key(0x35, "29", "a9", "Grave");
    key(0x39, "3a", "ba", "Caps Lock");
    key(0x3a, "3b", "bb", "F1");
    key(0x44, "57", "d7", "F11");
    key(0x45, "58", "d8", "F12");
    key(0x47, "46", "c6", "Scroll Lock");
    key(0x49, "e052", "e0d2", "Insert");
    key(0x4a, "e047", "e0c7", "Home");
    key(0x4c, "e053", "e0d3", "Delete");
    key(0x4f, "e04d", "e0cd", "Right");
    key(0x52, "e048", "e0c8", "Up");
    key(0x53, "45", "c5", "Num Lock");
    key(0x54, "e035", "e0b5", "Keypad divide");
    key(0x58, "e01c", "e09c", "Keypad enter");
    key(0x59, "4f", "cf", "Keypad 1");
    key(0x62, "52", "d2", "Keypad 0");
    key(0x64, "56", "d6", "Non-US backslash");
    key(0x65, "e05d", "e0dd", "Application");
    key(0x68, "64", "e4", "F13");
    key(0x73, "76", "f6", "F24");
    key(0x87, "73", "f3", "International 1");
    key(0xe0, "1d", "9d", "Left Control");
    key(0xe1, "2a", "aa", "Left Shift");
    key(0xe3, "e05b", "e0db", "Left GUI");
    key(0xe4, "e01d", "e09d", "Right Control");
    key(0xe5, "36", "b6", "Right Shift");
    key(0xe6, "e038", "e0b8", "Right Alt");
    key(0xe7, "e05c", "e0dc", "Right GUI");

    /* print screen: four bytes in one call; pause: only a make, six bytes */
    list[0] = 0x46; list[1] = 0;
    st = run(list, 2, Make, &state, &c);
    CHECKF(st == STATUS_OK && is(&c, "e02ae037") && c.max_call <= 4, "Print Screen make e02ae037 (%d calls)", c.calls);
    st = run(list, 2, Break, &state, &c);
    CHECKF(st == STATUS_OK && is(&c, "e0b7e0aa"), "Print Screen break e0b7e0aa");
    list[0] = 0x48;
    st = run(list, 2, Make, &state, &c);
    CHECKF(st == STATUS_OK && is(&c, "e11d45e19dc5") && c.max_call <= 4, "Pause make e11d45e19dc5 in pieces of at most four bytes");
    st = run(list, 2, Break, &state, &c);
    CHECKF(st == STATUS_OK && c.len == 0 && c.calls == 0, "Pause has no break code");

    /* modifier state */
    state = 0;
    list[0] = 0xe1; list[1] = 0xe4; list[2] = 0x39; list[3] = 0;
    run(list, 4, Make, &state, &c);
    CHECKF(state == ((1u << 1) | (1u << 4) | (1u << 8)), "left shift, right control and caps lock are set (%#lx)", state);
    CHECKF(is(&c, "2ae01d3a"), "the codes of the three, in order");
    list[0] = 0xe1; list[1] = 0;
    run(list, 2, Break, &state, &c);
    CHECKF(state == ((1u << 4) | (1u << 8)), "left shift is cleared by its break (%#lx)", state);
    list[0] = 0xe0; list[1] = 0xe2; list[2] = 0xe3; list[3] = 0xe5; list[4] = 0xe6; list[5] = 0xe7; list[6] = 0x47; list[7] = 0x53;
    state = 0;
    run(list, 8, Make, &state, &c);
    CHECKF(state == (0x1u | 0x4 | 0x8 | 0x20 | 0x40 | 0x80 | (1u << 9) | (1u << 10)), "every modifier and lock bit (%#lx)", state);

    /* the list: zero ends it, the length bounds it */
    list[0] = 0x04; list[1] = 0; list[2] = 0x05; list[3] = 0;
    st = run(list, 4, Make, &state, &c);
    CHECKF(st == STATUS_OK && is(&c, "1e"), "a zero ends the list");
    list[0] = 0x04; list[1] = 0x05; list[2] = 0x06; list[3] = 0;
    st = run(list, 2, Make, &state, &c);
    CHECKF(st == STATUS_OK && is(&c, "1e30"), "the length bounds the list");
    st = run(list, 0, Make, &state, &c);
    CHECKF(st == STATUS_OK && c.len == 0, "an empty list");

    /* errors */
    list[0] = 0x04; list[1] = 0x03; list[2] = 0x05; list[3] = 0;
    st = run(list, 4, Make, &state, &c);
    CHECKF(st == STATUS_UNKNOWN && is(&c, "1e"), "an unknown usage stops the list: earlier ones were sent (%#lx)", st);
    list[0] = 0xe8; list[1] = 0;
    st = run(list, 2, Make, &state, &c);
    CHECKF(st == STATUS_UNKNOWN && c.len == 0, "usage 0xe8 has no scan code (%#lx)", st);
    memset(&c, 0, sizeof(c)); c.fail_after = 1;
    list[0] = 0x04; list[1] = 0x05; list[2] = 0x06; list[3] = 0;
    st = translate(list, 4, Make, &state, insert, &c);
    CHECKF(st == STATUS_SMALL && is(&c, "1e"), "an insert routine that refuses stops the translation (%#lx)", st);
    st = translate(NULL, 1, Make, &state, insert, &c);
    CHECKF(st == STATUS_NULLPTR, "a NULL list (%#lx)", st);
    st = translate(list, 1, Make, &state, NULL, &c);
    CHECKF(st == STATUS_NULLPTR, "a NULL insert routine (%#lx)", st);
    list[0] = 0x04; list[1] = 0;
    memset(&c, 0, sizeof(c)); c.fail_after = -1;
    st = translate(list, 2, Make, NULL, insert, &c);
    CHECKF(st == STATUS_OK && is(&c, "1e"), "a NULL modifier state is allowed (%#lx)", st);

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
