#define COBJMACROS
#include <windows.h>
#include <dxgi1_2.h>
#include <stdio.h>
static int bad, total;
#define EXPECT_HR(e, c) do { HRESULT h_ = (c); total++; if (h_ != (e)) { bad++; printf("MISMATCH line %d: %s = %lx, expected %lx\n", __LINE__, #c, h_, (HRESULT)(e)); } } while (0)
#define EXPECT(c) do { total++; if (!(c)) { bad++; printf("MISMATCH line %d: %s\n", __LINE__, #c); } } while (0)

int main(void)
{
    IDXGIFactory1 *factory = NULL;
    IDXGIFactory2 *factory2 = NULL;
    IDXGIAdapter *adapter = NULL;
    IDXGIOutput *output = NULL;
    IDXGIOutput1 *output1 = NULL;
    UINT n0 = 0, n_int = 0, n_scal = 0, n1 = 0, n1_stereo = 0;
    DXGI_MODE_DESC *modes;
    LUID luid = { 1, 2 };

    setvbuf(stdout, NULL, _IONBF, 0);
    EXPECT_HR(S_OK, CreateDXGIFactory1(&IID_IDXGIFactory1, (void **)&factory));
    if (!factory) { printf("RESULT: FAIL\n"); return 1; }
    EXPECT_HR(S_OK, IDXGIFactory1_QueryInterface(factory, &IID_IDXGIFactory2, (void **)&factory2));
    EXPECT_HR(S_OK, IDXGIFactory1_EnumAdapters(factory, 0, &adapter));
    if (SUCCEEDED(IDXGIAdapter_EnumOutputs(adapter, 0, &output)))
    {
        EXPECT_HR(S_OK, IDXGIOutput_GetDisplayModeList(output, DXGI_FORMAT_R8G8B8A8_UNORM, 0, &n0, NULL));
        EXPECT(n0 > 0);
        EXPECT_HR(S_OK, IDXGIOutput_GetDisplayModeList(output, DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_ENUM_MODES_INTERLACED, &n_int, NULL));
        EXPECT(n_int >= n0);
        EXPECT_HR(S_OK, IDXGIOutput_GetDisplayModeList(output, DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_ENUM_MODES_SCALING, &n_scal, NULL));
        EXPECT(n_scal >= n0);
        modes = calloc(n0 + 1, sizeof(*modes));
        n_scal = n0;
        EXPECT_HR(S_OK, IDXGIOutput_GetDisplayModeList(output, DXGI_FORMAT_R8G8B8A8_UNORM, 0, &n_scal, modes));
        EXPECT(n_scal == n0 && modes[0].Width && modes[0].Height);
        free(modes);
        if (n0 > 1)
        {
            UINT few = n0 - 1;
            modes = calloc(n0, sizeof(*modes));
            EXPECT_HR(DXGI_ERROR_MORE_DATA, IDXGIOutput_GetDisplayModeList(output, DXGI_FORMAT_R8G8B8A8_UNORM, 0, &few, modes));
            free(modes);
        }
        if (SUCCEEDED(IDXGIOutput_QueryInterface(output, &IID_IDXGIOutput1, (void **)&output1)))
        {
            EXPECT_HR(S_OK, IDXGIOutput1_GetDisplayModeList1(output1, DXGI_FORMAT_R8G8B8A8_UNORM, 0, &n1, NULL));
            EXPECT(n1 == n0);
            /* no stereo modes exist: asking for them adds nothing */
            EXPECT_HR(S_OK, IDXGIOutput1_GetDisplayModeList1(output1, DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_ENUM_MODES_STEREO, &n1_stereo, NULL));
            EXPECT(n1_stereo >= n1);
            IDXGIOutput1_Release(output1);
        }
        IDXGIOutput_Release(output);
    }
    else EXPECT(!"output");

    /* no shared resource handle names a resource here: every handle is invalid */
    EXPECT_HR(DXGI_ERROR_INVALID_CALL, IDXGIFactory2_GetSharedResourceAdapterLuid(factory2, (HANDLE)0x1234, &luid));
    EXPECT_HR(DXGI_ERROR_INVALID_CALL, IDXGIFactory2_GetSharedResourceAdapterLuid(factory2, GetCurrentProcess(), &luid));
    EXPECT_HR(DXGI_ERROR_INVALID_CALL, IDXGIFactory2_GetSharedResourceAdapterLuid(factory2, NULL, &luid));
    EXPECT_HR(DXGI_ERROR_INVALID_CALL, IDXGIFactory2_GetSharedResourceAdapterLuid(factory2, (HANDLE)0x1234, NULL));

    IDXGIAdapter_Release(adapter);
    IDXGIFactory2_Release(factory2);
    IDXGIFactory1_Release(factory);
    printf("checks %d, mismatches %d\nRESULT: %s\n", total, bad, bad ? "FAIL" : "PASS");
    return bad != 0;
}
