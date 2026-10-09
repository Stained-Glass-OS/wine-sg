/* Windows credentials prompt and authentication buffers (patches/sg/1703),
 * run by test/creduiwin-gate.sh on Xvfb: CredUIPromptForWindowsCredentials,
 * CredPackAuthenticationBuffer and CredUnPackAuthenticationBuffer were
 * stubs returning ERROR_CALL_NOT_IMPLEMENTED (the pack and unpack ones,
 * BOOL calls, so "succeeding" with nothing done); the A forms, the command
 * line prompt and CredUIPromptForCredentialsA were "@ stub"s and the SSO
 * calls FIXMEs keeping nothing. A thread fills the prompt in and presses OK
 * or Cancel.
 *
 *   creduiwin-probe.exe [cmdline] */
#define COBJMACROS
#include <windows.h>
#include <wincred.h>
#include <stdio.h>

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#ifndef CRED_PACK_PROTECTED_CREDENTIALS
#define CRED_PACK_PROTECTED_CREDENTIALS 1
#define CRED_PACK_WOW_BUFFER 2
#define CRED_PACK_GENERIC_CREDENTIALS 4
#endif

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

typedef BOOL (WINAPI *pack_w)(DWORD, WCHAR *, WCHAR *, BYTE *, DWORD *);
typedef BOOL (WINAPI *pack_a)(DWORD, char *, char *, BYTE *, DWORD *);
typedef BOOL (WINAPI *unpack_w)(DWORD, void *, DWORD, WCHAR *, DWORD *, WCHAR *, DWORD *, WCHAR *, DWORD *);
typedef BOOL (WINAPI *unpack_a)(DWORD, void *, DWORD, char *, DWORD *, char *, DWORD *, char *, DWORD *);
typedef DWORD (WINAPI *prompt_win)(CREDUI_INFOW *, DWORD, ULONG *, const void *, ULONG, void **, ULONG *, BOOL *, DWORD);
typedef DWORD (WINAPI *cmdline_w)(const WCHAR *, void *, DWORD, WCHAR *, ULONG, WCHAR *, ULONG, BOOL *, DWORD);
typedef DWORD (WINAPI *sso_store)(const WCHAR *, const WCHAR *, const WCHAR *, BOOL);
typedef DWORD (WINAPI *sso_read)(const WCHAR *, WCHAR **);

static pack_w pPack;
static unpack_w pUnpack;
static volatile int fill_mode; /* 1: fill and OK, 2: cancel */
static volatile BOOL prefilled_ok;
static const WCHAR *expect_user;

static DWORD WINAPI filler(void *arg)
{
    DWORD tid = (DWORD)(ULONG_PTR)arg, pid;
    HWND dlg = NULL;
    int i;

    for (i = 0; i < 200 && !dlg; i++)
    {
        HWND w = NULL;
        Sleep(50);
        while ((w = FindWindowExW(NULL, w, L"#32770", NULL)))
            if (GetWindowThreadProcessId(w, &pid) == tid && IsWindowVisible(w)) { dlg = w; break; }
    }
    if (!dlg)
    {
        printf("      no dialog found\n");
        fflush(stdout);
        return 1;
    }
    printf("      dialog %p\n", dlg); fflush(stdout);
    Sleep(200);
    if (expect_user)
    {
        WCHAR text[256] = {0};
        GetWindowTextW(GetDlgItem(dlg, 101), text, ARRAY_SIZE(text));
        prefilled_ok = !lstrcmpW(text, expect_user);
    }
    if (fill_mode == 1)
    {
        HWND combo = GetDlgItem(dlg, 101), edit = (HWND)SendMessageW(combo, 0x0406 /* CBEM_GETEDITCONTROL */, 0, 0);
        SetWindowTextW(combo, L"sguser");
        if (edit) SetWindowTextW(edit, L"sguser");
        SetDlgItemTextW(dlg, 102, L"sgpass");
        CheckDlgButton(dlg, 104, BST_CHECKED);
        printf("      filled\n"); fflush(stdout);
        PostMessageW(dlg, WM_COMMAND, IDOK, 0);
    }
    else PostMessageW(dlg, WM_COMMAND, IDCANCEL, 0);
    return 0;
}

static DWORD prompt(prompt_win fn, DWORD flags, const void *in, ULONG in_size, void **out, ULONG *out_size, BOOL *save)
{
    HANDLE thread = CreateThread(NULL, 0, filler, (void *)(ULONG_PTR)GetCurrentThreadId(), 0, NULL);
    ULONG package = 0;
    DWORD ret;

    ret = fn(NULL, 0, &package, in, in_size, out, out_size, save, flags);
    WaitForSingleObject(thread, 15000);
    CloseHandle(thread);
    return ret;
}

int main(int argc, char **argv)
{
    HMODULE credui = LoadLibraryA("credui.dll");
    pack_a pPackA;
    unpack_a pUnpackA;
    prompt_win pPrompt;
    cmdline_w pCmdline;
    sso_store pStore;
    sso_read pRead;
    BYTE buf[1024];
    DWORD size, ulen, dlen, plen;
    WCHAR user[256], dom[256], pass[256];
    char userA[256], passA[256];
    void *out = NULL;
    ULONG out_size = 0;
    BOOL save = FALSE;
    DWORD ret;

    pPack = (void *)GetProcAddress(credui, "CredPackAuthenticationBufferW");
    pPackA = (void *)GetProcAddress(credui, "CredPackAuthenticationBufferA");
    pUnpack = (void *)GetProcAddress(credui, "CredUnPackAuthenticationBufferW");
    pUnpackA = (void *)GetProcAddress(credui, "CredUnPackAuthenticationBufferA");
    pPrompt = (void *)GetProcAddress(credui, "CredUIPromptForWindowsCredentialsW");
    pCmdline = (void *)GetProcAddress(credui, "CredUICmdLinePromptForCredentialsW");
    pStore = (void *)GetProcAddress(credui, "CredUIStoreSSOCredW");
    pRead = (void *)GetProcAddress(credui, "CredUIReadSSOCredW");

    if (argc > 1 && !strcmp(argv[1], "cmdline"))
    {
        WCHAR u[64] = L"", p[64] = L"";
        BOOL s = FALSE;
        ret = pCmdline(L"sgserver", NULL, 0, u, 64, p, 64, &s,
                       CREDUI_FLAGS_GENERIC_CREDENTIALS | CREDUI_FLAGS_ALWAYS_SHOW_UI |
                       CREDUI_FLAGS_SHOW_SAVE_CHECK_BOX | CREDUI_FLAGS_DO_NOT_PERSIST);
        return !ret && !lstrcmpW(u, L"cmduser") && !lstrcmpW(p, L"cmdpass") && s ? 0 : 1;
    }

    check(pPack && pPackA && pUnpack && pUnpackA && pPrompt && pCmdline && pStore && pRead, "the calls are there");
    if (failures) goto done;

    /* packing */
    size = 0;
    check(!pPack(0, (WCHAR *)L"SGDOM\\alice", (WCHAR *)L"secret", NULL, &size) && GetLastError() == ERROR_INSUFFICIENT_BUFFER
          && size > 0, "CredPackAuthenticationBuffer: the size needed (it 'succeeded' doing nothing)");
    check(pPack(0, (WCHAR *)L"SGDOM\\alice", (WCHAR *)L"secret", buf, &size), "packed");
    check(*(ULONG *)buf == 2, "a KERB_INTERACTIVE_LOGON");
    ulen = dlen = plen = 256;
    check(pUnpack(0, buf, size, user, &ulen, dom, &dlen, pass, &plen) && !lstrcmpW(user, L"SGDOM\\alice")
          && !dom[0] && !lstrcmpW(pass, L"secret") && ulen == 12 && plen == 7, "CredUnPackAuthenticationBuffer: back");
    ulen = 3; plen = 256; dlen = 256;
    check(!pUnpack(0, buf, size, user, &ulen, dom, &dlen, pass, &plen) && GetLastError() == ERROR_INSUFFICIENT_BUFFER
          && ulen == 12, "a small user buffer: the size needed");

    size = sizeof(buf);
    check(pPack(CRED_PACK_PROTECTED_CREDENTIALS, (WCHAR *)L"bob", (WCHAR *)L"hidden", buf, &size), "packed protected");
    ulen = plen = 256;
    check(pUnpack(0, buf, size, user, &ulen, NULL, NULL, pass, &plen) && lstrcmpW(pass, L"hidden"),
          "unpacked without the flag: still protected");
    ulen = plen = 256;
    check(pUnpack(CRED_PACK_PROTECTED_CREDENTIALS, buf, size, user, &ulen, NULL, NULL, pass, &plen)
          && !lstrcmpW(pass, L"hidden"), "with it: the password");

    size = sizeof(buf);
    check(pPack(CRED_PACK_WOW_BUFFER, (WCHAR *)L"carol", (WCHAR *)L"pw", buf, &size) && size < 28 + 64,
          "a 32-bit (WOW) buffer");
    ulen = plen = 256;
    check(pUnpack(CRED_PACK_WOW_BUFFER, buf, size, user, &ulen, NULL, NULL, pass, &plen) && !lstrcmpW(user, L"carol"),
          "and back");

    memset(buf, 0x55, 64);
    ulen = plen = 256;
    check(!pUnpack(0, buf, 64, user, &ulen, NULL, NULL, pass, &plen), "not a buffer: refused");

    size = sizeof(buf);
    check(pPackA(0, (char *)"dave", (char *)"apw", buf, &size), "CredPackAuthenticationBufferA");
    ulen = plen = 256;
    check(pUnpackA(0, buf, size, userA, &ulen, NULL, NULL, passA, &plen) && !strcmp(userA, "dave") && !strcmp(passA, "apw"),
          "CredUnPackAuthenticationBufferA");

    /* the prompt */
    fill_mode = 1;
    ret = prompt(pPrompt, CREDUIWIN_GENERIC | CREDUIWIN_CHECKBOX, NULL, 0, &out, &out_size, &save);
    check(ret == ERROR_SUCCESS && out && out_size, "CredUIPromptForWindowsCredentials: OK (was not implemented)");
    if (!ret && out)
    {
        ulen = plen = 256;
        check(pUnpack(0, out, out_size, user, &ulen, NULL, NULL, pass, &plen) && !lstrcmpW(user, L"sguser")
              && !lstrcmpW(pass, L"sgpass"), "what was entered, packed");
        check(save, "the check box");
        CoTaskMemFree(out);
    }
    size = sizeof(buf);
    pPack(0, (WCHAR *)L"preset", (WCHAR *)L"x", buf, &size);
    fill_mode = 2;
    expect_user = L"preset";
    out = NULL;
    ret = prompt(pPrompt, CREDUIWIN_GENERIC, buf, size, &out, &out_size, NULL);
    check(ret == ERROR_CANCELLED && !out, "cancelled: ERROR_CANCELLED");
    check(prefilled_ok, "the user name from the input buffer");
    expect_user = NULL;
    check(pPrompt(NULL, 0, NULL, NULL, 0, &out, &out_size, NULL, 0) == ERROR_INVALID_PARAMETER, "no package: refused");

    /* single sign-on */
    {
        WCHAR *name = NULL;
        check(pStore(L"sg.realm", L"ssouser", L"ssopass", FALSE) == ERROR_SUCCESS, "CredUIStoreSSOCred");
        check(pRead(L"sg.realm", &name) == ERROR_SUCCESS && name && !lstrcmpW(name, L"ssouser"),
              "CredUIReadSSOCred: the user (was never found)");
        if (name) LocalFree(name);
        CredDeleteW(L"sg.realm", CRED_TYPE_GENERIC, 0);
        name = NULL;
        check(pRead(L"sg.other", &name) == ERROR_NOT_FOUND && !name, "another realm: ERROR_NOT_FOUND");
    }

    /* the command line prompt reads standard input */
    {
        char cmd[MAX_PATH + 16];
        STARTUPINFOA si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
        HANDLE rd, wr, nul;
        DWORD code = 99, written;

        CreatePipe(&rd, &wr, &sa, 0);
        SetHandleInformation(wr, HANDLE_FLAG_INHERIT, 0);
        nul = CreateFileA("NUL", GENERIC_WRITE, FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, NULL);
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdInput = rd;
        si.hStdOutput = nul;
        si.hStdError = nul;
        cmd[0] = '"';
        GetModuleFileNameA(NULL, cmd + 1, MAX_PATH);
        strcat(cmd, "\" cmdline");
        if (CreateProcessA(NULL, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi))
        {
            WriteFile(wr, "cmduser\r\ncmdpass\r\nY\r\n", 21, &written, NULL);
            CloseHandle(wr);
            WaitForSingleObject(pi.hProcess, 20000);
            GetExitCodeProcess(pi.hProcess, &code);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }
        CloseHandle(rd);
        CloseHandle(nul);
        check(code == 0, "CredUICmdLinePromptForCredentials: user, password and save from standard input (was a stub)");
    }
done:
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
