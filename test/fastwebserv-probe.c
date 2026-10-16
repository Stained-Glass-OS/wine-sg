#include <windows.h>
#include <stdio.h>
#include <string.h>
typedef struct { int type; } WS_ENC;
typedef struct { WS_ENC encoding; int charset; } WS_TEXT_ENC;
typedef struct { int type; } WS_INPUT;
typedef struct { WS_INPUT input; void *data; ULONG size; } WS_BUF_INPUT;
typedef struct { char minValue, maxValue; } D8;
typedef struct { USHORT minValue, maxValue; } D16U;
typedef struct { __int64 minValue, maxValue; } D64;
static HRESULT (WINAPI *pCreateReader)(const void *, ULONG, void **, void *);
static HRESULT (WINAPI *pSetInput)(void *, const void *, const void *, const void *, ULONG, void *);
static HRESULT (WINAPI *pReadType)(void *, int, int, const void *, int, void *, void *, ULONG, void *);
static void (WINAPI *pFreeReader)(void *);
static void *reader, *heap;
static int bad;
#define EXPECT(c) do { if (!(c)) { bad++; printf("MISMATCH line %d\n", __LINE__); } } while (0)
static HRESULT (WINAPI *pReadStart)(void *, void *);
static HRESULT (WINAPI *pReadToStart)(void *, const void *, const void *, BOOL *, void *);
static HRESULT (WINAPI *pCreateHeap)(SIZE_T, SIZE_T, const void *, ULONG, void **, void *);
static HRESULT rd(const char *xml, int type, const void *desc, void *val, ULONG size)
{
    WS_TEXT_ENC enc = {{1}, 1 /* WS_CHARSET_UTF8 */};
    WS_BUF_INPUT in = {{1}, (void *)xml, (ULONG)strlen(xml)};
    HRESULT hr = pSetInput(reader, &enc, &in, NULL, 0, NULL); if (hr) printf("SETINPUT %08lx\n", hr);
    if (hr != S_OK) return hr;
    hr = pReadToStart(reader, NULL, NULL, NULL, NULL); if (hr) printf("START %08lx\n", hr);
    if (hr != S_OK) return hr;
    hr = pReadStart(reader, NULL);
    if (hr != S_OK) return hr;
    return pReadType(reader, 3 /* element content mapping */, type, desc, 1 /* required value */, heap, val, size, NULL);
}
#define P(...) printf(__VA_ARGS__)
int main(void)
{
    HMODULE m = LoadLibraryA("webservices.dll");
    HRESULT hr; D8 d8 = {-10, 100}; D16U d16 = {5, 1000}; D64 d64 = {-5, 5};
    signed char v8 = 0; USHORT v16 = 0; __int64 v64 = 0;
    setvbuf(stdout, NULL, _IONBF, 0);
    pCreateReader = (void *)GetProcAddress(m, "WsCreateReader");
    pSetInput = (void *)GetProcAddress(m, "WsSetInput");
    pReadType = (void *)GetProcAddress(m, "WsReadType");
    pReadStart = (void *)GetProcAddress(m, "WsReadStartElement");
    pReadToStart = (void *)GetProcAddress(m, "WsReadToStartElement");
    pCreateHeap = (void *)GetProcAddress(m, "WsCreateHeap");
    pFreeReader = (void *)GetProcAddress(m, "WsFreeReader");
    pCreateHeap(1 << 16, 0, NULL, 0, &heap, NULL);
    hr = pCreateReader(NULL, 0, &reader, NULL); P("CREATE %08lx\n", hr);
    hr = rd("<t>50</t>", 1, &d8, &v8, sizeof(v8)); P("I8_IN %08lx %d\n", hr, v8); EXPECT(hr == S_OK && v8 == 50);
    hr = rd("<t>101</t>", 1, &d8, &v8, sizeof(v8)); P("I8_HI %08lx\n", hr); EXPECT(hr == 0x803d0000);
    hr = rd("<t>-11</t>", 1, &d8, &v8, sizeof(v8)); P("I8_LO %08lx\n", hr); EXPECT(hr == 0x803d0000);
    hr = rd("<t>-10</t>", 1, &d8, &v8, sizeof(v8)); P("I8_EDGE %08lx %d\n", hr, v8); EXPECT(hr == S_OK && v8 == -10);
    hr = rd("<t>4</t>", 6, &d16, &v16, sizeof(v16)); P("U16_LO %08lx\n", hr); EXPECT(hr == 0x803d0000);
    hr = rd("<t>1000</t>", 6, &d16, &v16, sizeof(v16)); P("U16_EDGE %08lx %u\n", hr, v16); EXPECT(hr == S_OK && v16 == 1000);
    hr = rd("<t>6</t>", 4, &d64, &v64, sizeof(v64)); P("I64_HI %08lx\n", hr); EXPECT(hr == 0x803d0000);
    hr = rd("<t>-5</t>", 4, &d64, &v64, sizeof(v64)); P("I64_EDGE %08lx %I64d\n", hr, v64); EXPECT(hr == S_OK && v64 == -5);
    hr = rd("<t>7</t>", 1, NULL, &v8, sizeof(v8)); P("I8_NODESC %08lx %d\n", hr, v8); EXPECT(hr == S_OK && v8 == 7);
    pFreeReader(reader);
    P("%s\n", bad ? "RESULT: FAIL" : "RESULT: PASS");
    P("DONE\n");
    return 0;
}
