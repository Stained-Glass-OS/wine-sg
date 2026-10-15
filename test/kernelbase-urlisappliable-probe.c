/* Probe for patches/sg/5043: UrlIs(URLIS_APPLIABLE) - "Attempt to determine a
 * valid scheme for the URL. If the function is able to determine a scheme, it
 * returns TRUE, or FALSE otherwise": a URL with a known scheme, or one whose
 * scheme can be guessed (www., ftp., a file path); A and W alike. It always
 * returned FALSE. Table driven; lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include <windows.h>
#include <shlwapi.h>
#include <stdio.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

int main(void)
{
    static const struct { const char *url; BOOL want; } tests[] =
    {
        {"http://www.winehq.org/", TRUE}, {"mailto:someone@example.com", TRUE}, {"file:///c:/windows", TRUE},
        {"www.winehq.org", TRUE}, {"ftp.winehq.org", TRUE}, {"c:\\windows\\win.ini", TRUE}, {"\\\\server\\share", TRUE},
        {"nosuchscheme", FALSE}, {"", FALSE},
    };
    unsigned int i;

    for (i = 0; i < ARRAYSIZE(tests); i++)
    {
        WCHAR urlW[256];
        BOOL a, w;

        MultiByteToWideChar(CP_ACP, 0, tests[i].url, -1, urlW, ARRAYSIZE(urlW));
        w = UrlIsW(urlW, URLIS_APPLIABLE);
        a = UrlIsA(tests[i].url, URLIS_APPLIABLE);
        check(w == tests[i].want && a == tests[i].want, "\"%s\": W %d A %d (want %d)", tests[i].url, w, a, tests[i].want);
    }
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
