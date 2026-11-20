/* elevargs gate probe (wine-sg 8400): an elevated program's arguments as the caller wrote them.
 *   elevargs-probe.exe runas FILE N   ShellExecuteEx "runas" FILE with case N's parameters as
 *                                     lpParameters; wait, print "RUNAS code X"
 *   elevargs-probe.exe case N         print case N's parameters, between [ ]
 *   elevargs-probe.exe show ...       (the elevated side) append the arguments part of its own
 *                                     command line, between [ ], to C:\users\Public\elevargs.txt
 * A command line typed in a Unix shell is written again by Wine's loader, so the cases live
 * here. */
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const WCHAR *cases[] =
{
    /* 0: advpack's uninstall line (Notepad2): a quoted path inside an argument */
    L"show advpack.dll,LaunchINFSectionEx \"C:\\Program Files\\Notepad2\\Notepad2.inf\",DefaultUninstall,,8,N",
    /* 1: an MSI property with a quoted value */
    L"show /i x.msi INSTALLDIR=\"C:\\Program Files\\X Y\" /qn",
    /* 2: plain arguments and a quoted path: the loader writes them so anyway */
    L"show /S /D=C:\\Program Files\\X \"C:\\a b\\c.txt\"",
    /* 3: NSIS's /UAC: is left out, the rest as written */
    L"show /UAC:1a2b /NCRC _?=\"C:\\P Q\"",
    /* 4: blanks between arguments as written, and an escaped quote */
    L"show a  b\t\\\"c\\\" \"d e\"f",
    /* 5: no arguments after the program's own word */
    L"show",
};
static const WCHAR *expected[] =
{
    L"advpack.dll,LaunchINFSectionEx \"C:\\Program Files\\Notepad2\\Notepad2.inf\",DefaultUninstall,,8,N",
    L"/i x.msi INSTALLDIR=\"C:\\Program Files\\X Y\" /qn",
    L"/S /D=C:\\Program Files\\X \"C:\\a b\\c.txt\"",
    L"/NCRC _?=\"C:\\P Q\"",
    L"a  b\t\\\"c\\\" \"d e\"f",
    L"",
};

/* past the first argument of s (CommandLineToArgvW's rule for the program) */
static const WCHAR *skip_arg0(const WCHAR *s)
{
    if (*s == '"') { for (s++; *s; ) if (*s++ == '"') break; }
    else while (*s && *s != ' ' && *s != '\t') s++;
    return s;
}

static void put(FILE *f, const WCHAR *s)
{
    fputc('[', f);
    for (; *s; s++) fputc(*s == '\t' ? '~' : *s < 0x80 ? (char)*s : '?', f);
    fputs("]\n", f);
}

int wmain(int argc, WCHAR **argv)
{
    if (argc > 1 && !wcscmp(argv[1], L"show"))
    {
        const WCHAR *p = skip_arg0(GetCommandLineW());
        FILE *f = _wfopen(L"C:\\users\\Public\\elevargs.txt", L"ab");

        if (!f) return 3;
        /* " show", then the one blank the caller put after it */
        while (*p == ' ' || *p == '\t') p++;
        p += 4;
        if (*p == ' ') p++;
        put(f, p);
        fclose(f);
        return 0;
    }
    if (argc > 2 && !wcscmp(argv[1], L"case"))
    {
        int n = _wtoi(argv[2]);
        if (n < 0 || n >= ARRAYSIZE(expected)) return 2;
        put(stdout, expected[n]);
        return 0;
    }
    if (argc > 3 && !wcscmp(argv[1], L"runas"))
    {
        SHELLEXECUTEINFOW sei = { sizeof(sei) };
        DWORD code = 0xdeadbeef;
        int n = _wtoi(argv[3]);

        if (n < 0 || n >= ARRAYSIZE(cases)) return 2;
        sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC;
        sei.lpVerb = L"runas";
        sei.lpFile = argv[2];
        sei.lpParameters = cases[n];
        sei.nShow = SW_SHOWNORMAL;
        if (!ShellExecuteExW(&sei)) { printf("RUNAS failed %lu\n", GetLastError()); return 1; }
        if (sei.hProcess)
        {
            WaitForSingleObject(sei.hProcess, 120000);
            GetExitCodeProcess(sei.hProcess, &code);
        }
        printf("RUNAS code %lu\n", code);
        return 0;
    }
    return 2;
}
