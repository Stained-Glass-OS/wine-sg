/* proxyflagsui-probe (patches/sg/9752, test/proxyflagsui-gate.sh): InternetQueryOption's
 * per-connection INTERNET_PER_CONN_FLAGS_UI (10), which Firefox, Thunderbird and Pale Moon ask for
 * first when they read the system proxy. Prints:
 *   QUERY <ok|fail gle> <flags_ui> <flags>    both in one list, the process's settings
 *   QUERYSESSION <ok|fail gle> <flags_ui> <flags>  the same on an Internet session handle
 *   SET <ok|fail gle>                          FLAGS_UI = PROXY_TYPE_DIRECT|PROXY_TYPE_AUTO_DETECT
 *   AFTER <flags>                              INTERNET_PER_CONN_FLAGS read back */
#include <windows.h>
#include <wininet.h>
#include <stdio.h>

#ifndef INTERNET_PER_CONN_FLAGS_UI
#define INTERNET_PER_CONN_FLAGS_UI 10
#endif

static BOOL query(HINTERNET h, DWORD *a, DWORD *b, DWORD n)
{
    INTERNET_PER_CONN_OPTIONW opt[2];
    INTERNET_PER_CONN_OPTION_LISTW list;
    DWORD size = sizeof(list);
    BOOL r;

    opt[0].dwOption = INTERNET_PER_CONN_FLAGS_UI;
    opt[1].dwOption = INTERNET_PER_CONN_FLAGS;
    list.dwSize = sizeof(list);
    list.pszConnection = NULL;
    list.dwOptionCount = n;
    list.dwOptionError = 0;
    list.pOptions = opt;
    r = InternetQueryOptionW(h, INTERNET_OPTION_PER_CONNECTION_OPTION, &list, &size);
    *a = opt[0].Value.dwValue;
    if (b) *b = opt[1].Value.dwValue;
    return r;
}

int main(void)
{
    INTERNET_PER_CONN_OPTIONW opt;
    INTERNET_PER_CONN_OPTION_LISTW list;
    HINTERNET ses;
    DWORD a = 0, b = 0;

    if (query(NULL, &a, &b, 2)) printf("QUERY ok %lu %lu\n", a, b);
    else printf("QUERY fail %lu\n", GetLastError());
    ses = InternetOpenW(L"probe", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    a = b = 0;
    if (query(ses, &a, &b, 2)) printf("QUERYSESSION ok %lu %lu\n", a, b);
    else printf("QUERYSESSION fail %lu\n", GetLastError());
    InternetCloseHandle(ses);

    opt.dwOption = INTERNET_PER_CONN_FLAGS_UI;
    opt.Value.dwValue = PROXY_TYPE_DIRECT | PROXY_TYPE_AUTO_DETECT;
    list.dwSize = sizeof(list);
    list.pszConnection = NULL;
    list.dwOptionCount = 1;
    list.dwOptionError = 0;
    list.pOptions = &opt;
    if (InternetSetOptionW(NULL, INTERNET_OPTION_PER_CONNECTION_OPTION, &list, sizeof(list))) printf("SET ok\n");
    else printf("SET fail %lu\n", GetLastError());
    opt.dwOption = INTERNET_PER_CONN_FLAGS;
    opt.Value.dwValue = 0;
    {
        DWORD size = sizeof(list);
        InternetQueryOptionW(NULL, INTERNET_OPTION_PER_CONNECTION_OPTION, &list, &size);
    }
    printf("AFTER %lu\n", opt.Value.dwValue);
    return 0;
}
