/* hhctrl batch (patches/sg/3010), run by test/fasthhctrl-gate.sh: HtmlHelp commands that need no help file. */
#include <windows.h>
#include <stdio.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECK(e) check((e), #e)

typedef HWND (WINAPI *HtmlHelpA_t)(HWND, const char *, UINT, DWORD_PTR);

#define HH_GET_WIN_HANDLE 0x06
#define HH_KEYWORD_LOOKUP 0x0D
#define HH_PRETRANSLATEMESSAGE 0x20
#define HH_INITIALIZE 0x1C
#define HH_UNINITIALIZE 0x1D

typedef struct
{
    int cbStruct;
    BOOL fReserved;
    const char *pszKeywords, *pszUrl, *pszMsgText, *pszMsgTitle, *pszWindow;
    BOOL fIndexOnFail;
} ALINKA;

int main(void)
{
    HMODULE mod = LoadLibraryA("hhctrl.ocx");
    HtmlHelpA_t pHtmlHelpA;
    DWORD cookie = 0;
    ALINKA link = { sizeof(link) };

    CHECK(mod != NULL);
    if (!mod) return 1;
    pHtmlHelpA = (HtmlHelpA_t)GetProcAddress(mod, "HtmlHelpA");
    CHECK(pHtmlHelpA != NULL);
    if (!pHtmlHelpA) return 1;

    pHtmlHelpA(NULL, NULL, HH_INITIALIZE, (DWORD_PTR)&cookie);
    CHECK(cookie != 0);
    pHtmlHelpA(NULL, NULL, HH_UNINITIALIZE, cookie);

    CHECK(pHtmlHelpA(NULL, NULL, HH_GET_WIN_HANDLE, (DWORD_PTR)"NoSuchWindow") == NULL);

    /* no help window exists: nothing translates the message */
    {
        MSG msg = { 0 };
        CHECK(pHtmlHelpA(NULL, NULL, HH_PRETRANSLATEMESSAGE, (DWORD_PTR)&msg) == NULL);
        CHECK(pHtmlHelpA(NULL, NULL, HH_PRETRANSLATEMESSAGE, 0) == NULL);
    }

    link.pszKeywords = "nothing";
    CHECK(pHtmlHelpA(NULL, "C:\\no\\such\\file.chm", HH_KEYWORD_LOOKUP, (DWORD_PTR)&link) == NULL);

    printf("%d failure(s)\n", failures);
    /* the gate looks for the RESULT line (it was never printed: the gate could not pass) */
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
