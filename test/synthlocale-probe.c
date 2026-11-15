/* synthlocale-probe.c: GetLocaleInfoEx for a well-formed locale name it has no data of its own
 * for (wine-sg 7785). Windows 10 answers as for a "custom unspecified" locale (LCID 0x1000):
 * the name in its canonical case, the data of its language. .NET 5 and later on Windows take
 * an ICU culture only when GetLocaleInfoEx(name, LOCALE_SNAME) succeeds: new CultureInfo("sr-SP")
 * threw CultureNotFoundException (DevToys 2 died at its start). Prints NAME=<result>. */
#include <windows.h>
#include <stdio.h>

static void q(const WCHAR *name)
{
    WCHAR buf[128], lang[128];
    DWORD lcid = 0;
    int ret;
    SetLastError(0xdeadbeef);
    ret = GetLocaleInfoEx(name, LOCALE_SNAME, buf, ARRAYSIZE(buf));
    if (!ret) { printf("%ls=fail %lu\n", name, GetLastError()); return; }
    GetLocaleInfoEx(name, LOCALE_ILANGUAGE | LOCALE_RETURN_NUMBER, (WCHAR *)&lcid, sizeof(lcid) / sizeof(WCHAR));
    if (!GetLocaleInfoEx(name, LOCALE_SENGLISHLANGUAGENAME, lang, ARRAYSIZE(lang))) lstrcpyW(lang, L"?");
    printf("%ls=%ls len=%d lcid=%04lx lang=%ls\n", name, buf, ret, lcid, lang);
}

int main(void)
{
    WCHAR small[3];
    static const WCHAR *names[] = { L"en-US", L"sr-SP", L"xx-YY", L"en-XX", L"SR-sp", L"zh-hans-xx", L"de-419",
                                    L"dummy", L"e", L"en-", L"en-US-", L"1a-BB", L"en-ABCD-EF-GH", L"en_XX" };
    int i, ret;
    for (i = 0; i < ARRAYSIZE(names); i++) q(names[i]);
    SetLastError(0xdeadbeef);
    ret = GetLocaleInfoEx(L"sr-SP", LOCALE_SNAME, small, ARRAYSIZE(small));
    printf("short=%d %lu\n", ret, GetLastError());
    printf("size=%d\n", GetLocaleInfoEx(L"sr-SP", LOCALE_SNAME, NULL, 0));
    return 0;
}
