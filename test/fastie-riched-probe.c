#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <richedit.h>
#include <richole.h>
#include <tom.h>
#include <stdio.h>
const GUID IID_ITextDocument = {0x8cc497c0,0xa1df,0x11ce,{0x80,0x98,0x00,0xaa,0x00,0x47,0xbe,0x5d}};
static int bad, total;
#define EXPECT_HR(e, c) do { HRESULT h_ = (c); total++; if (h_ != (e)) { bad++; printf("MISMATCH line %d: %s = %lx, expected %lx\n", __LINE__, #c, h_, (HRESULT)(e)); } } while (0)
#define EXPECT(c) do { total++; if (!(c)) { bad++; printf("MISMATCH line %d: %s\n", __LINE__, #c); } } while (0)

int main(void)
{
    HWND edit;
    IRichEditOle *reole = NULL;
    ITextDocument *doc = NULL;
    ITextSelection *sel = NULL;
    ITextRange *range = NULL;
    ITextFont *font = NULL, *dup = NULL;
    LONG v;

    setvbuf(stdout, NULL, _IONBF, 0);
    OleInitialize(NULL);
    LoadLibraryW(L"riched20.dll");
    edit = CreateWindowExW(0, L"RichEdit20W", L"hello world", WS_POPUP | ES_MULTILINE, 0, 0, 200, 100, NULL, NULL, NULL, NULL);
    EXPECT(edit != NULL);
    SendMessageW(edit, EM_GETOLEINTERFACE, 0, (LPARAM)&reole);
    EXPECT(reole != NULL);
    EXPECT_HR(S_OK, IUnknown_QueryInterface((IUnknown *)reole, &IID_ITextDocument, (void **)&doc));

    /* selection flags */
    EXPECT_HR(S_OK, ITextDocument_GetSelection(doc, &sel));
    v = 0xdead;
    EXPECT_HR(S_OK, ITextSelection_GetFlags(sel, &v));
    EXPECT(v == 0);
    EXPECT_HR(E_INVALIDARG, ITextSelection_GetFlags(sel, NULL));
    EXPECT_HR(S_OK, ITextSelection_SetFlags(sel, tomSelStartActive | tomSelAtEOL));
    EXPECT_HR(S_OK, ITextSelection_GetFlags(sel, &v));
    EXPECT(v == (tomSelStartActive | tomSelAtEOL));
    EXPECT_HR(S_OK, ITextSelection_SetFlags(sel, 0));
    EXPECT_HR(S_OK, ITextSelection_GetFlags(sel, &v));
    EXPECT(v == 0);

    /* font style handle */
    EXPECT_HR(S_OK, ITextDocument_Range(doc, 0, 5, &range));
    EXPECT_HR(S_OK, ITextRange_GetFont(range, &font));
    v = 0xdead;
    EXPECT_HR(S_OK, ITextFont_GetStyle(font, &v));
    EXPECT(v == 0);
    EXPECT_HR(E_INVALIDARG, ITextFont_GetStyle(font, NULL));
    EXPECT_HR(S_OK, ITextFont_SetStyle(font, 7));
    EXPECT_HR(S_OK, ITextFont_GetStyle(font, &v));
    EXPECT(v == 7);
    EXPECT_HR(S_OK, ITextFont_GetDuplicate(font, &dup));
    EXPECT_HR(S_OK, ITextFont_GetStyle(dup, &v));
    EXPECT(v == 7);
    ITextFont_Release(dup);

    ITextFont_Release(font);
    ITextRange_Release(range);
    ITextSelection_Release(sel);
    ITextDocument_Release(doc);
    IUnknown_Release((IUnknown *)reole);
    DestroyWindow(edit);
    printf("checks %d, mismatches %d\nRESULT: %s\n", total, bad, bad ? "FAIL" : "PASS");
    return bad != 0;
}
