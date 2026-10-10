/* shell32 batch (patches/sg/2024), run by test/shexecargs-gate.sh. How
 * ShellExecute splits the parameters it is given and fills the %2 ... %9, %*
 * and %~n directives of a verb's command (from Wine's argify tests, which
 * record Windows). A verb is registered under HKCR for a made-up extension;
 * its command runs this program again to write its command line to a file.
 *
 *   shexecargs-probe.exe */
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <string.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static char exe[MAX_PATH], outfile[MAX_PATH], srcfile[MAX_PATH];

static void set_command(const char *verb, const char *fmt)
{
    char key[200], cmd[1024];
    HKEY k;
    snprintf(key, sizeof(key), "sgargfile\\shell\\%s\\command", verb);
    snprintf(cmd, sizeof(cmd), "\"%s\" --child \"%s\" %s %s", exe, outfile, verb, fmt);
    if (!RegCreateKeyExA(HKEY_CLASSES_ROOT, key, 0, NULL, 0, KEY_WRITE, NULL, &k, NULL))
    {
        RegSetValueExA(k, NULL, 0, REG_SZ, (const BYTE *)cmd, strlen(cmd) + 1);
        RegCloseKey(k);
    }
}

/* the command line the child saw, after "<verb> " */
static int run(const char *verb, const char *params, char *got, size_t size)
{
    SHELLEXECUTEINFOA sei;
    char line[2048];
    HANDLE h;
    DWORD n = 0;
    size_t vl = strlen(verb);
    char *p;

    DeleteFileA(outfile);
    memset(&sei, 0, sizeof(sei));
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI;
    sei.lpVerb = verb;
    sei.lpFile = srcfile;
    sei.lpParameters = params;
    sei.nShow = SW_HIDE;
    if (!ShellExecuteExA(&sei)) { snprintf(got, size, "(ShellExecuteEx failed %lu)", GetLastError()); return 0; }
    if (sei.hProcess) { WaitForSingleObject(sei.hProcess, 20000); CloseHandle(sei.hProcess); }
    h = CreateFileA(outfile, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) { snprintf(got, size, "(no output)"); return 0; }
    ReadFile(h, line, sizeof(line) - 1, &n, NULL);
    CloseHandle(h);
    line[n] = 0;
    p = strstr(line, verb);
    if (!p) { snprintf(got, size, "(verb not found: %s)", line); return 0; }
    snprintf(got, size, "%s", p + vl);
    return 1;
}

static const struct { const char *verb, *fmt, *params, *expected; } cases[] =
{
    { "Raw", "%*", "p2 p3 \"p4 ", " p2 p3 \"p4 " },
    { "Dup", "%2 %3 \"%2\" \"%*\"", "p2 p3 p4 ", " p2 p3 \"p2\" \"p2 p3 p4 \"" },
    { "Dup", "%2 %3 \"%2\" \"%*\"", "\"p two\" p3 p4  ", " p two p3 \"p two\" \"\"p two\" p3 p4  \"" },
    { "Single", "\"%20\"", "p", " \"p0\"" },
    { "Five", "\"%2\" \"%3\" \"%4\" \"%5\" \"%6\"", "'p2 p3` p4\\ $even", " \"'p2\" \"p3`\" \"p4\\\" \"$even\" \"\"" },
    { "Five", "\"%2\" \"%3\" \"%4\" \"%5\" \"%6\"", "p=2 p-3 p4\tp4\rp4\np4", " \"p=2\" \"p-3\" \"p4\tp4\rp4\np4\" \"\" \"\"" },
    { "Eight", "\"%2\" \"%3\" \"%4\" \"%5\" \"%6\" \"%7\" \"%8\" \"%9\"", "one\"quote \"p four\" one\"quote p7",
      " \"one\" \"quote\" \"p four\" \"one\" \"quote\" \"p7\" \"\" \"\"" },
    { "Eight", "\"%2\" \"%3\" \"%4\" \"%5\" \"%6\" \"%7\" \"%8\" \"%9\"", "two\"\"quotes \"p three\" two\"\"quotes p5",
      " \"two\"quotes\" \"p three\" \"two\"quotes\" \"p5\" \"\" \"\" \"\" \"\"" },
    { "Eight", "\"%2\" \"%3\" \"%4\" \"%5\" \"%6\" \"%7\" \"%8\" \"%9\"", "three\"\"\"quotes \"p four\" three\"\"\"quotes p6",
      " \"three\"\" \"quotes\" \"p four\" \"three\"\" \"quotes\" \"p6\" \"\" \"\"" },
    { "Five", "\"%2\" \"%3\" \"%4\" \"%5\" \"%6\"", "\"p two\"p3 \"p four\"p5 p6", " \"p two\" \"p3\" \"p four\" \"p5\" \"p6\"" },
    { "Eight", "\"%2\" \"%3\" \"%4\" \"%5\" \"%6\" \"%7\" \"%8\" \"%9\"", "\"one q\"uote \"p four\" \"one q\"uote p7",
      " \"one q\" \"uote\" \"p four\" \"one q\" \"uote\" \"p7\" \"\" \"\"" },
    { "Eight", "\"%2\" \"%3\" \"%4\" \"%5\" \"%6\" \"%7\" \"%8\" \"%9\"", "\"two \"\" quotes\" \"p three\" \"two \"\" quotes\" p5",
      " \"two \" quotes\" \"p three\" \"two \" quotes\" \"p5\" \"\" \"\" \"\" \"\"" },
    { "Eight", "\"%2\" \"%3\" \"%4\" \"%5\" \"%6\" \"%7\" \"%8\" \"%9\"", "\"\"twoquotes \"p four\" \"\"twoquotes p7",
      " \"\" \"twoquotes\" \"p four\" \"\" \"twoquotes\" \"p7\" \"\" \"\"" },
    { "Eight", "\"%2\" \"%3\" \"%4\" \"%5\" \"%6\" \"%7\" \"%8\" \"%9\"", "\"\"\"three quotes\" \"p three\" \"\"\"three quotes\" p5",
      " \"\"three quotes\" \"p three\" \"\"three quotes\" \"p5\" \"\" \"\" \"\" \"\"" },
    { "Five", "\"%2\" \"%3\" \"%4\" \"%5\" \"%6\"", "p2 \"p3\" \"p4 is lost", " \"p2\" \"p3\" \"\" \"\" \"\"" },
    { "Five", "\"%2\" \"%3\" \"%4\" \"%5\" \"%6\"", "\\\"p\\three \"pfour\\\" pfive", " \"\\\" \"p\\three\" \"pfour\\\" \"pfive\" \"\"" },
    { "Tilde", "~2=\"%~2\" ~3=\"%~3\" ~4=\"%~4\" ~5=%~5", "p2  p3 \"p4\"  p5 p6 ",
      " ~2=\"p2  p3 \"p4\"  p5 p6 \" ~3=\"  p3 \"p4\"  p5 p6 \" ~4=\" \"p4\"  p5 p6 \" ~5=  p5 p6 " },
    { "Tilde9", "~9=\"%~9\"", "p2 p3 p4 p5 p6 p7 p8   ", " ~9=\"   \"" },
    { "Tilde9", "~9=\"%~9\"", "p2 p3 p4 p5 p6 p7   ", " ~9=\"\"" },
    { "Tilde9", "~9=\"%~9\"", "p2 p3 p4 p5 p6 p7 p8 p9 p10 p11 and beyond!", " ~9=\" p9 p10 p11 and beyond!\"" },
    { "Env", "\"%a %b\"", "p2", " \"a b\"" },
};

int main(int argc, char **argv)
{
    unsigned i;
    char got[2048], what[200];
    HKEY k;

    if (argc >= 3 && !strcmp(argv[1], "--child"))
    {
        HANDLE h = CreateFileA(argv[2], GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
        DWORD n;
        const char *cl = GetCommandLineA();
        WriteFile(h, cl, strlen(cl), &n, NULL);
        CloseHandle(h);
        return 0;
    }

    GetModuleFileNameA(NULL, exe, sizeof(exe));
    GetTempPathA(sizeof(outfile), outfile);
    strcpy(srcfile, outfile);
    strcat(outfile, "sg-shexecargs-out.txt");
    strcat(srcfile, "sg-shexecargs.sgarg");
    {
        HANDLE h = CreateFileA(srcfile, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
        CloseHandle(h);
    }
    if (!RegCreateKeyExA(HKEY_CLASSES_ROOT, ".sgarg", 0, NULL, 0, KEY_WRITE, NULL, &k, NULL))
    {
        RegSetValueExA(k, NULL, 0, REG_SZ, (const BYTE *)"sgargfile", 10);
        RegCloseKey(k);
    }

    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        set_command(cases[i].verb, cases[i].fmt);
        run(cases[i].verb, cases[i].params, got, sizeof(got));
        snprintf(what, sizeof(what), "case %u: %s", i, cases[i].verb);
        if (strcmp(got, cases[i].expected)) printf("   expected [%s]\n        got [%s]\n", cases[i].expected, got);
        check(!strcmp(got, cases[i].expected), what);
    }

    /* a file path with spaces and a URL with & and % as parameters, through
     * the open verb of a .txt association (the previous association is put back) */
    {
        static const char *url = "http://h.example/p?a=1&b=%41%20c&d=100%";
        char dir[MAX_PATH], txt[MAX_PATH], cmd[1024], old[200] = "", want[2048];
        DWORD oldsz = sizeof(old), type;
        int had = !RegQueryValueExA(HKEY_CLASSES_ROOT, ".txt", NULL, &type, (BYTE *)old, &oldsz);
        HANDLE h;
        strcpy(dir, outfile); strcpy(strrchr(dir, '\\') + 1, "sg shexec dir");
        CreateDirectoryA(dir, NULL);
        snprintf(txt, sizeof(txt), "%s\\a b & c.txt", dir);
        h = CreateFileA(txt, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL); CloseHandle(h);
        if (!RegCreateKeyExA(HKEY_CLASSES_ROOT, ".txt", 0, NULL, 0, KEY_WRITE, NULL, &k, NULL))
        {
            RegSetValueExA(k, NULL, 0, REG_SZ, (const BYTE *)"sgtxtfile", 10);
            RegCloseKey(k);
        }
        snprintf(cmd, sizeof(cmd), "\"%s\" --child \"%s\" open \"%%1\" %%*", exe, outfile);
        if (!RegCreateKeyExA(HKEY_CLASSES_ROOT, "sgtxtfile\\shell\\open\\command", 0, NULL, 0, KEY_WRITE, NULL, &k, NULL))
        {
            RegSetValueExA(k, NULL, 0, REG_SZ, (const BYTE *)cmd, strlen(cmd) + 1);
            RegCloseKey(k);
        }
        strcpy(srcfile, txt);
        snprintf(want, sizeof(want), " \"%s\" %s", txt, url);
        run("open", url, got, sizeof(got));
        if (strcmp(got, want)) printf("   expected [%s]\n        got [%s]\n", want, got);
        check(!strcmp(got, want), "txt open: path with spaces and & plus URL with & and % parameters");
        snprintf(want, sizeof(want), " \"%s\" ", txt);
        run("open", NULL, got, sizeof(got));
        if (strcmp(got, want)) printf("   expected [%s]\n        got [%s]\n", want, got);
        check(!strcmp(got, want), "txt open: no parameters");
        run("open", "\"x y\" &z", got, sizeof(got));
        snprintf(want, sizeof(want), " \"%s\" \"x y\" &z", txt);
        if (strcmp(got, want)) printf("   expected [%s]\n        got [%s]\n", want, got);
        check(!strcmp(got, want), "txt open: quoted parameter and &");
        DeleteFileA(txt); RemoveDirectoryA(dir);
        RegDeleteTreeA(HKEY_CLASSES_ROOT, "sgtxtfile");
        if (had) RegSetValueExA(HKEY_CLASSES_ROOT, ".txt", 0, REG_SZ, (const BYTE *)old, oldsz); /* best effort */
        else RegDeleteTreeA(HKEY_CLASSES_ROOT, ".txt");
    }

    DeleteFileA(srcfile);
    DeleteFileA(outfile);
    RegDeleteTreeA(HKEY_CLASSES_ROOT, "sgargfile");
    RegDeleteTreeA(HKEY_CLASSES_ROOT, ".sgarg");
    printf("%d checks failed\n", failures);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
