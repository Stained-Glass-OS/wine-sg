#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <mshtml.h>
#include <stdio.h>

static const CLSID CLSID_HTMLDocument_ = {0x25336920,0x03f9,0x11cf,{0x8f,0xd0,0x00,0xaa,0x00,0x68,0x6f,0x13}};
static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static void pump(DWORD ms)
{
    DWORD end = GetTickCount() + ms; MSG msg;
    while (GetTickCount() < end) { while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); } Sleep(10); }
}

int main(void)
{
    IHTMLDocument2 *doc; IPersistFile *pf; HRESULT hr; LPOLESTR cur = NULL;
    WCHAR path[MAX_PATH]; HANDLE f; DWORD w; const char html[] = "<html><body>hello file</body></html>";
    IHTMLElement *body = NULL; BSTR text = NULL;

    CoInitialize(NULL);
    GetFullPathNameW(L"loadme.htm", MAX_PATH, path, NULL);
    f = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    WriteFile(f, html, sizeof(html) - 1, &w, NULL); CloseHandle(f);

    hr = CoCreateInstance(&CLSID_HTMLDocument_, NULL, CLSCTX_INPROC_SERVER, &IID_IHTMLDocument2, (void **)&doc);
    CHECK(hr == S_OK, "doc %08lx", hr);
    hr = IHTMLDocument2_QueryInterface(doc, &IID_IPersistFile, (void **)&pf);
    CHECK(hr == S_OK, "IPersistFile %08lx", hr);

    hr = IPersistFile_GetCurFile(pf, &cur);
    CHECK(hr == S_FALSE && cur && !wcscmp(cur, L"*.htm"), "no file yet: %08lx %ls", hr, cur ? cur : L"(null)");
    CoTaskMemFree(cur); cur = NULL;
    CHECK(IPersistFile_SaveCompleted(pf, NULL) == S_OK, "SaveCompleted");

    hr = IPersistFile_Load(pf, path, STGM_READ);
    CHECK(hr == S_OK, "Load %08lx", hr);
    pump(2500);
    hr = IHTMLDocument2_get_body(doc, &body);
    if (body) { IHTMLElement_get_innerText(body, &text); IHTMLElement_Release(body); }
    CHECK(text && wcsstr(text, L"hello file"), "body text %ls", text ? text : L"(null)");
    SysFreeString(text);

    hr = IPersistFile_GetCurFile(pf, &cur);
    CHECK(hr == S_OK && cur && !wcsicmp(cur, path), "cur file %08lx %ls", hr, cur ? cur : L"(null)");
    CoTaskMemFree(cur);

    IPersistFile_Release(pf); IHTMLDocument2_Release(doc);
    printf(fails ? "RESULT: FAIL\n" : "RESULT: PASS\n");
    return fails != 0;
}
