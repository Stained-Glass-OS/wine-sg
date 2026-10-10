/* wininet behaviours that Wine's own tests record from real Windows
 * (patches/sg/2423), run by test/wininetground-gate.sh: cookie domains that
 * are public suffixes, the error of a cookie read for a string that is no URL,
 * the proxy flags that cannot go together, the C library's asctime layout in
 * InternetTimeToSystemTime, and DetectAutoProxyUrl. */
#include <windows.h>
#include <wininet.h>
#include <stdio.h>
#include <string.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

typedef BOOL (WINAPI *legal_t)(const WCHAR *, const WCHAR *);

int main(void)
{
    legal_t legal = (legal_t)GetProcAddress(LoadLibraryA("wininet.dll"), (LPCSTR)117);  /* by ordinal */
    BOOL ret;
    DWORD sz;
    char buf[128];
    SYSTEMTIME t;
    static const struct { const WCHAR *domain, *host; int legal; } rows[] =
    {
        { L"gmail.com", L"gmail.com", 1 }, { L"gmail.com", L"www.gmail.com", 1 }, { L"gmail.com", L"mail.gmail.com", 1 },
        { L"gmail.co.uk", L"gmail.co.uk", 1 }, { L"example.com.pl", L"www.aaa.example.com.pl", 1 },
        { L"co.uk", L"gmail.co.uk", 0 }, { L"com.pl", L"www.aaa.example.com.pl", 0 }, { L"com.au", L"shop.com.au", 0 },
        { L"gov.uk", L"www.gov.uk", 0 }, { L"CO.UK", L"gmail.co.uk", 0 },
        { L"gmail.com", L"com", 0 }, { L"com", L"gmail.com", 0 }, { L"uk", L"co.uk", 0 },
        { L".gmail.com", L"mail.gmail.com", 0 }, { L"mail.gmail.com", L"gmail.com", 0 }, { NULL, NULL, 0 },
    };
    unsigned i;
    char msg[160];

    check(legal != NULL, "IsDomainLegalCookieDomainW is exported");
    for (i = 0; legal && i < sizeof(rows) / sizeof(rows[0]); i++)
    {
        ret = legal(rows[i].domain, rows[i].host);
        sprintf(msg, "domain %ls for host %ls is %s", rows[i].domain ? rows[i].domain : L"(null)",
                rows[i].host ? rows[i].host : L"(null)", rows[i].legal ? "legal" : "not legal");
        check(!!ret == rows[i].legal, msg);
    }
    /* cookies */
    check(InternetSetCookieA("http://www.aaa.example.com.pl/bar", NULL, "E=F; domain=example.com.pl"), "a cookie for example.com.pl is taken");
    check(!InternetSetCookieA("http://www.aaa.example.com.pl/bar", NULL, "E=F; domain=com.pl"), "one for com.pl is refused");
    check(!InternetSetCookieA("http://www.example.co.uk/", NULL, "E=F; domain=co.uk"), "and one for co.uk");
    SetLastError(0);
    sz = 0;
    ret = InternetGetCookieW(L"server", NULL, NULL, &sz);
    check(!ret && GetLastError() == ERROR_INTERNET_UNRECOGNIZED_SCHEME, "reading cookies of a string that is no URL: ERROR_INTERNET_UNRECOGNIZED_SCHEME");
    SetLastError(0);
    ret = InternetGetCookieW(NULL, NULL, NULL, &sz);
    check(!ret && (GetLastError() == ERROR_INVALID_PARAMETER || GetLastError() == ERROR_INTERNET_UNRECOGNIZED_SCHEME), "and of no URL at all: an error");

    /* proxy flags */
    {
        HINTERNET ses = InternetOpenA(NULL, INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
        INTERNET_PER_CONN_OPTION_LISTW list;
        INTERNET_PER_CONN_OPTIONW opt;
        DWORD size = sizeof(list);

        memset(&list, 0, sizeof(list));
        list.dwSize = sizeof(list); list.dwOptionCount = 1; list.pOptions = &opt;
        opt.dwOption = INTERNET_PER_CONN_FLAGS; opt.Value.dwValue = PROXY_TYPE_PROXY;
        check(InternetSetOptionW(ses, INTERNET_OPTION_PER_CONNECTION_OPTION, &list, size), "the proxy flag is set");
        opt.Value.dwValue = 0;
        InternetQueryOptionW(ses, INTERNET_OPTION_PER_CONNECTION_OPTION, &list, &size);
        check(opt.Value.dwValue == PROXY_TYPE_PROXY, "and read back");
        opt.dwOption = INTERNET_PER_CONN_FLAGS; opt.Value.dwValue = PROXY_TYPE_PROXY | PROXY_TYPE_DIRECT;
        check(InternetSetOptionW(ses, INTERNET_OPTION_PER_CONNECTION_OPTION, &list, size), "proxy and direct together are accepted");
        opt.Value.dwValue = 0;
        InternetQueryOptionW(ses, INTERNET_OPTION_PER_CONNECTION_OPTION, &list, &size);
        check(opt.Value.dwValue == PROXY_TYPE_DIRECT, "and read back as direct only");
        opt.Value.dwValue = PROXY_TYPE_DIRECT;
        InternetSetOptionW(ses, INTERNET_OPTION_PER_CONNECTION_OPTION, &list, size);
        opt.Value.dwValue = 0;
        InternetQueryOptionW(ses, INTERNET_OPTION_PER_CONNECTION_OPTION, &list, &size);
        check(opt.Value.dwValue == PROXY_TYPE_DIRECT, "direct alone stays direct");
        InternetCloseHandle(ses);
    }

    /* times */
    {
        static const struct { const char *s; WORD d, mo, y, h, mi, se; } times[] =
        {
            { "Fri Jan 7 12:06:35 2005", 7, 1, 2005, 12, 6, 35 },
            { "Fri Jan 7 12:06:35 2005 GMT", 7, 1, 2005, 12, 6, 35 },
            { "sat dec 25 00:00:01 1999 UTC", 25, 12, 1999, 0, 0, 1 },
            { "Fri, 07 Jan 2005 12:06:35 GMT", 7, 1, 2005, 12, 6, 35 },
            { "Fri, 07-01-2005 12:06:35", 7, 1, 2005, 12, 6, 35 },
            { "2, 11-Jan-2022 11:13:05", 11, 1, 2022, 11, 13, 5 },
        };
        for (i = 0; i < sizeof(times) / sizeof(times[0]); i++)
        {
            memset(&t, 0, sizeof(t));
            ret = InternetTimeToSystemTimeA(times[i].s, &t, 0);
            sprintf(msg, "'%s' is %u.%u.%u %u:%02u:%02u", times[i].s, times[i].d, times[i].mo, times[i].y, times[i].h, times[i].mi, times[i].se);
            check(ret && t.wDay == times[i].d && t.wMonth == times[i].mo && t.wYear == times[i].y && t.wHour == times[i].h &&
                  t.wMinute == times[i].mi && t.wSecond == times[i].se, msg);
        }
        memset(&t, 0, sizeof(t));
        InternetTimeToSystemTimeA("Fri Jan 7 12:06:35 2005", &t, 0);
        check(t.wDayOfWeek == 5, "the day of the week of an asctime string comes from its name");
    }

    /* DetectAutoProxyUrl */
    {
        char proxy[256];

        SetLastError(0xdeadbeef);
        memset(proxy, 'x', sizeof(proxy));
        ret = DetectAutoProxyUrl(proxy, sizeof(proxy), PROXY_AUTO_DETECT_TYPE_DHCP);
        check((ret && proxy[0]) || (!ret && GetLastError() == 12180 && !proxy[0]), "DHCP: a URL, or ERROR_WINHTTP_AUTODETECTION_FAILED and an empty one");
        SetLastError(0xdeadbeef);
        memset(proxy, 'x', sizeof(proxy));
        ret = DetectAutoProxyUrl(proxy, sizeof(proxy), PROXY_AUTO_DETECT_TYPE_DNS_A);
        check((ret && proxy[0]) || (!ret && GetLastError() == 12180 && !proxy[0]), "DNS: the same");
        SetLastError(0xdeadbeef);
        check(!DetectAutoProxyUrl(NULL, 10, PROXY_AUTO_DETECT_TYPE_DHCP) && GetLastError() == ERROR_INVALID_PARAMETER, "no buffer: ERROR_INVALID_PARAMETER");
        SetLastError(0xdeadbeef);
        check(!DetectAutoProxyUrl(proxy, sizeof(proxy), 0) && GetLastError() == ERROR_INVALID_PARAMETER, "no method: ERROR_INVALID_PARAMETER");
        SetLastError(0xdeadbeef);
        check(!DetectAutoProxyUrl(proxy, sizeof(proxy), 0x100) && GetLastError() == ERROR_INVALID_PARAMETER, "a method that does not exist: ERROR_INVALID_PARAMETER");
    }
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
