/* toolkitenv-probe (patches/sg/5946, 5951, test/toolkitenv-gate.sh)
 *   check          prints WINENV NAME VALUE (or -) for the toolkit variables
 *   child FILE     starts itself again ("unix FILE") as a Windows child process and waits
 *   unix FILE      runs /bin/sh, a Unix program, which writes "$GDK_BACKEND $SDL_VIDEODRIVER
 *                  $QT_QPA_PLATFORM $JAVA_TOOL_OPTIONS" to FILE (a Unix path)
 *   env FILE       starts the Unix program /usr/bin/env with CreateProcess, its output to FILE (a
 *                  Windows path) */
#include <windows.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    static const char *names[] = { "GDK_BACKEND", "SDL_VIDEODRIVER", "GTK_THEME", "GSK_RENDERER", "QT_QPA_PLATFORM", "MOZ_ENABLE_WAYLAND", "JAVA_TOOL_OPTIONS" };
    const char *m = argc > 1 ? argv[1] : "check";
    char buf[512];
    unsigned int i;

    if (!strcmp(m, "check"))
        for (i = 0; i < sizeof(names) / sizeof(names[0]); i++)
            printf("WINENV %s %s\n", names[i], GetEnvironmentVariableA(names[i], buf, sizeof(buf)) ? buf : "-");
    else if (!strcmp(m, "child") && argc > 2)
    {
        STARTUPINFOA si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        char cmd[1024];
        GetModuleFileNameA(NULL, buf, sizeof(buf));
        snprintf(cmd, sizeof(cmd), "\"%s\" unix %s", buf, argv[2]);
        if (!CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) { printf("CREATE %lu\n", GetLastError()); return 1; }
        WaitForSingleObject(pi.hProcess, 60000);
        printf("CHILD done\n");
    }
    else if (!strcmp(m, "unix") && argc > 2)
    {
        LONG (WINAPI *spawnvp)(char * const argv[], int wait) =
            (void *)GetProcAddress(GetModuleHandleA("ntdll.dll"), "__wine_unix_spawnvp");
        char sh[] = "/bin/sh", c[] = "-c", script[700];
        char *args[] = { sh, c, script, NULL };
        snprintf(script, sizeof(script), "echo \"$GDK_BACKEND $SDL_VIDEODRIVER $QT_QPA_PLATFORM $JAVA_TOOL_OPTIONS\" > '%s'", argv[2]);
        if (!spawnvp) return 2;
        return spawnvp(args, TRUE);
    }
    else if (!strcmp(m, "env") && argc > 2)
    {
        SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
        STARTUPINFOA si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        char cmd[] = "Z:\\usr\\bin\\env";
        HANDLE out = CreateFileA(argv[2], GENERIC_WRITE, FILE_SHARE_READ, &sa, CREATE_ALWAYS, 0, NULL);
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdOutput = si.hStdError = out;
        si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
        if (!CreateProcessA(NULL, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) { printf("CREATE %lu\n", GetLastError()); return 1; }
        WaitForSingleObject(pi.hProcess, 60000);
        CloseHandle(out);
    }
    fflush(stdout);
    return 0;
}
