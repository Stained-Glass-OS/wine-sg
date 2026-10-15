#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <urlmon.h>
#include <stdio.h>
static int bad, total;
#define EXPECT_HR(e, c) do { HRESULT h_ = (c); total++; if (h_ != (e)) { bad++; printf("MISMATCH line %d: %s = %lx, expected %lx\n", __LINE__, #c, h_, (HRESULT)(e)); } } while (0)
#define EXPECT(c) do { total++; if (!(c)) { bad++; printf("MISMATCH line %d: %s\n", __LINE__, #c); } } while (0)

int main(void)
{
    IUri *uri = NULL;
    DWORD v = 0xdeadbeef, buf[4];
    HRESULT hr;

    setvbuf(stdout, NULL, _IONBF, 0);
    CoInitialize(NULL);

    /* no protocol handler answers for this scheme: an option it does not know is E_FAIL, as a short buffer */
    EXPECT_HR(E_FAIL, CoInternetQueryInfo(L"zzunknown:abc", QUERY_CAN_NAVIGATE, 0, buf, sizeof(buf), NULL, 0));
    EXPECT_HR(E_FAIL, CoInternetQueryInfo(L"zzunknown:abc", QUERY_USES_NETWORK, 0, buf, 1, NULL, 0));
    EXPECT_HR(S_OK, CoInternetQueryInfo(L"zzunknown:abc", QUERY_USES_NETWORK, 0, buf, sizeof(DWORD), NULL, 0));

    hr = CreateUri(L"http://www.example.com:8080/path?q=1", 0, 0, &uri);
    EXPECT_HR(S_OK, hr);
    if (uri) {
        EXPECT_HR(S_OK, IUri_GetPropertyDWORD(uri, Uri_PROPERTY_PORT, &v, 0));
        EXPECT(v == 8080);
        v = 0xdeadbeef;
        EXPECT_HR(E_INVALIDARG, IUri_GetPropertyDWORD(uri, Uri_PROPERTY_DWORD_LAST + 1, &v, 0));
        EXPECT(v == 0);
        v = 0xdeadbeef;
        EXPECT_HR(E_INVALIDARG, IUri_GetPropertyDWORD(uri, Uri_PROPERTY_ABSOLUTE_URI, &v, 0));
        IUri_Release(uri);
    }
    printf("checks %d, mismatches %d\nRESULT: %s\n", total, bad, bad ? "FAIL" : "PASS");
    return bad != 0;
}
