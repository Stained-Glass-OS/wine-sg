/* runas of one's own prefix (wine-sg 7745): ShellExecuteEx "runas" from the prefix owner's filtered
 * token starts the program elevated, with the linked token, as upstream Wine does. 7740 refused any
 * linked token below impersonation level, which upstream Wine's is too, so with no consent broker
 * runas started the program unelevated -- and msiexec, which relaunches itself with runas for an
 * all-users package, started itself without end.
 *   runasown-probe.exe             parent: runas itself as "child", prints the child's answer
 *   runasown-probe.exe child FILE  child: writes "elevated=N" to FILE
 *   runasown-probe.exe makemsi MSI  writes an all-users package whose custom action runs
 *                                   "cmd /c set > C:\t\caenv.txt" (what a custom action inherits) */
#include <windows.h>
#include <shellapi.h>
#include <msi.h>
#include <msiquery.h>
#include <stdio.h>

static UINT run_sql(MSIHANDLE db, const WCHAR *sql)
{
    MSIHANDLE view;
    UINT r = MsiDatabaseOpenViewW(db, sql, &view);
    if (r) return r;
    r = MsiViewExecute(view, 0);
    MsiViewClose(view);
    MsiCloseHandle(view);
    return r;
}

static int make_msi(const WCHAR *path)
{
    static const WCHAR *sql[] =
    {
        L"CREATE TABLE `Property` (`Property` CHAR(72) NOT NULL, `Value` CHAR(0) NOT NULL PRIMARY KEY `Property`)",
        L"CREATE TABLE `InstallExecuteSequence` (`Action` CHAR(72) NOT NULL, `Condition` CHAR(255), `Sequence` SHORT PRIMARY KEY `Action`)",
        L"CREATE TABLE `CustomAction` (`Action` CHAR(72) NOT NULL, `Type` SHORT NOT NULL, `Source` CHAR(72), `Target` CHAR(255) PRIMARY KEY `Action`)",
        L"INSERT INTO `Property` (`Property`, `Value`) VALUES ('ProductCode', '{6A5F2C31-7E0B-4B8E-9C1D-2F4A8B6C0D17}')",
        L"INSERT INTO `Property` (`Property`, `Value`) VALUES ('ProductName', 'SG runasown')",
        L"INSERT INTO `Property` (`Property`, `Value`) VALUES ('ProductVersion', '1.0.0')",
        L"INSERT INTO `Property` (`Property`, `Value`) VALUES ('ProductLanguage', '1033')",
        L"INSERT INTO `Property` (`Property`, `Value`) VALUES ('Manufacturer', 'Stained Glass OS gate')",
        L"INSERT INTO `Property` (`Property`, `Value`) VALUES ('ALLUSERS', '1')",
        L"INSERT INTO `Property` (`Property`, `Value`) VALUES ('CMDEXE', 'C:\\windows\\system32\\cmd.exe')",
        L"INSERT INTO `CustomAction` (`Action`, `Type`, `Source`, `Target`) VALUES ('ShowEnv', 50, 'CMDEXE', '/c set > C:\\t\\caenv.txt')",
        L"INSERT INTO `InstallExecuteSequence` (`Action`, `Sequence`) VALUES ('CostInitialize', 800)",
        L"INSERT INTO `InstallExecuteSequence` (`Action`, `Sequence`) VALUES ('FileCost', 900)",
        L"INSERT INTO `InstallExecuteSequence` (`Action`, `Sequence`) VALUES ('CostFinalize', 1000)",
        L"INSERT INTO `InstallExecuteSequence` (`Action`, `Sequence`) VALUES ('InstallValidate', 1400)",
        L"INSERT INTO `InstallExecuteSequence` (`Action`, `Sequence`) VALUES ('InstallInitialize', 1500)",
        L"INSERT INTO `InstallExecuteSequence` (`Action`, `Sequence`) VALUES ('ShowEnv', 1600)",
        L"INSERT INTO `InstallExecuteSequence` (`Action`, `Sequence`) VALUES ('InstallFinalize', 6600)",
    };
    MSIHANDLE db, si;
    UINT r, i;

    DeleteFileW(path);
    if ((r = MsiOpenDatabaseW(path, MSIDBOPEN_CREATE, &db))) { printf("makemsi open %u\n", r); return 1; }
    for (i = 0; i < ARRAYSIZE(sql); i++)
        if ((r = run_sql(db, sql[i]))) { printf("makemsi sql %u: %u\n", i, r); return 1; }
    if (!MsiGetSummaryInformationW(db, NULL, 8, &si))
    {
        MsiSummaryInfoSetPropertyW(si, 7 /* template */, VT_LPSTR, 0, NULL, L"x64;1033");
        MsiSummaryInfoSetPropertyW(si, 9 /* revision */, VT_LPSTR, 0, NULL, L"{1C2D3E4F-5A6B-4C7D-8E9F-0A1B2C3D4E5F}");
        MsiSummaryInfoSetPropertyW(si, 14 /* pages */, VT_I4, 200, NULL, NULL);
        MsiSummaryInfoSetPropertyW(si, 15 /* words */, VT_I4, 2, NULL, NULL);
        MsiSummaryInfoPersist(si);
        MsiCloseHandle(si);
    }
    r = MsiDatabaseCommit(db);
    MsiCloseHandle(db);
    printf("makemsi=%u\n", r);
    return r ? 1 : 0;
}

int wmain(int argc, WCHAR **argv)
{
    WCHAR self[MAX_PATH], params[MAX_PATH + 16], file[MAX_PATH];
    SHELLEXECUTEINFOW sei = { sizeof(sei) };
    TOKEN_ELEVATION elevation = { 0 };
    HANDLE token;
    DWORD size;
    char buf[64] = "";
    FILE *f;

    if (argc == 3 && !wcscmp(argv[1], L"makemsi")) return make_msi(argv[2]);
    if (argc == 3 && !wcscmp(argv[1], L"child"))
    {
        OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token);
        GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size);
        if ((f = _wfopen(argv[2], L"w"))) { fprintf(f, "elevated=%lu\n", elevation.TokenIsElevated); fclose(f); }
        return 0;
    }

    OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token);
    GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size);
    printf("parent elevated=%lu\n", elevation.TokenIsElevated);

    GetModuleFileNameW(NULL, self, MAX_PATH);
    GetTempPathW(MAX_PATH, file);
    lstrcatW(file, L"runasown.txt");
    DeleteFileW(file);
    swprintf(params, ARRAYSIZE(params), L"child \"%ls\"", file);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI;
    sei.lpVerb = L"runas";
    sei.lpFile = self;
    sei.lpParameters = params;
    sei.nShow = SW_HIDE;
    if (!ShellExecuteExW(&sei) || !sei.hProcess) { printf("runas failed %lu\n", GetLastError()); return 1; }
    WaitForSingleObject(sei.hProcess, 30000);
    CloseHandle(sei.hProcess);
    if ((f = _wfopen(file, L"r"))) { fgets(buf, sizeof(buf), f); fclose(f); }
    printf("child %s", buf[0] ? buf : "(no answer)\n");
    return 0;
}
