#include <windows.h>
#include <werapi.h>
#include <stdio.h>

static int failures, checks;
#define CHECK(c, ...) do { checks++; if (!(c)) { failures++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

int main(void)
{
    HREPORT r = NULL;
    WER_SUBMIT_RESULT res = 0;
    HRESULT hr;
    WCHAR big[300];
    int i;

    hr = WerReportCreate(L"APPCRASH", WerReportCritical, NULL, &r);
    CHECK(hr == S_OK && r, "create");

    for (i = 0; i < 10; i++)
    {
        hr = WerReportSetParameter(r, i, L"Name", L"Value");
        CHECK(hr == S_OK, "param %d %lx", i, hr);
    }
    CHECK(WerReportSetParameter(r, 10, L"Name", L"Value") == E_INVALIDARG, "param id 10");
    CHECK(WerReportSetParameter(r, 0, NULL, L"Value") == S_OK, "no name");
    CHECK(WerReportSetParameter(r, 0, L"Name", NULL) == E_INVALIDARG, "no value");
    for (i = 0; i < 299; i++) big[i] = 'x';
    big[299] = 0;
    CHECK(WerReportSetParameter(r, 1, L"Name", big) == E_INVALIDARG, "value too long");
    CHECK(WerReportSetParameter((HREPORT)0x1234, 1, L"Name", L"v") == E_INVALIDARG, "bad handle");

    hr = WerReportAddFile(r, L"C:\\windows\\system.ini", WerFileTypeOther, 0);
    CHECK(hr == S_OK, "add file %lx", hr);
    CHECK(WerReportAddFile(r, NULL, WerFileTypeOther, 0) == E_INVALIDARG, "null path");
    CHECK(WerReportAddFile(r, L"C:\\x", 0, 0) == E_INVALIDARG, "type 0");
    CHECK(WerReportAddFile(r, L"C:\\x", WerFileTypeMax, 0) == E_INVALIDARG, "type max");
    CHECK(WerReportAddFile((HREPORT)0x1234, L"C:\\x", WerFileTypeOther, 0) == E_INVALIDARG, "bad handle file");
    for (i = 1; i < 16; i++) WerReportAddFile(r, L"C:\\windows\\win.ini", WerFileTypeOther, 0);
    CHECK(WerReportAddFile(r, L"C:\\windows\\win.ini", WerFileTypeOther, 0) != S_OK, "17th file refused");

    hr = WerReportAddDump(r, GetCurrentProcess(), NULL, WerDumpTypeMiniDump, NULL, NULL, 0);
    CHECK(hr == S_OK, "add dump %lx", hr);
    CHECK(WerReportAddDump(r, NULL, NULL, WerDumpTypeMiniDump, NULL, NULL, 0) == E_INVALIDARG, "dump no process");
    CHECK(WerReportAddDump(r, GetCurrentProcess(), NULL, 0, NULL, NULL, 0) == E_INVALIDARG, "dump type 0");
    CHECK(WerReportAddDump(r, GetCurrentProcess(), NULL, WerDumpTypeMax, NULL, NULL, 0) == E_INVALIDARG, "dump type max");
    CHECK(WerReportAddDump((HREPORT)0x1234, GetCurrentProcess(), NULL, WerDumpTypeMiniDump, NULL, NULL, 0) == E_INVALIDARG, "dump bad handle");

    CHECK(WerReportSetUIOption(r, WerUICloseText, L"Close it") == S_OK, "ui option");
    CHECK(WerReportSetUIOption(r, 0, L"x") == E_INVALIDARG, "ui 0");
    CHECK(WerReportSetUIOption(r, WerUIMax, L"x") == E_INVALIDARG, "ui max");
    CHECK(WerReportSetUIOption(r, WerUICloseText, NULL) == E_INVALIDARG, "ui null");
    CHECK(WerReportSetUIOption((HREPORT)0x1234, WerUICloseText, L"x") == E_INVALIDARG, "ui bad handle");

    hr = WerReportSubmit(r, WerConsentNotAsked, 0, &res);
    CHECK(hr == S_OK && res == WerDisabled, "submit %lx %d", hr, res);
    CHECK(WerReportSubmit(r, WerConsentNotAsked, 0, NULL) == E_INVALIDARG, "submit null result");
    CHECK(WerReportSubmit(r, 0, 0, &res) == E_INVALIDARG, "consent 0");
    CHECK(WerReportSubmit(r, WerConsentMax, 0, &res) == E_INVALIDARG, "consent max");
    CHECK(WerReportSubmit((HREPORT)0x1234, WerConsentNotAsked, 0, &res) == E_INVALIDARG, "submit bad handle");

    CHECK(WerReportCloseHandle(r) == S_OK, "close");
    CHECK(WerReportSetParameter(r, 0, L"n", L"v") == E_INVALIDARG, "use after close");
    CHECK(WerReportAddFile(r, L"C:\\x", WerFileTypeOther, 0) == E_INVALIDARG, "file after close");

    {
        WER_REPORT_INFORMATION info;
        HREPORT r2;
        memset(&info, 0, sizeof(info));
        info.dwSize = sizeof(info);
        lstrcpyW(info.wzApplicationName, L"MyApp");
        hr = WerReportCreate(L"MyEvent", WerReportNonCritical, &info, &r2);
        CHECK(hr == S_OK, "create with info");
        WerReportCloseHandle(r2);
    }
    printf("%d checks, %d failures\n", checks, failures);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
