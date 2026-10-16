#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <mshtml.h>
#include <stdio.h>
static const IID IID_IHTMLElement3_ = {0x3050f673,0x98b5,0x11cf,{0xbb,0x82,0x00,0xaa,0x00,0xbd,0xce,0x0b}};
#define IID_IHTMLElement3 IID_IHTMLElement3_
#define P(...) printf(__VA_ARGS__)
static const CLSID CLSID_HTMLDocument_ = {0x25336920,0x03f9,0x11cf,{0x8f,0xd0,0x00,0xaa,0x00,0x68,0x6f,0x13}};

static const WCHAR page[] =
L"<!DOCTYPE html>\n<html><head><meta http-equiv=\"x-ua-compatible\" content=\"IE=11\"></head><body>"
L"<div id=a title=t1><span id=s1>one</span></div><div id=b><p id=p1>x</p></div><div id=c></div><div id=d></div>"
L"</body></html>";

static const WCHAR script1[] =
L"var r=[]; function t(n,v){r.push(n+'='+v);}"
L"function T(n,f){try{f();}catch(e){r.push(n+'=ERR '+(e.number&0xffff)+' '+e.message);}}"
L"T('A0',function(){var u=new Uint8Array(4); t('A0',u.length+' '+u.byteLength+' '+u.byteOffset+' '+u[0]+' '+u.BYTES_PER_ELEMENT+' '+Uint8Array.BYTES_PER_ELEMENT+' '+(u.buffer instanceof ArrayBuffer)+' '+u.buffer.byteLength);});"
L"T('A1',function(){var u=new Uint8Array(4); u[0]=257; u[1]=-1; u[2]=3.9; u[3]='7'; u[9]=5; t('A1',[u[0],u[1],u[2],u[3],u[9]].join(',')+' '+u.length);});"
L"T('A2',function(){var u=new Uint8ClampedArray(5); u[0]=300; u[1]=-5; u[2]=1.5; u[3]=2.5; u[4]=NaN; t('A2',[].join.call(u,','));});"
L"T('A3',function(){var i=new Int8Array([127,128,255,-129]); var s=new Int16Array([32768,-32769,65535]); var u=new Uint16Array([-1,65536,65537]); t('A3',[].join.call(i,',')+'|'+[].join.call(s,',')+'|'+[].join.call(u,','));});"
L"T('A4',function(){var i=new Int32Array([2147483648,-1,4294967296,1.9,-1.9]); var u=new Uint32Array([-1,4294967296,4294967297,2147483648]); t('A4',[].join.call(i,',')+'|'+[].join.call(u,','));});"
L"T('A5',function(){var f=new Float32Array([0.1,16777217,1e40,-0]); var d=new Float64Array([0.1,1/3]); t('A5',f[0]+' '+f[1]+' '+f[2]+' '+(1/f[3])+'|'+d[0]+' '+d[1]);});"
L"T('B0',function(){var buf=new ArrayBuffer(8); var a=new Uint8Array(buf); var b=new Uint32Array(buf); b[0]=0x01020304; t('B0',[a[0],a[1],a[2],a[3]].join(',')+' '+b.length+' '+(a.buffer===buf)); var c=new Uint16Array(buf,2,2); t('B1',c.length+' '+c.byteOffset+' '+c.byteLength+' '+c[0]); var d=new Uint8Array(buf,4); t('B2',d.length+' '+d.byteOffset);});"
L"T('C0',function(){var u=new Uint8Array([1,2,3,4,5]); var s=u.subarray(1,4); t('C0',s.length+' '+s.byteOffset+' '+[].join.call(s,',')); s[0]=9; t('C1',u[1]+' '+u.subarray(-2).length+' '+u.subarray(2).length+' '+u.subarray(3,1).length);});"
L"T('C2',function(){var u=new Uint8Array(6); u.set([1,2,3]); u.set([7,8],4); u.set(new Uint8Array([5]),3); t('C2',[].join.call(u,',')); try{u.set([1,2,3],5); t('C3','no error');}catch(e){t('C3','error');}});"
L"T('D0',function(){var u=new Uint8Array([1,2]); t('D0',Object.prototype.toString.call(u)+' '+Object.prototype.toString.call(new Float64Array(1))+' '+ArrayBuffer.isView(u)+' '+ArrayBuffer.isView(new DataView(new ArrayBuffer(2)))+' '+ArrayBuffer.isView([]));});"
L"T('D1',function(){var u=new Uint16Array([5,6,7]); var k=[]; for(var i in u) k.push(i); t('D1',k.join(',')+' '+Array.prototype.slice.call(u).join(',')+' '+(u instanceof Uint16Array)+' '+(u.constructor===Uint16Array)+' '+Array.isArray(u));});"
L"T('D2',function(){var u=new Uint8Array(new Uint16Array([258,3])); t('D2',[].join.call(u,',')+' '+u.length); var e=new Float32Array(0); t('D3',e.length+' '+e.byteLength);});"
L"T('E0',function(){try{new Int32Array(new ArrayBuffer(8),1);t('E0','no error');}catch(e){t('E0','error');} try{new Int32Array(new ArrayBuffer(7));t('E1','no error');}catch(e){t('E1','error');} try{new Uint8Array(-1);t('E2','no error');}catch(e){t('E2','error');}});"
L"document.title=r.join('|');";

static void pump(DWORD ms)
{
    DWORD end = GetTickCount() + ms;
    MSG msg;
    while (GetTickCount() < end) {
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
        Sleep(10);
    }
}

static IHTMLElement *byid(IHTMLDocument3 *d3, const WCHAR *id)
{
    IHTMLElement *e = NULL; BSTR b = SysAllocString(id);
    IHTMLDocument3_getElementById(d3, b, &e); SysFreeString(b);
    return e;
}

int main(void)
{
    IHTMLDocument2 *doc; IHTMLDocument3 *doc3; IHTMLWindow2 *win; HRESULT hr;
    VARIANT res; BSTR b, lang;
    setvbuf(stdout, NULL, _IONBF, 0);
    CoInitialize(NULL);
    hr = CoCreateInstance(&CLSID_HTMLDocument_,NULL,CLSCTX_INPROC_SERVER,&IID_IHTMLDocument2,(void**)&doc);
    P("DOC %08lx\n", hr);
    {
        IPersistStreamInit *psi; IStream *stream; HGLOBAL mem; char *ascii; int len;
        len = WideCharToMultiByte(CP_ACP, 0, page, -1, NULL, 0, NULL, NULL) - 1;
        mem = GlobalAlloc(0, len); ascii = GlobalLock(mem);
        WideCharToMultiByte(CP_ACP, 0, page, -1, ascii, len, NULL, NULL); GlobalUnlock(mem);
        CreateStreamOnHGlobal(mem, TRUE, &stream);
        IHTMLDocument2_QueryInterface(doc, &IID_IPersistStreamInit, (void**)&psi);
        IPersistStreamInit_InitNew(psi);
        IPersistStreamInit_Load(psi, stream);
        IPersistStreamInit_Release(psi); IStream_Release(stream);
    }
    pump(1500);
    IHTMLDocument2_QueryInterface(doc, &IID_IHTMLDocument3, (void**)&doc3);
    IHTMLDocument2_get_parentWindow(doc, &win);
    b = SysAllocString(script1); lang = SysAllocString(L"jscript");
    VariantInit(&res);
    hr = IHTMLWindow2_execScript(win, b, lang, &res);
    P("SCRIPT %08lx\n", hr);
    SysFreeString(b);
    pump(100);
    SysFreeString(lang);
    b = NULL; IHTMLDocument2_get_title(doc, &b);
    if (b) {
        WCHAR *p = b, *q;
        while (p && *p) {
            q = wcschr(p, '|');
            if (q) *q = 0;
            P("%ls\n", p);
            p = q ? q + 1 : NULL;
        }
    }
    SysFreeString(b);
    P("DONE\n");
    return 0;
}
