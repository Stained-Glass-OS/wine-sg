#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <mshtml.h>
#include <stdio.h>
static const IID IID_IHTMLEventObj3_ = {0x3050f680,0x98b5,0x11cf,{0xbb,0x82,0x00,0xaa,0x00,0xbd,0xce,0x0b}};
#define IID_IHTMLEventObj3 IID_IHTMLEventObj3_
static const IID IID_IHTMLEventObj4_ = {0x3050f814,0x98b5,0x11cf,{0xbb,0x82,0x00,0xaa,0x00,0xbd,0xce,0x0b}};
#define IID_IHTMLEventObj4 IID_IHTMLEventObj4_
static const IID IID_IHTMLEventObj5_ = {0x30510478,0x98b5,0x11cf,{0xbb,0x82,0x00,0xaa,0x00,0xbd,0xce,0x0b}};
#define IID_IHTMLEventObj5 IID_IHTMLEventObj5_
static const IID IID_IHTMLEventObj6_ = {0x30510734,0x98b5,0x11cf,{0xbb,0x82,0x00,0xaa,0x00,0xbd,0xce,0x0b}};
#define IID_IHTMLEventObj6 IID_IHTMLEventObj6_
static const IID IID_IHTMLDocument4_ = {0x3050f69a,0x98b5,0x11cf,{0xbb,0x82,0x00,0xaa,0x00,0xbd,0xce,0x0b}};
#define IID_IHTMLDocument4 IID_IHTMLDocument4_
#define P(...) printf(__VA_ARGS__)
static const CLSID CLSID_HTMLDocument_ = {0x25336920,0x03f9,0x11cf,{0x8f,0xd0,0x00,0xaa,0x00,0x68,0x6f,0x13}};


static const WCHAR page[] =
L"<!DOCTYPE html>\n<html><head><meta http-equiv=\"x-ua-compatible\" content=\"IE=11\"></head><body>"
L"<div id=d1>x</div><div id=d2>y</div>"
L"</body></html>";

static const WCHAR script[] =
L"var r=[]; function t(n,v){r.push(n+'='+v);}"
L"function $(i){return document.getElementById(i);}"
L"function T(n,f){try{f();}catch(e){r.push(n+'=ERR '+(e.number&0xffff)+' '+e.message);}}"
L"T('MEV',function(){var e=document.createEvent('MouseEvent'); e.initMouseEvent('click',true,true,window,0,1,2,3,4,false,false,true,false,0,null);"
L" t('MEV0',e.getModifierState('Shift')+' '+e.getModifierState('Control')+' '+e.shiftKey);});"
L"T('KEV',function(){var e=document.createEvent('KeyboardEvent'); e.initKeyboardEvent('keydown',true,true,window,'a',1,'Control Shift',true,'en-US');"
L" t('KEV0',e.getModifierState('Control')+' '+e.getModifierState('Alt')+' '+e.shiftKey+' '+e.key+' '+e.location+' '+e.repeat+' '+e.locale+' '+e.charCode);});"
L"T('SEV',function(){var e=document.createEvent('StorageEvent'); e.initStorageEvent('storage',false,false,'k','o','n','http://x/',window.sessionStorage);"
L" t('SEV0',e.key+' '+e.oldValue+' '+e.newValue+' '+e.url+' '+(e.storageArea===window.sessionStorage));});"
L"T('OFF',function(){var d=$('d1'),got; d.addEventListener('click',function(e){got=e;}); var e=document.createEvent('MouseEvent');"
L" e.initMouseEvent('click',true,true,window,0,0,0,50,40,false,false,false,false,0,null); d.dispatchEvent(e);"
L" var r=d.getBoundingClientRect(); t('OFF0',got.offsetX==50-r.left-d.clientLeft && got.offsetY==40-r.top-d.clientTop);});"
L"t('MODE',document.documentMode);"
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

#define LONGPROP(NAME, put, get) do { \
        hr = IHTMLEventObj2_put_##put(ev2, v); l = -1; hr2 = IHTMLEventObj_get_##get(ev, &l); \
        l2 = -2; IHTMLEventObj2_get_##get(ev2, &l2); \
        P("%s %08lx %08lx %ld %ld\n", NAME, hr, hr2, l, l2); } while(0)

int main(void)
{
    IHTMLDocument2 *doc; IHTMLDocument3 *doc3; IHTMLDocument4 *doc4 = NULL; IHTMLWindow2 *win; HRESULT hr, hr2;
    IHTMLEventObj *ev = NULL; IHTMLEventObj2 *ev2 = NULL; IHTMLEventObj3 *ev3 = NULL; IHTMLEventObj4 *ev4 = NULL;
    IHTMLEventObj5 *ev5 = NULL; IHTMLEventObj6 *ev6 = NULL;
    VARIANT res, none; BSTR b, lang; LONG l, l2; VARIANT_BOOL vb, vb2; LONG_PTR lp;
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
    IHTMLDocument2_QueryInterface(doc, &IID_IHTMLDocument4, (void**)&doc4);
    IHTMLDocument2_get_parentWindow(doc, &win);
    b = SysAllocString(script); lang = SysAllocString(L"jscript");
    VariantInit(&res);
    hr = IHTMLWindow2_execScript(win, b, lang, &res);
    P("SCRIPT %08lx\n", hr);
    SysFreeString(b); SysFreeString(lang);
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

    /* the legacy event object: a standalone one has no DOM event behind it */
    V_VT(&none) = VT_ERROR; V_ERROR(&none) = DISP_E_PARAMNOTFOUND;
    hr = IHTMLDocument4_createEventObject(doc4, &none, &ev);
    P("EVOBJ %08lx %d\n", hr, ev != NULL);
    IHTMLEventObj_QueryInterface(ev, &IID_IHTMLEventObj2, (void**)&ev2);
    IHTMLEventObj_QueryInterface(ev, &IID_IHTMLEventObj3, (void**)&ev3);
    IHTMLEventObj_QueryInterface(ev, &IID_IHTMLEventObj4, (void**)&ev4);
    IHTMLEventObj_QueryInterface(ev, &IID_IHTMLEventObj5, (void**)&ev5);
    IHTMLEventObj_QueryInterface(ev, &IID_IHTMLEventObj6, (void**)&ev6);
    P("IFACES %d %d %d %d %d\n", ev2 != NULL, ev3 != NULL, ev4 != NULL, ev5 != NULL, ev6 != NULL);
    {
        LONG kc = -1;
        hr = IHTMLEventObj_put_keyCode(ev, 65); IHTMLEventObj_get_keyCode(ev, &kc);
        P("KEYCODE %08lx %ld\n", hr, kc);
    }
    { LONG v = 11; LONGPROP("BUTTON", button, button); }
    { LONG v = 12; LONGPROP("REASON", reason, reason); }
    { LONG v = 13; LONGPROP("X", x, x); }
    { LONG v = 14; LONGPROP("Y", y, y); }
    { LONG v = 15; LONGPROP("CLIENTX", clientX, clientX); }
    { LONG v = 16; LONGPROP("CLIENTY", clientY, clientY); }
    { LONG v = 17; LONGPROP("OFFSETX", offsetX, offsetX); }
    { LONG v = 18; LONGPROP("OFFSETY", offsetY, offsetY); }
    { LONG v = 19; LONGPROP("SCREENX", screenX, screenX); }
    { LONG v = 20; LONGPROP("SCREENY", screenY, screenY); }
    vb = vb2 = 5;
    hr = IHTMLEventObj2_put_altKey(ev2, VARIANT_TRUE); hr2 = IHTMLEventObj2_put_ctrlKey(ev2, VARIANT_TRUE);
    IHTMLEventObj_get_altKey(ev, &vb); IHTMLEventObj_get_ctrlKey(ev, &vb2);
    P("MODS %08lx %08lx %d %d", hr, hr2, vb, vb2);
    IHTMLEventObj2_put_shiftKey(ev2, VARIANT_TRUE); IHTMLEventObj_get_shiftKey(ev, &vb);
    IHTMLEventObj2_put_altKey(ev2, VARIANT_FALSE); IHTMLEventObj2_get_altKey(ev2, &vb2);
    P(" %d %d\n", vb, vb2);
    vb = 5; IHTMLEventObj2_put_repeat(ev2, VARIANT_TRUE); hr = IHTMLEventObj2_get_repeat(ev2, &vb); P("REPEAT %08lx %d\n", hr, vb);
    vb = 5; hr = IHTMLEventObj3_get_contentOverflow(ev3, &vb); P("OVERFLOW %08lx %d\n", hr, vb);
    IHTMLEventObj3_put_shiftLeft(ev3, VARIANT_TRUE); IHTMLEventObj3_put_altLeft(ev3, VARIANT_FALSE); IHTMLEventObj3_put_ctrlLeft(ev3, VARIANT_TRUE);
    P("LEFT"); IHTMLEventObj3_get_shiftLeft(ev3, &vb); P(" %d", vb); IHTMLEventObj3_get_altLeft(ev3, &vb); P(" %d", vb);
    IHTMLEventObj3_get_ctrlLeft(ev3, &vb); P(" %d\n", vb);
    P("IME"); hr = IHTMLEventObj3_get_imeCompositionChange(ev3, &lp); P(" %08lx %ld", hr, (long)lp);
    hr = IHTMLEventObj3_get_imeRequest(ev3, &lp); P(" %08lx %ld", hr, (long)lp);
    hr = IHTMLEventObj3_get_keyboardLayout(ev3, &lp); P(" %08lx %ld\n", hr, (long)lp);
    l = -1; l2 = -1; hr = IHTMLEventObj3_get_behaviorCookie(ev3, &l); hr2 = IHTMLEventObj3_get_behaviorPart(ev3, &l2);
    P("BEHAVIOR %08lx %08lx %ld %ld\n", hr, hr2, l, l2);
    l = -1; hr = IHTMLEventObj4_get_wheelDelta(ev4, &l); P("WHEEL %08lx %ld\n", hr, l);
    b = (BSTR)1; hr = IHTMLEventObj6_get_actionURL(ev6, &b); l = -1; hr2 = IHTMLEventObj6_get_buttonID(ev6, &l);
    P("EV6 %08lx %08lx %d %ld\n", hr, hr2, b == NULL, l); SysFreeString(b);
    {
        IHTMLDataTransfer *dt = (IHTMLDataTransfer*)1;
        hr = IHTMLEventObj2_get_dataTransfer(ev2, &dt); P("DATATRANSFER %08lx %d\n", hr, dt == NULL);
    }
    /* strings */
    {
        BSTR s = SysAllocString(L"mytype"), q = NULL;
        hr = IHTMLEventObj2_put_type(ev2, s); hr2 = IHTMLEventObj_get_type(ev, &q);
        P("TYPE %08lx %08lx %ls\n", hr, hr2, q ? q : L"(null)"); SysFreeString(q); SysFreeString(s);
        s = SysAllocString(L"qual"); q = NULL;
        hr = IHTMLEventObj2_put_qualifier(ev2, s); hr2 = IHTMLEventObj_get_qualifier(ev, &q);
        P("QUALIFIER %08lx %08lx %ls\n", hr, hr2, q ? q : L"(null)"); SysFreeString(q); SysFreeString(s);
        s = SysAllocString(L"urn:x"); q = NULL;
        hr = IHTMLEventObj2_put_srcUrn(ev2, s); hr2 = IHTMLEventObj2_get_srcUrn(ev2, &q);
        P("SRCURN %08lx %08lx %ls\n", hr, hr2, q ? q : L"(null)"); SysFreeString(q); SysFreeString(s);
        s = SysAllocString(L"fld"); q = NULL;
        hr = IHTMLEventObj2_put_dataFld(ev2, s); hr2 = IHTMLEventObj2_get_dataFld(ev2, &q);
        P("DATAFLD %08lx %08lx %ls\n", hr, hr2, q ? q : L"(null)"); SysFreeString(q); SysFreeString(s);
        s = SysAllocString(L"prop"); q = NULL;
        hr = IHTMLEventObj2_put_propertyName(ev2, s); hr2 = IHTMLEventObj2_get_propertyName(ev2, &q);
        P("PROPNAME %08lx %08lx %ls\n", hr, hr2, q ? q : L"(null)"); SysFreeString(q); SysFreeString(s);
        s = SysAllocString(L"payload"); q = NULL;
        hr = IHTMLEventObj5_put_data(ev5, s); hr2 = IHTMLEventObj5_get_data(ev5, &q);
        P("DATA %08lx %08lx %ls\n", hr, hr2, q ? q : L"(null)"); SysFreeString(q); SysFreeString(s);
        vb = 5; hr = IHTMLEventObj5_put_issession(ev5, VARIANT_TRUE); hr2 = IHTMLEventObj5_get_issession(ev5, &vb);
        P("ISSESSION %08lx %08lx %d\n", hr, hr2, vb);
    }
    /* objects */
    {
        IHTMLElement *d1 = byid(doc3, L"d1"), *d2 = byid(doc3, L"d2"), *out = NULL; BSTR id = NULL;
        hr = ev2->lpVtbl->putref_srcElement(ev2, d1); hr2 = IHTMLEventObj_get_srcElement(ev, &out);
        if (out) IHTMLElement_get_id(out, &id);
        P("SRCELEM %08lx %08lx %ls\n", hr, hr2, id ? id : L"(null)"); SysFreeString(id); if (out) IHTMLElement_Release(out);
        out = NULL; id = NULL;
        hr = ev2->lpVtbl->putref_toElement(ev2, d2); hr2 = IHTMLEventObj_get_toElement(ev, &out);
        if (out) IHTMLElement_get_id(out, &id);
        P("TOELEM %08lx %08lx %ls\n", hr, hr2, id ? id : L"(null)"); SysFreeString(id); if (out) IHTMLElement_Release(out);
        out = NULL; id = NULL;
        hr = ev2->lpVtbl->putref_fromElement(ev2, d1); hr2 = IHTMLEventObj2_get_fromElement(ev2, &out);
        if (out) IHTMLElement_get_id(out, &id);
        P("FROMELEM %08lx %08lx %ls\n", hr, hr2, id ? id : L"(null)"); SysFreeString(id); if (out) IHTMLElement_Release(out);
        out = NULL;
        hr = ev2->lpVtbl->putref_srcElement(ev2, NULL); hr2 = IHTMLEventObj_get_srcElement(ev, &out);
        P("SRCNULL %08lx %08lx %d\n", hr, hr2, out == NULL); if (out) IHTMLElement_Release(out);
        {
            IDispatch *disp = NULL;
            hr = ev2->lpVtbl->putref_srcFilter(ev2, (IDispatch*)d1); hr2 = IHTMLEventObj_get_srcFilter(ev, &disp);
            P("SRCFILTER %08lx %08lx %d\n", hr, hr2, disp == (IDispatch*)d1); if (disp) IDispatch_Release(disp);
            disp = NULL;
            hr = ev2->lpVtbl->putref_recordset(ev2, (IDispatch*)d2); hr2 = IHTMLEventObj2_get_recordset(ev2, &disp);
            P("RECORDSET %08lx %08lx %d\n", hr, hr2, disp == (IDispatch*)d2); if (disp) IDispatch_Release(disp);
        }
        IHTMLElement_Release(d1); IHTMLElement_Release(d2);
    }
    /* named attributes */
    {
        BSTR n = SysAllocString(L"Foo"), n2 = SysAllocString(L"foo"); VARIANT v, out; VARIANT_BOOL ok;
        V_VT(&v) = VT_I4; V_I4(&v) = 77;
        hr = IHTMLEventObj2_setAttribute(ev2, n, v, 0);
        VariantInit(&out); hr2 = IHTMLEventObj2_getAttribute(ev2, n2, 0, &out);
        P("ATTR0 %08lx %08lx %d %ld\n", hr, hr2, V_VT(&out), V_VT(&out) == VT_I4 ? V_I4(&out) : 0);
        VariantInit(&out); hr2 = IHTMLEventObj2_getAttribute(ev2, n2, 1, &out);
        P("ATTR1 %08lx %d\n", hr2, V_VT(&out));
        ok = 5; hr = IHTMLEventObj2_removeAttribute(ev2, n2, 1, &ok); P("ATTR2 %08lx %d", hr, ok);
        ok = 5; hr = IHTMLEventObj2_removeAttribute(ev2, n2, 0, &ok); P(" %08lx %d", hr, ok);
        ok = 5; hr = IHTMLEventObj2_removeAttribute(ev2, n2, 0, &ok); P(" %08lx %d\n", hr, ok);
        VariantInit(&out); hr2 = IHTMLEventObj2_getAttribute(ev2, n, 0, &out); P("ATTR3 %08lx %d\n", hr2, V_VT(&out));
        SysFreeString(n); SysFreeString(n2);
    }
    IHTMLEventObj6_Release(ev6); IHTMLEventObj5_Release(ev5); IHTMLEventObj4_Release(ev4);
    IHTMLEventObj3_Release(ev3); IHTMLEventObj2_Release(ev2); IHTMLEventObj_Release(ev);
    P("DONE\n");
    return 0;
}
