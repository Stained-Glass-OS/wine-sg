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
L"<div id=c1><div id=rm1><b>1</b><i>2</i></div></div>"
L"<div id=c2><div id=rm2><b>1</b></div></div>"
L"<div id=sw><p id=s1>1</p><p id=s2>2</p><p id=s3>3</p></div>"
L"<div id=rp><span id=r1>x</span></div>"
L"<div id=ib><u>0</u></div>"
L"<div id=tx>text</div>"
L"<div id=e1 class=k><b>x</b></div><div id=e2 class=k><b>x</b></div>"
L"<div id=at foo=1></div>"
L"<p id=kids><a>1</a><a>2</a><a>3</a></p>"
L"<fieldset disabled id=fs><div id=inner><span id=deep>d</span></div></fieldset><br id=br>"
L"</body></html>";

static const WCHAR script[] =
L"var r=[]; function t(n,v){r.push(n+'='+v);}"
L"function $(i){return document.getElementById(i);}"
L"function T(n,f){try{f();}catch(e){r.push(n+'=ERR '+(e.number&0xffff)+' '+e.message);}}"
/* removeNode */
L"T('RM',function(){$('rm1').removeNode(false); t('RM0',$('c1').innerHTML);"
L" $('rm2').removeNode(true); t('RM1','['+$('c2').innerHTML+']');});"
/* swapNode */
L"T('SW',function(){$('s1').swapNode($('s3')); t('SW0',$('sw').innerHTML);"
L" $('s1').swapNode($('s2')); t('SW1',$('sw').innerHTML);});"
/* replaceNode */
L"T('RP',function(){var em=document.createElement('em'); var old=$('r1').replaceNode(em); t('RP0',$('rp').innerHTML+' '+(old.id));});"
/* insertBefore with an undefined reference appends */
L"T('IB',function(){var nu=document.createElement('s'); $('ib').insertBefore(nu, undefined); t('IB',$('ib').innerHTML);});"
/* nodeValue of a non-string */
L"T('NV',function(){var tn=$('tx').firstChild; tn.nodeValue=42; t('NV',tn.nodeValue);});"
/* Node3 */
L"T('EQ',function(){t('EQ',$('e1').isEqualNode($('e2'))+' '+$('e1').isEqualNode($('s2')));});"
L"T('SAME',function(){t('SAME',$('e1').isSameNode($('e1'))+' '+$('e1').isSameNode($('e2')));});"
L"T('LOCAL',function(){t('LOCAL',$('e1').localName+' '+$('e1').prefix);});"
L"T('NSLOOKUP',function(){t('NSLOOKUP',$('e1').lookupNamespaceURI(null)+' '+$('e1').isDefaultNamespace('http://www.w3.org/1999/xhtml'));});"
/* hasAttributes / normalize */
L"T('HASATTR',function(){t('HASATTR',$('e1').hasAttributes()+' '+document.createElement('p').hasAttributes());});"
L"T('NORM',function(){var n=document.createElement('div'); n.appendChild(document.createTextNode('a')); n.appendChild(document.createTextNode('b'));"
L" t('NORM0',n.childNodes.length); n.normalize(); t('NORM1',n.childNodes.length+' '+n.firstChild.nodeValue);});"
/* CharacterData */
L"T('CD',function(){var cd=document.createTextNode('hello'); cd.insertData(2,'XX'); t('CD0',cd.data); cd.deleteData(1,2); t('CD1',cd.data);"
L" cd.replaceData(0,1,'J'); t('CD2',cd.data+' '+cd.length+' '+cd.substringData(1,2)); cd.appendData('!'); t('CD3',cd.data);});"
/* attribute collection / nodes */
L"T('SNI',function(){var at=$('at'); var a=document.createAttribute('bar'); a.value='baz'; at.attributes.setNamedItem(a); t('SNI',at.getAttribute('bar'));"
L" var rem=at.attributes.removeNamedItem('bar'); t('RNI',rem.value+' '+at.getAttribute('bar'));"
L" var rem2=at.removeAttributeNode(at.getAttributeNode('foo')); t('RAN',rem2.name+'='+rem2.value+' '+at.hasAttribute('foo'));"
L" t('ATTR',a.nodeType+' '+a.previousSibling+' '+a.nextSibling+' '+(a.ownerDocument==document));});"
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

    /* element properties through the C interfaces */
    {
        IHTMLElement *el; IHTMLElement3 *el3; IHTMLElement2 *el2; VARIANT_BOOL vb; LONG l;
        el = byid(doc3, L"deep");
        IHTMLElement_QueryInterface(el, &IID_IHTMLElement3, (void**)&el3);
        vb = 5; hr = IHTMLElement3_get_isDisabled(el3, &vb); P("ISDIS_DEEP %08lx %d\n", hr, vb);
        vb = 5; hr = IHTMLElement3_get_disabled(el3, &vb); P("DIS_DEEP %d\n", vb);
        IHTMLElement3_Release(el3); IHTMLElement_Release(el);
        el = byid(doc3, L"e1");
        IHTMLElement_QueryInterface(el, &IID_IHTMLElement3, (void**)&el3);
        vb = 5; hr = IHTMLElement3_get_isDisabled(el3, &vb); P("ISDIS_E1 %08lx %d\n", hr, vb);
        vb = 5; hr = IHTMLElement3_get_canHaveHTML(el3, &vb); P("CANHTML_DIV %08lx %d\n", hr, vb);
        hr = IHTMLElement3_put_hideFocus(el3, VARIANT_TRUE); vb = 5; IHTMLElement3_get_hideFocus(el3, &vb); P("HIDEFOCUS %08lx %d", hr, vb);
        hr = IHTMLElement3_put_hideFocus(el3, VARIANT_FALSE); vb = 5; IHTMLElement3_get_hideFocus(el3, &vb); P(" %d\n", vb);
        IHTMLElement3_Release(el3);
        IHTMLElement_QueryInterface(el, &IID_IHTMLElement2, (void**)&el2);
        l = -1; hr = IHTMLElement2_get_readyStateValue(el2, &l); P("RSV %08lx %ld\n", hr, l);
        IHTMLElement2_Release(el2);
        {
            IHTMLElement *pte = NULL; BSTR tn = NULL;
            hr = IHTMLElement_get_parentTextEdit(el, &pte);
            if (pte) IHTMLElement_get_tagName(pte, &tn);
            P("PTE %08lx %ls\n", hr, tn ? tn : L"(none)");
            SysFreeString(tn); if (pte) IHTMLElement_Release(pte);
        }
        IHTMLElement_Release(el);
        el = byid(doc3, L"br");
        IHTMLElement_QueryInterface(el, &IID_IHTMLElement3, (void**)&el3);
        vb = 5; hr = IHTMLElement3_get_canHaveHTML(el3, &vb); P("CANHTML_BR %08lx %d\n", hr, vb);
        IHTMLElement3_Release(el3); IHTMLElement_Release(el);
    }
    /* enumerator clones */
    {
        IHTMLElement *el; IHTMLDOMNode *node; IDispatch *kids; IHTMLDOMChildrenCollection *col; IUnknown *unk;
        IEnumVARIANT *en, *en2; VARIANT v[2]; ULONG got; LONG len;
        el = byid(doc3, L"kids");
        IHTMLElement_QueryInterface(el, &IID_IHTMLDOMNode, (void**)&node);
        IHTMLDOMNode_get_childNodes(node, &kids);
        IDispatch_QueryInterface(kids, &IID_IHTMLDOMChildrenCollection, (void**)&col);
        IHTMLDOMChildrenCollection_get_length(col, &len);
        IHTMLDOMChildrenCollection_get__newEnum(col, &unk);
        IUnknown_QueryInterface(unk, &IID_IEnumVARIANT, (void**)&en);
        got = 0; IEnumVARIANT_Next(en, 1, v, &got); VariantClear(&v[0]);
        hr = IEnumVARIANT_Clone(en, &en2); P("CLONE_NODES %08lx", hr);
        got = 0; IEnumVARIANT_Next(en2, 2, v, &got); P(" %lu", got); VariantClear(&v[0]); if (got > 1) VariantClear(&v[1]);
        got = 0; hr = IEnumVARIANT_Next(en, 2, v, &got); P(" %lu\n", got); VariantClear(&v[0]); if (got > 1) VariantClear(&v[1]);
        IEnumVARIANT_Release(en2); IEnumVARIANT_Release(en); IUnknown_Release(unk);
        IHTMLDOMChildrenCollection_Release(col); IDispatch_Release(kids); IHTMLDOMNode_Release(node); IHTMLElement_Release(el);
        /* attributes enumerator */
        el = byid(doc3, L"e1");
        IHTMLElement_QueryInterface(el, &IID_IHTMLDOMNode, (void**)&node);
        {
            IDispatch *attrs; IHTMLAttributeCollection *ac;
            IHTMLDOMNode_get_attributes(node, &attrs);
            IDispatch_QueryInterface(attrs, &IID_IHTMLAttributeCollection, (void**)&ac);
            IHTMLAttributeCollection_get__newEnum(ac, &unk);
            IUnknown_QueryInterface(unk, &IID_IEnumVARIANT, (void**)&en);
            hr = IEnumVARIANT_Clone(en, &en2); P("CLONE_ATTRS %08lx %d\n", hr, en2 != NULL);
            if (en2) IEnumVARIANT_Release(en2);
            IEnumVARIANT_Release(en); IUnknown_Release(unk); IHTMLAttributeCollection_Release(ac); IDispatch_Release(attrs);
        }
        IHTMLDOMNode_Release(node); IHTMLElement_Release(el);
    }
    P("DONE\n");
    return 0;
}
