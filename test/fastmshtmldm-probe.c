#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <mshtml.h>
#include <stdio.h>
static const CLSID CLSID_HTMLDocument_ = {0x25336920,0x03f9,0x11cf,{0x8f,0xd0,0x00,0xaa,0x00,0x68,0x6f,0x13}};
static const char page[] = "<!DOCTYPE html><html><head><meta http-equiv=\"x-ua-compatible\" content=\"IE=11\"></head><body><div id=a>x</div></body></html>";
static const WCHAR script1[] =
L"var o=[]; document.designMode='on'; o.push(document.designMode);"
L"o.push(document.queryCommandEnabled('bold'));"
L"var a=document.getElementById('a'); var r=document.body.createTextRange(); r.moveToElementText(a); r.select();"
L"o.push(document.execCommand('bold',false,null)); o.push(a.innerHTML.toLowerCase().replace('strong','b'));"
L"document.designMode='off'; o.push(document.designMode);"
L"document.title=o.join('|');";
static void pump(DWORD ms){DWORD end=GetTickCount()+ms;MSG m;while(GetTickCount()<end){while(PeekMessageW(&m,NULL,0,0,PM_REMOVE)){TranslateMessage(&m);DispatchMessageW(&m);}Sleep(10);}}
int main(void)
{
    IHTMLDocument2 *doc; IHTMLWindow2 *w; VARIANT r; HRESULT hr; BSTR b = NULL, s, l;
    setvbuf(stdout, NULL, _IONBF, 0);
    CoInitialize(NULL);
    CoCreateInstance(&CLSID_HTMLDocument_, NULL, CLSCTX_INPROC_SERVER, &IID_IHTMLDocument2, (void**)&doc);
    {
        IPersistStreamInit *psi; IStream *st; HGLOBAL m = GlobalAlloc(0, sizeof page);
        memcpy(GlobalLock(m), page, sizeof page); GlobalUnlock(m);
        CreateStreamOnHGlobal(m, TRUE, &st);
        IHTMLDocument2_QueryInterface(doc, &IID_IPersistStreamInit, (void**)&psi);
        IPersistStreamInit_InitNew(psi); IPersistStreamInit_Load(psi, st);
    }
    pump(1500);
    IHTMLDocument2_get_parentWindow(doc, &w);
    s = SysAllocString(script1); l = SysAllocString(L"JScript"); VariantInit(&r);
    hr = IHTMLWindow2_execScript(w, s, l, &r);
    printf("EXEC=%08lx\n", hr);
    pump(800);
    IHTMLDocument2_get_title(doc, &b);
    printf("TITLE=%ls\n", b ? b : L"-");
    /* the host-facing object reflects the state too */
    b = SysAllocString(L"on"); hr = IHTMLDocument2_put_designMode(doc, b); printf("HOSTON=%08lx\n", hr);
    pump(1500);
    b = NULL; hr = IHTMLDocument2_get_designMode(doc, &b); printf("HOSTGET=%08lx %ls\n", hr, b ? b : L"-");
    printf("DONE\n");
    return 0;
}
