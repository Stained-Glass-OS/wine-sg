/* A program's own pseudoconsole host started as ours (patches/sg/6181), as Windows Terminal's ConPTY code
 * does it: the console driver's server opened without synchronous I/O and its "\Reference" below it
 * (a name relative to the device's open file, with a leading backslash); the program's
 * OpenConsole.exe started with "--headless --textMeasurement ... --signal 0x.. --server 0x.." runs as our
 * conhost.exe; a synchronous signal pipe; a client started on the pseudoconsole is named by
 * GetModuleFileNameEx before its loader has run; and its output reaches the program. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winternl.h>
#include <psapi.h>
#include <stdio.h>
#include <string.h>

#ifndef PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE
#define PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE 0x00020016
#endif

typedef NTSTATUS (WINAPI *open_fn)(HANDLE *, ACCESS_MASK, OBJECT_ATTRIBUTES *, IO_STATUS_BLOCK *, ULONG, ULONG);

static NTSTATUS open_device(open_fn open, HANDLE *h, HANDLE root, const WCHAR *name, ACCESS_MASK access, BOOL inherit, ULONG options)
{
    UNICODE_STRING str;
    OBJECT_ATTRIBUTES attr;
    IO_STATUS_BLOCK io;
    str.Buffer = (WCHAR *)name;
    str.Length = (USHORT)(wcslen(name) * sizeof(WCHAR));
    str.MaximumLength = str.Length + sizeof(WCHAR);
    InitializeObjectAttributes(&attr, &str, OBJ_CASE_INSENSITIVE | (inherit ? OBJ_INHERIT : 0), root, NULL);
    return open(h, access, &attr, &io, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, options);
}

struct pty { HANDLE signal, reference, process; };

int main(void)
{
    open_fn open = (open_fn)GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtOpenFile");
    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
    HANDLE server = NULL, reference = NULL, sig_host, sig_ours, in_read, in_write, out_read, out_write;
    HANDLE inherit[4];
    STARTUPINFOEXW si;
    PROCESS_INFORMATION host, client;
    SIZE_T size = 0;
    WCHAR cmd[512], name[MAX_PATH];
    char output[8192] = "";
    DWORD got = 0, avail, n, start;
    struct pty pty;
    NTSTATUS status;
    BOOL ok;

    setvbuf(stdout, NULL, _IONBF, 0);
    status = open_device(open, &server, NULL, L"\\Device\\ConDrv\\Server", GENERIC_ALL, TRUE, 0);
    printf("server=%08lx\n", status);
    status = open_device(open, &reference, server, L"\\Reference", GENERIC_READ | GENERIC_WRITE | SYNCHRONIZE, FALSE,
                         0x20 /* FILE_SYNCHRONOUS_IO_NONALERT */);
    printf("reference=%08lx\n", status);
    if (!server || !reference) return 1;

    CreatePipe(&sig_host, &sig_ours, NULL, 0);
    SetHandleInformation(sig_host, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
    CreatePipe(&in_read, &in_write, &sa, 0);
    CreatePipe(&out_read, &out_write, &sa, 0);
    SetHandleInformation(in_write, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(out_read, HANDLE_FLAG_INHERIT, 0);

    /* the program's own host, which is not there: ours runs for it */
    swprintf(cmd, 512, L"\"C:\\probe\\OpenConsole.exe\" --headless --textMeasurement graphemes --width 80 --height 25 "
             L"--signal 0x%Ix --server 0x%Ix", (ULONG_PTR)sig_host, (ULONG_PTR)server);
    memset(&si, 0, sizeof(si));
    si.StartupInfo.cb = sizeof(si);
    si.StartupInfo.hStdInput = in_read;
    si.StartupInfo.hStdOutput = si.StartupInfo.hStdError = out_write;
    si.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    inherit[0] = server; inherit[1] = in_read; inherit[2] = out_write; inherit[3] = sig_host;
    InitializeProcThreadAttributeList(NULL, 1, 0, &size);
    si.lpAttributeList = HeapAlloc(GetProcessHeap(), 0, size);
    InitializeProcThreadAttributeList(si.lpAttributeList, 1, 0, &size);
    UpdateProcThreadAttribute(si.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inherit, sizeof(inherit), NULL, NULL);
    ok = CreateProcessW(L"C:\\probe\\OpenConsole.exe", cmd, NULL, NULL, TRUE, EXTENDED_STARTUPINFO_PRESENT, NULL, NULL,
                        &si.StartupInfo, &host);
    printf("host=%d,%lu\n", ok, ok ? 0 : GetLastError());
    if (!ok) return 1;
    CloseHandle(host.hThread);
    CloseHandle(in_read);
    CloseHandle(out_write);

    /* a client on the pseudoconsole */
    pty.signal = sig_ours; pty.reference = reference; pty.process = host.hProcess;
    memset(&si, 0, sizeof(si));
    si.StartupInfo.cb = sizeof(si);
    size = 0;
    InitializeProcThreadAttributeList(NULL, 1, 0, &size);
    si.lpAttributeList = HeapAlloc(GetProcessHeap(), 0, size);
    InitializeProcThreadAttributeList(si.lpAttributeList, 1, 0, &size);
    UpdateProcThreadAttribute(si.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE, &pty, sizeof(void *), NULL, NULL);   /* the HPCON */
    wcscpy(cmd, L"C:\\windows\\system32\\cmd.exe /c echo sg-pty-hello");
    ok = CreateProcessW(NULL, cmd, NULL, NULL, FALSE, EXTENDED_STARTUPINFO_PRESENT, NULL, NULL, &si.StartupInfo, &client);
    name[0] = 0;
    n = ok ? GetModuleFileNameExW(client.hProcess, NULL, name, MAX_PATH) : 0;
    printf("client=%d,%d\n", ok, n && !_wcsicmp(name + wcslen(name) - 7, L"cmd.exe"));

    /* a resize signal: the host takes it and goes on */
    {
        unsigned short resize[3] = { 8, 100, 30 };
        DWORD written;
        WriteFile(sig_ours, resize, sizeof(resize), &written, NULL);
    }

    /* its output */
    start = GetTickCount();
    while (GetTickCount() - start < 15000 && !strstr(output, "sg-pty-hello"))
    {
        if (PeekNamedPipe(out_read, NULL, 0, NULL, &avail, NULL) && avail)
        {
            if (!ReadFile(out_read, output + got, min(avail, sizeof(output) - got - 1), &n, NULL)) break;
            got += n;
            output[got] = 0;
        }
        else Sleep(50);
    }
    printf("echo=%d\n", strstr(output, "sg-pty-hello") != NULL);
    TerminateProcess(host.hProcess, 0);
    return 0;
}
