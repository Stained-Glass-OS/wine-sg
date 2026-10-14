/* riched20 TOM text ranges by word, paragraph and story (patch 3690) */
#define COBJMACROS
#include <windows.h>
#include <richedit.h>
#include <richole.h>
#include <tom.h>
#include <stdio.h>

static const GUID IID_ITextDocument_ = {0x8cc497c0, 0xa1df, 0x11ce, {0x80, 0x98, 0x00, 0xaa, 0x00, 0x47, 0xbe, 0x5d}};
#define IID_ITextDocument IID_ITextDocument_
static int failed;
#define CHECK(c, ...) do { if (!(c)) { failed++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static ITextDocument *doc;

static ITextRange *range(LONG a, LONG b)
{
    ITextRange *r = NULL;
    ITextDocument_Range(doc, a, b, &r);
    return r;
}

static void expect(ITextRange *r, LONG s, LONG e, const char *what)
{
    LONG a = -1, b = -1;
    ITextRange_GetStart(r, &a);
    ITextRange_GetEnd(r, &b);
    CHECK(a == s && b == e, "%s: [%ld,%ld] want [%ld,%ld]", what, a, b, s, e);
}

int main(void)
{
    HWND hwnd;
    IRichEditOle *ole = NULL;
    ITextRange *r;
    LONG d, len;
    HRESULT hr;

    LoadLibraryA("riched20.dll");
    CoInitialize(NULL);
    hwnd = CreateWindowExW(0, L"RichEdit20W", NULL, WS_POPUP | ES_MULTILINE, 0, 0, 300, 200, NULL, NULL, NULL, NULL);
    SetWindowTextW(hwnd, L"Hello big world.\rSecond  para here\rThird");
    SendMessageW(hwnd, EM_GETOLEINTERFACE, 0, (LPARAM)&ole);
    ole->lpVtbl->QueryInterface(ole, &IID_ITextDocument, (void **)&doc);
    CHECK(doc != NULL, "document");
    if (!doc) return 1;
    ITextRange_GetStoryLength((r = range(0, 0)), &len);
    ITextRange_Release(r);
    CHECK(len == 42 || len > 38, "story length %ld", len);

    r = range(0, 0); d = 99;
    hr = ITextRange_Move(r, tomWord, 1, &d);
    CHECK(hr == S_OK && d == 1, "Move word hr %#lx d %ld", hr, d); expect(r, 6, 6, "Move word 1");
    hr = ITextRange_Move(r, tomWord, 2, &d);
    CHECK(d == 2, "Move 2 words d %ld", d); expect(r, 15, 15, "Move word 2");
    hr = ITextRange_Move(r, tomWord, -3, &d);
    CHECK(d == -3, "Move back 3 words d %ld", d); expect(r, 0, 0, "Move word -3");
    hr = ITextRange_Move(r, tomWord, -1, &d);
    CHECK(hr == S_FALSE && d == 0, "Move before the start hr %#lx d %ld", hr, d);
    ITextRange_Release(r);

    r = range(2, 2); d = 99;
    hr = ITextRange_StartOf(r, tomWord, tomMove, &d);
    CHECK(hr == S_OK && d == -2, "StartOf hr %#lx d %ld", hr, d); expect(r, 0, 0, "StartOf word");
    ITextRange_Release(r);
    r = range(2, 2);
    hr = ITextRange_EndOf(r, tomWord, tomMove, &d);
    CHECK(hr == S_OK && d == 4, "EndOf d %ld", d); expect(r, 6, 6, "EndOf word");
    ITextRange_Release(r);
    r = range(2, 2);
    ITextRange_EndOf(r, tomWord, tomExtend, &d); expect(r, 2, 6, "EndOf word extend");
    ITextRange_Release(r);

    r = range(2, 2);
    hr = ITextRange_Expand(r, tomWord, &d);
    CHECK(hr == S_OK && d == 6, "Expand word d %ld", d); expect(r, 0, 6, "Expand word");
    hr = ITextRange_Expand(r, tomWord, &d);
    CHECK(hr == S_FALSE && d == 0, "Expand again is no change: hr %#lx d %ld", hr, d);
    ITextRange_Release(r);

    r = range(20, 20);
    ITextRange_Expand(r, tomParagraph, &d); expect(r, 17, 35, "Expand paragraph");
    ITextRange_Release(r);

    r = range(0, 5);
    ITextRange_MoveEnd(r, tomWord, 1, &d);
    CHECK(d == 1, "MoveEnd d %ld", d); expect(r, 0, 6, "MoveEnd word");
    ITextRange_MoveEnd(r, tomWord, -3, &d); expect(r, 0, 0, "MoveEnd back past the start");
    ITextRange_Release(r);
    r = range(6, 10);
    ITextRange_MoveEnd(r, tomWord, -3, &d); expect(r, 0, 0, "MoveEnd back past the start drags the start");
    ITextRange_Release(r);
    r = range(6, 10);
    ITextRange_MoveStart(r, tomWord, -1, &d);
    CHECK(d == -1, "MoveStart d %ld", d); expect(r, 0, 10, "MoveStart word back");
    ITextRange_MoveStart(r, tomWord, 3, &d); expect(r, 15, 15, "MoveStart past the end");
    ITextRange_Release(r);

    r = range(0, 0);
    hr = ITextRange_Move(r, tomParagraph, 1, &d);
    CHECK(d == 1, "Move paragraph d %ld", d); expect(r, 17, 17, "Move paragraph");
    ITextRange_SetStart(r, 20); ITextRange_SetEnd(r, 20);
    hr = ITextRange_Move(r, tomParagraph, -1, &d);
    CHECK(d == -1, "Move back a paragraph d %ld", d); expect(r, 17, 17, "Move paragraph -1");
    hr = ITextRange_Move(r, tomStory, 1, &d);
    CHECK(d == 1, "Move story d %ld", d); { LONG e = -1; ITextRange_GetEnd(r, &e); CHECK(e == len, "Move story: end %ld want %ld", e, len); }
    ITextRange_Release(r);

    SetWindowTextW(hwnd, L"One two. Three four!  Five\rNext");
    r = range(0, 0);
    ITextRange_Move(r, tomSentence, 1, &d);
    CHECK(d == 1, "Move sentence d %ld", d); expect(r, 9, 9, "Move sentence");
    ITextRange_Move(r, tomSentence, 1, &d); expect(r, 22, 22, "Move sentence 2");
    ITextRange_Move(r, tomSentence, 1, &d); expect(r, 27, 27, "Move sentence over the paragraph");
    ITextRange_Release(r);
    r = range(12, 12);
    ITextRange_Expand(r, tomSentence, &d); expect(r, 9, 22, "Expand sentence");
    ITextRange_Release(r);
    SetWindowTextW(hwnd, L"Hello big world.\rSecond  para here\rThird");

    /* the selection takes the same */
    {
        ITextSelection *sel = NULL;
        ITextDocument_GetSelection(doc, &sel);
        ITextSelection_SetRange(sel, 0, 0);
        hr = ITextSelection_Move(sel, tomWord, 2, &d);
        CHECK(hr == S_OK && d == 2, "selection Move hr %#lx d %ld", hr, d);
        {
            LONG st = -1;
            ITextSelection_GetStart(sel, &st);
            CHECK(st == 10, "selection at %ld", st);
        }
        ITextSelection_Release(sel);
    }
    ITextDocument_Release(doc);
    ole->lpVtbl->Release(ole);
    DestroyWindow(hwnd);
    printf(failed ? "RESULT: FAIL (%d)\n" : "RESULT: PASS\n", failed);
    return failed != 0;
}
