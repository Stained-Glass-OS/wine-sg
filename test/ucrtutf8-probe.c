/* ucrtutf8 gate probe (wine-sg 8406): the UCRT's setlocale(LC_ALL, "") in a program whose manifest
 * asks for UTF-8 as its code page (activeCodePage), and in one that does not. Built twice, with and
 * without that manifest; prints "acp=N cp=N name=..." of ucrtbase's setlocale and code page. */
#include <windows.h>
#include <stdio.h>

int main(void)
{
    HMODULE ucrt = LoadLibraryA("ucrtbase.dll");
    char *(__cdecl *p_setlocale)(int, const char *) = (void *)GetProcAddress(ucrt, "setlocale");
    unsigned int (__cdecl *p_cp)(void) = (void *)GetProcAddress(ucrt, "___lc_codepage_func");
    const char *name;

    if (!p_setlocale || !p_cp) { printf("acp=%u cp=0 name=(no ucrtbase)\n", GetACP()); return 0; }
    name = p_setlocale(0 /* LC_ALL */, "");
    printf("acp=%u cp=%u name=%s\n", GetACP(), p_cp(), name ? name : "(null)");
    /* an explicit code page still wins */
    name = p_setlocale(0, ".1252");
    printf("explicit=%u\n", p_cp());
    return 0;
}
