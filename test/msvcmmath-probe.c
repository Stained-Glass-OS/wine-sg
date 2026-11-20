/* msvcm80/msvcm90 (wine-sg 8208), for test/msvcmmath-gate.sh: a C++/CLI module's math error handler
 * (__setusermatherr_m) is the C runtime's: sqrt(-1) calls it, and its return value is sqrt's.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <stdio.h>
#include <string.h>

struct exception_ { int type; char *name; double arg1, arg2, retval; };
static int calls, last_type;
static char last_name[16];

static int __cdecl handler(struct exception_ *e)
{
    calls++;
    last_type = e->type;
    lstrcpynA(last_name, e->name ? e->name : "", sizeof(last_name));
    e->retval = 42.0;
    return 1;
}

static void check(const char *m, const char *r)
{
    HMODULE msvcm = LoadLibraryA(m), crt = LoadLibraryA(r);
    void (__cdecl *set)(void *) = msvcm ? (void *)GetProcAddress(msvcm, "__setusermatherr_m") : NULL;
    void (__cdecl *validate)(void) = msvcm ? (void *)GetProcAddress(msvcm, "?DoDllLanguageSupportValidation@<CrtImplementationDetails>@@YAXXZ") : NULL;
    double (__cdecl *sqrt_)(double) = crt ? (void *)GetProcAddress(crt, "sqrt") : NULL;
    double v;

    if (!set || !sqrt_ || !validate) { printf("%s=missing %p %p %p\n", m, set, sqrt_, validate); return; }
    validate();
    calls = 0;
    set(handler);
    v = sqrt_(-1.0);
    printf("%s=%d %d %s %.0f\n", m, calls, last_type, last_name, v);
    set(NULL);
}

int main(void)
{
    check("msvcm80.dll", "msvcr80.dll");
    check("msvcm90.dll", "msvcr90.dll");
    printf("done=1\n");
    return 0;
}
