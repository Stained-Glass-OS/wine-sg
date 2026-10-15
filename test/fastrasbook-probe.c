/* rasapi32 phone book, credentials, autodial and notifications (patch 3181), run by
 * test/fastrasbook-gate.sh in a scratch prefix.
 *   fastrasbook-probe.exe book       every call as an administrator
 *   fastrasbook-probe.exe nonadmin   changes to the all-users phone book from a thread without Administrators
 *   fastrasbook-probe.exe notify UNIXSTATEFILE   RasConnectionNotification against a stand-in sg-netctl */
#include <windows.h>
#include <ras.h>
#include <raserror.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;
static void expect(DWORD got, DWORD want, const char *what)
{
    int ok = got == want;
    printf("%s  %s (%lu, wanted %lu)\n", ok ? "PASS" : "FAIL", what, got, want);
    fflush(stdout);
    if (!ok) failures++;
}
static void checkw(const WCHAR *got, const WCHAR *want, const char *what)
{
    int ok = got && !wcscmp(got, want);
    printf("%s  %s (%ls, wanted %ls)\n", ok ? "PASS" : "FAIL", what, got ? got : L"(null)", want);
    fflush(stdout);
    if (!ok) failures++;
}

static HANDLE restricted_thread_token(void)
{
    HANDLE proc, restricted, imp = NULL;
    SID_IDENTIFIER_AUTHORITY nt = { SECURITY_NT_AUTHORITY };
    PSID admins;
    SID_AND_ATTRIBUTES sa;

    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ALL_ACCESS, &proc)) return NULL;
    AllocateAndInitializeSid(&nt, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &admins);
    sa.Sid = admins; sa.Attributes = 0;
    if (CreateRestrictedToken(proc, DISABLE_MAX_PRIVILEGE, 1, &sa, 0, NULL, 0, NULL, &restricted))
    {
        DuplicateTokenEx(restricted, TOKEN_ALL_ACCESS, NULL, SecurityImpersonation, TokenImpersonation, &imp);
        CloseHandle(restricted);
    }
    CloseHandle(proc);
    FreeSid(admins);
    return imp;
}

static void common_path(WCHAR *out)
{
    WCHAR pd[MAX_PATH];
    GetEnvironmentVariableW(L"ProgramData", pd, MAX_PATH);
    swprintf(out, MAX_PATH, L"%ls\\Microsoft\\Network\\Connections\\Pbk\\rasphone.pbk", pd);
}

static DWORD delete_subentry(const WCHAR *entry, DWORD id)
{
    DWORD (WINAPI *p)(LPCWSTR, LPCWSTR, DWORD) = (void *)GetProcAddress(GetModuleHandleA("rasapi32.dll"), "RasDeleteSubEntryW");
    return p ? p(NULL, entry, id) : 0xdead;
}

static void book(void)
{
    RASENTRYW e = { sizeof(e) }, g = { sizeof(g) };
    RASENTRYA ea = { sizeof(ea) }, ga = { sizeof(ga) };
    DWORD size = 0, dev = 0, count = 0, ret;
    RASENTRYNAMEW names[8];
    RASENTRYNAMEA namesa[8];
    RASDIALPARAMSW dp = { sizeof(dp) };
    RASCREDENTIALSW cr = { sizeof(cr) };
    RASSUBENTRYW sub = { sizeof(sub) }, gsub = { sizeof(gsub) };
    BOOL pw;
    WCHAR common[MAX_PATH];

    common_path(common);
    RasDeleteEntryW(common, L"Shared");   /* left by the other architecture's run */
    expect(RasValidateEntryNameW(NULL, L""), ERROR_INVALID_NAME, "an empty entry name is invalid");
    expect(RasValidateEntryNameW(NULL, L"bad[name"), ERROR_INVALID_NAME, "[ is invalid in an entry name");
    expect(RasValidateEntryNameW(NULL, L"Office"), 0, "a new entry name is valid");

    e.dwfOptions = RASEO_UseCountryAndAreaCodes | RASEO_SpecificIpAddr;
    e.dwCountryID = 1; e.dwCountryCode = 1;
    wcscpy(e.szAreaCode, L"206"); wcscpy(e.szLocalPhoneNumber, L"555-0100");
    e.ipaddr.a = 10; e.ipaddr.b = 1; e.ipaddr.c = 2; e.ipaddr.d = 3;
    wcscpy(e.szDeviceType, L"modem"); wcscpy(e.szDeviceName, L"My Modem");
    e.dwType = RASET_Phone; e.dwFramingProtocol = RASFP_Ppp; e.dwIdleDisconnectSeconds = 300;
    expect(RasSetEntryPropertiesW(NULL, L"Office", &e, sizeof(e), NULL, 0), 0, "RasSetEntryProperties creates an entry");
    expect(RasValidateEntryNameW(NULL, L"Office"), ERROR_ALREADY_EXISTS, "the name is taken now");
    expect(RasSetEntryPropertiesW(NULL, L"Bad]", &e, sizeof(e), NULL, 0), ERROR_INVALID_NAME, "bad name refused");
    e.dwSize = 8;
    expect(RasSetEntryPropertiesW(NULL, L"Tiny", &e, sizeof(e), NULL, 0), ERROR_INVALID_SIZE, "a structure size that is none");
    e.dwSize = sizeof(e);

    expect(RasGetEntryPropertiesW(NULL, L"Office", NULL, &size, NULL, &dev), ERROR_BUFFER_TOO_SMALL, "RasGetEntryProperties size query");
    expect(size, sizeof(RASENTRYW), "needed size");
    expect(RasGetEntryPropertiesW(NULL, L"Office", &g, &size, NULL, &dev), 0, "RasGetEntryProperties");
    checkw(g.szLocalPhoneNumber, L"555-0100", "phone number round-trips");
    checkw(g.szAreaCode, L"206", "area code");
    checkw(g.szDeviceName, L"My Modem", "device name");
    expect(g.ipaddr.a * 1000000 + g.ipaddr.b * 10000 + g.ipaddr.c * 100 + g.ipaddr.d,
           10 * 1000000 + 1 * 10000 + 2 * 100 + 3, "IP address");
    expect(g.dwIdleDisconnectSeconds, 300, "idle time");
    expect(g.dwType, RASET_Phone, "type");
    expect(g.dwSubEntries, 1, "one sub-entry");
    expect(g.guidId.Data1 != 0, 1, "a GUID was made for the entry");
    expect(RasGetEntryPropertiesW(NULL, L"Nope", &g, &size, NULL, &dev), ERROR_CANNOT_FIND_PHONEBOOK_ENTRY, "unknown entry");

    /* A variant on the same file */
    strcpy(ea.szLocalPhoneNumber, "555-0199"); strcpy(ea.szDeviceName, "Other Modem"); ea.dwType = RASET_Phone;
    expect(RasSetEntryPropertiesA(NULL, "Home", &ea, sizeof(ea), NULL, 0), 0, "RasSetEntryPropertiesA");
    size = sizeof(ga);
    expect(RasGetEntryPropertiesA(NULL, "Home", &ga, &size, NULL, &dev), 0, "RasGetEntryPropertiesA");
    expect(strcmp(ga.szLocalPhoneNumber, "555-0199"), 0, "A phone number");

    /* listing */
    names[0].dwSize = sizeof(RASENTRYNAMEW); size = sizeof(names); count = 0;
    expect(RasEnumEntriesW(NULL, NULL, names, &size, &count), 0, "RasEnumEntries");
    expect(count, 2, "two entries");
    {
        int office = 0, home = 0; DWORD i;
        for (i = 0; i < count; i++) { if (!wcscmp(names[i].szEntryName, L"Office")) office = 1; if (!wcscmp(names[i].szEntryName, L"Home")) home = 1; }
        expect(office + home, 2, "Office and Home are listed");
    }
    namesa[0].dwSize = sizeof(RASENTRYNAMEA); size = sizeof(namesa); count = 0;
    expect(RasEnumEntriesA(NULL, NULL, namesa, &size, &count), 0, "RasEnumEntriesA");
    expect(count, 2, "two entries (A)");

    /* dial parameters and credentials */
    wcscpy(dp.szEntryName, L"Office"); wcscpy(dp.szUserName, L"bob"); wcscpy(dp.szPassword, L"pw1"); wcscpy(dp.szDomain, L"DOM");
    wcscpy(dp.szPhoneNumber, L"555-0111");
    expect(RasSetEntryDialParamsW(NULL, &dp, FALSE), 0, "RasSetEntryDialParams");
    memset(&dp, 0, sizeof(dp)); dp.dwSize = sizeof(dp); wcscpy(dp.szEntryName, L"Office"); pw = FALSE;
    expect(RasGetEntryDialParamsW(NULL, &dp, &pw), 0, "RasGetEntryDialParams");
    checkw(dp.szUserName, L"bob", "user name");
    checkw(dp.szDomain, L"DOM", "domain");
    checkw(dp.szPhoneNumber, L"555-0111", "phone number from the dial parameters");
    expect(pw, TRUE, "a password is kept");
    checkw(dp.szPassword, L"pw1", "password round-trips (protected on disk)");
    wcscpy(dp.szUserName, L"bob"); 
    expect(RasSetEntryDialParamsW(NULL, &dp, TRUE), 0, "RasSetEntryDialParams, remove password");
    memset(&dp, 0, sizeof(dp)); dp.dwSize = sizeof(dp); wcscpy(dp.szEntryName, L"Office");
    RasGetEntryDialParamsW(NULL, &dp, &pw);
    expect(pw, FALSE, "the password is gone");
    wcscpy(dp.szEntryName, L"Nope");
    expect(RasSetEntryDialParamsW(NULL, &dp, FALSE), ERROR_CANNOT_FIND_PHONEBOOK_ENTRY, "dial parameters of no entry");

    cr.dwMask = RASCM_UserName | RASCM_Domain | RASCM_Password;
    wcscpy(cr.szUserName, L"carl"); wcscpy(cr.szDomain, L"D2"); wcscpy(cr.szPassword, L"secret2");
    expect(RasSetCredentialsW(NULL, L"Office", &cr, FALSE), 0, "RasSetCredentials");
    memset(&cr, 0, sizeof(cr)); cr.dwSize = sizeof(cr); cr.dwMask = RASCM_UserName | RASCM_Domain | RASCM_Password;
    expect(RasGetCredentialsW(NULL, L"Office", &cr), 0, "RasGetCredentials");
    checkw(cr.szUserName, L"carl", "credential user");
    checkw(cr.szPassword, L"secret2", "credential password");
    cr.dwMask = RASCM_Password;
    expect(RasSetCredentialsW(NULL, L"Office", &cr, TRUE), 0, "RasSetCredentials, clear the password");
    memset(&cr, 0, sizeof(cr)); cr.dwSize = sizeof(cr); cr.dwMask = RASCM_UserName | RASCM_Password;
    RasGetCredentialsW(NULL, L"Office", &cr);
    expect(cr.dwMask, RASCM_UserName, "only the user name is left");

    {
        BYTE auth[4] = { 1, 2, 3, 4 };
        expect(RasSetCustomAuthDataW(NULL, L"Office", auth, 4), 0, "RasSetCustomAuthData");
        expect(RasSetCustomAuthDataW(NULL, L"Nope", auth, 4), ERROR_CANNOT_FIND_PHONEBOOK_ENTRY, "RasSetCustomAuthData, unknown entry");
    }

    /* sub-entries */
    wcscpy(sub.szDeviceType, L"modem"); wcscpy(sub.szDeviceName, L"Second Modem"); wcscpy(sub.szLocalPhoneNumber, L"555-0200");
    expect(RasSetSubEntryPropertiesW(NULL, L"Office", 3, &sub, sizeof(sub), NULL, 0), ERROR_INVALID_PARAMETER, "sub-entries are added in order");
    expect(RasSetSubEntryPropertiesW(NULL, L"Office", 2, &sub, sizeof(sub), NULL, 0), 0, "RasSetSubEntryProperties adds one");
    size = sizeof(g); g.dwSize = sizeof(g);
    RasGetEntryPropertiesW(NULL, L"Office", &g, &size, NULL, &dev);
    expect(g.dwSubEntries, 2, "two sub-entries");
    size = sizeof(gsub); gsub.dwSize = sizeof(gsub);
    expect(RasGetSubEntryPropertiesW(NULL, L"Office", 2, &gsub, &size, NULL, NULL), 0, "RasGetSubEntryProperties");
    checkw(gsub.szLocalPhoneNumber, L"555-0200", "sub-entry phone number");
    expect(delete_subentry(L"Office", 2), 0, "RasDeleteSubEntry");
    expect(delete_subentry(L"Office", 1), ERROR_INVALID_PARAMETER, "the last sub-entry stays");

    /* rename and delete */
    expect(RasRenameEntryW(NULL, L"Office", L"Home"), ERROR_ALREADY_EXISTS, "rename onto an existing name");
    expect(RasRenameEntryW(NULL, L"Nope", L"X"), ERROR_CANNOT_FIND_PHONEBOOK_ENTRY, "rename of an unknown entry");
    expect(RasRenameEntryW(NULL, L"Office", L"Work"), 0, "RasRenameEntry");
    size = sizeof(g);
    expect(RasGetEntryPropertiesW(NULL, L"Work", &g, &size, NULL, &dev), 0, "the renamed entry");
    checkw(g.szLocalPhoneNumber, L"555-0111", "keeps its properties (the number set with the dial parameters)");
    expect(RasGetEntryPropertiesW(NULL, L"Office", &g, &size, NULL, &dev), ERROR_CANNOT_FIND_PHONEBOOK_ENTRY, "the old name is gone");
    expect(RasDeleteEntryW(NULL, L"Work"), 0, "RasDeleteEntry");
    expect(RasDeleteEntryW(NULL, L"Work"), ERROR_CANNOT_FIND_PHONEBOOK_ENTRY, "RasDeleteEntry again");
    expect(RasDeleteEntryA(NULL, "Home"), 0, "RasDeleteEntryA");

    /* the all-users phone book */
    memset(&e, 0, sizeof(e)); e.dwSize = sizeof(e); wcscpy(e.szLocalPhoneNumber, L"555-0300"); e.dwType = RASET_Phone;
    expect(RasSetEntryPropertiesW(common, L"Shared", &e, sizeof(e), NULL, 0), 0, "an administrator writes the all-users phone book");
    {
        RASENTRYNAMEW n7[4]; n7[0].dwSize = sizeof(RASENTRYNAMEW); size = sizeof(n7); count = 0;
        expect(RasEnumEntriesW(NULL, NULL, n7, &size, &count), 0, "enumeration over both phone books");
        expect(count, 1, "the all-users entry is listed");
        count = 0; size = sizeof(n7);
        expect(RasEnumEntriesW(NULL, common, n7, &size, &count), 0, "enumeration of the all-users file");
        expect(count, 1, "and it has the entry");
    }

    /* automatic dialling */
    {
        RASAUTODIALENTRYW ad = { sizeof(ad) }, got[2];
        DWORD cb = 0, cnt = 0, v = 0, vs = sizeof(v);
        LPWSTR addrs[8]; BOOL en = TRUE;
        ad.dwDialingLocation = 1; wcscpy(ad.szEntry, L"Home");
        expect(RasSetAutodialAddressW(L"example.com", 0, &ad, sizeof(ad), 1), 0, "RasSetAutodialAddress");
        cb = sizeof(got);
        expect(RasGetAutodialAddressW(L"example.com", NULL, got, &cb, &cnt), 0, "RasGetAutodialAddress");
        expect(cnt, 1, "one autodial entry");
        checkw(got[0].szEntry, L"Home", "its phone book entry");
        cb = sizeof(addrs); cnt = 0;
        expect(RasEnumAutodialAddressesW(addrs, &cb, &cnt), 0, "RasEnumAutodialAddresses");
        expect(cnt, 1, "one address");
        if (cnt) checkw(addrs[0], L"example.com", "the address");
        expect(RasSetAutodialAddressW(L"example.com", 0, NULL, 0, 0), 0, "RasSetAutodialAddress removes the address");
        cb = 0; cnt = 5;
        expect(RasGetAutodialAddressW(L"example.com", NULL, NULL, &cb, &cnt), 0, "and it is gone");
        expect(cnt, 0, "no entries");
        v = 7;
        expect(RasSetAutodialParamW(RASADP_SavedAddressesLimit, &v, sizeof(v)), 0, "RasSetAutodialParam");
        v = 0;
        expect(RasGetAutodialParamW(RASADP_SavedAddressesLimit, &v, &vs), 0, "RasGetAutodialParam");
        expect(v, 7, "the parameter was stored");
        expect(RasSetAutodialParamW(RASADP_SavedAddressesLimit, &v, 1), ERROR_INVALID_SIZE, "wrong parameter size");
        expect(RasSetAutodialParamW(99, &v, sizeof(v)), ERROR_INVALID_PARAMETER, "unknown parameter");
        expect(RasSetAutodialEnableW(1, TRUE), 0, "RasSetAutodialEnable");
        en = FALSE;
        expect(RasGetAutodialEnableW(1, &en), 0, "RasGetAutodialEnable");
        expect(en, TRUE, "enabled");
        RasGetAutodialEnableW(2, &en);
        expect(en, FALSE, "another dialing location is not");
    }

    expect(RasHangUpW((HRASCONN)0x1234), ERROR_INVALID_HANDLE, "hanging up a handle that is no connection");
}

struct dlg_job { const WCHAR *title, *name, *phone, *device; };

static DWORD WINAPI dlg_thread(void *arg)
{
    struct dlg_job *j = arg;
    HWND w = NULL;
    int i;
    for (i = 0; i < 100 && !(w = FindWindowW(NULL, j->title)); i++) Sleep(100);
    if (!w) return 1;
    if (j->name) SetDlgItemTextW(w, 101, j->name);
    SetDlgItemTextW(w, 102, j->phone);
    SetDlgItemTextW(w, 103, j->device);
    PostMessageW(w, WM_COMMAND, IDOK, 0);
    return 0;
}

static void dialog(void)
{
    struct dlg_job j1 = { L"New Phone Book Entry", L"Dialed", L"555-0500", L"A Modem" };
    struct dlg_job j2 = { L"Phone Book Entry Properties", NULL, L"555-0600", L"B Modem" };
    RASENTRYW g = { sizeof(g) };
    DWORD size = sizeof(g), dev = 0;
    HANDLE t = CreateThread(NULL, 0, dlg_thread, &j1, 0, NULL);

    expect(RasCreatePhonebookEntryW(NULL, NULL), 0, "RasCreatePhonebookEntry");
    WaitForSingleObject(t, 5000); CloseHandle(t);
    expect(RasGetEntryPropertiesW(NULL, L"Dialed", &g, &size, NULL, &dev), 0, "the entry made in the dialog exists");
    checkw(g.szLocalPhoneNumber, L"555-0500", "its phone number");
    checkw(g.szDeviceName, L"A Modem", "its device");
    t = CreateThread(NULL, 0, dlg_thread, &j2, 0, NULL);
    expect(RasEditPhonebookEntryW(NULL, NULL, L"Dialed"), 0, "RasEditPhonebookEntry");
    WaitForSingleObject(t, 5000); CloseHandle(t);
    size = sizeof(g);
    RasGetEntryPropertiesW(NULL, L"Dialed", &g, &size, NULL, &dev);
    checkw(g.szLocalPhoneNumber, L"555-0600", "the phone number was changed");
    expect(RasEditPhonebookEntryW(NULL, NULL, L"Nope"), ERROR_CANNOT_FIND_PHONEBOOK_ENTRY, "editing an unknown entry");
    RasDeleteEntryW(NULL, L"Dialed");
}

static void nonadmin(void)
{
    RASENTRYW e = { sizeof(e) };
    WCHAR common[MAX_PATH];
    HANDLE imp = restricted_thread_token();

    expect(imp && SetThreadToken(NULL, imp), 1, "a thread without the Administrators group");
    common_path(common);
    wcscpy(e.szLocalPhoneNumber, L"555-0400"); e.dwType = RASET_Phone;
    expect(RasSetEntryPropertiesW(common, L"Intruder", &e, sizeof(e), NULL, 0), ERROR_ACCESS_DENIED, "all-users phone book refused");
    expect(RasSetEntryPropertiesW(NULL, L"Mine", &e, sizeof(e), NULL, 0), 0, "the user's own phone book is fine");
    expect(RasDeleteEntryW(common, L"Shared"), ERROR_ACCESS_DENIED, "deleting from the all-users book refused");
    expect(RasRenameEntryW(common, L"Shared", L"Renamed"), ERROR_ACCESS_DENIED, "renaming in the all-users book refused");
}

static void notify(const char *statefile)
{
    WCHAR path[MAX_PATH + 16], *p;
    HANDLE ev = CreateEventW(NULL, FALSE, FALSE, NULL), f;
    DWORD w;

    swprintf(path, ARRAYSIZE(path), L"\\\\?\\unix%hs", statefile);
    for (p = path + 8; *p; p++) if (*p == '/') *p = '\\';
    expect(RasConnectionNotificationW(INVALID_HANDLE_VALUE, ev, 0x10), ERROR_INVALID_PARAMETER, "unknown notification flags");
    expect(RasConnectionNotificationW(INVALID_HANDLE_VALUE, NULL, RASCN_Connection), ERROR_INVALID_PARAMETER, "no event");
    expect(RasConnectionNotificationW(INVALID_HANDLE_VALUE, ev, RASCN_Connection | RASCN_Disconnection), 0, "register for connect and disconnect");
    w = WaitForSingleObject(ev, 2500);
    expect(w, WAIT_TIMEOUT, "nothing happens while nothing changes");
    f = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_NEW, 0, NULL);
    if (f != INVALID_HANDLE_VALUE) CloseHandle(f);
    w = WaitForSingleObject(ev, 6000);
    expect(w, WAIT_OBJECT_0, "a connection coming up signals the event");
    DeleteFileW(path);
    w = WaitForSingleObject(ev, 6000);
    expect(w, WAIT_OBJECT_0, "and going down");
    expect(RasConnectionNotificationW(INVALID_HANDLE_VALUE, ev, 0), 0, "withdraw the registration");
    Sleep(300);
    f = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_NEW, 0, NULL);
    if (f != INVALID_HANDLE_VALUE) CloseHandle(f);
    w = WaitForSingleObject(ev, 3500);
    expect(w, WAIT_TIMEOUT, "no signal after the registration is gone");
    DeleteFileW(path);
}

int main(int argc, char **argv)
{
    if (argc > 1 && !strcmp(argv[1], "book")) book();
    else if (argc > 1 && !strcmp(argv[1], "nonadmin")) nonadmin();
    else if (argc > 1 && !strcmp(argv[1], "dialog")) dialog();
    else if (argc > 2 && !strcmp(argv[1], "notify")) notify(argv[2]);
    else return 2;
    printf(failures ? "RESULT: FAIL\n" : "RESULT: PASS\n");
    return failures != 0;
}
