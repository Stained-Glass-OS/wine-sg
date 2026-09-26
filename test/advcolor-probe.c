/* advcolor-probe: a display's colour capabilities (patches/sg/0342).
 *
 *  - DisplayConfigGetDeviceInfo answers DISPLAYCONFIG_DEVICE_INFO_GET_ADVANCED_COLOR_INFO
 *    (not supported, not enabled, RGB, 8 bits per channel) and
 *    ..._GET_SDR_WHITE_LEVEL (1000 = 80 nits) for an active target, and
 *    refuses a target that does not exist;
 *  - IDXGIOutput6::GetDesc1 describes an 8-bit sRGB SDR display.
 *
 * Programs that ask (Chromium, OBS, Paint.NET, games) take the SDR path.
 *
 * Prints name=value lines; see test/advcolor-gate.sh.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <dxgi1_6.h>
#include <stdio.h>

int main(void)
{
    DISPLAYCONFIG_PATH_INFO paths[16];
    DISPLAYCONFIG_MODE_INFO modes[32];
    UINT32 npaths = 16, nmodes = 32;
    DISPLAYCONFIG_GET_ADVANCED_COLOR_INFO color;
    DISPLAYCONFIG_SDR_WHITE_LEVEL white;
    IDXGIFactory1 *factory;
    IDXGIAdapter1 *adapter;
    IDXGIOutput *output;
    IDXGIOutput6 *output6;
    DXGI_OUTPUT_DESC1 desc;
    LONG ret;

    if ((ret = QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &npaths, paths, &nmodes, modes, NULL)) || !npaths)
    {
        printf("no_paths=%ld\n", ret);
        return 1;
    }

    memset(&color, 0xcc, sizeof(color));
    color.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_ADVANCED_COLOR_INFO;
    color.header.size = sizeof(color);
    color.header.adapterId = paths[0].targetInfo.adapterId;
    color.header.id = paths[0].targetInfo.id;
    ret = DisplayConfigGetDeviceInfo(&color.header);
    printf("advanced_color=%d (ret %ld value %#x encoding %d bits %u)\n",
           !ret && !color.advancedColorSupported && !color.advancedColorEnabled
           && color.colorEncoding == DISPLAYCONFIG_COLOR_ENCODING_RGB && color.bitsPerColorChannel == 8,
           ret, color.value, color.colorEncoding, color.bitsPerColorChannel);

    memset(&white, 0xcc, sizeof(white));
    white.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SDR_WHITE_LEVEL;
    white.header.size = sizeof(white);
    white.header.adapterId = paths[0].targetInfo.adapterId;
    white.header.id = paths[0].targetInfo.id;
    ret = DisplayConfigGetDeviceInfo(&white.header);
    printf("sdr_white_level=%d (ret %ld level %lu)\n", !ret && white.SDRWhiteLevel == 1000, ret, white.SDRWhiteLevel);

    color.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_ADVANCED_COLOR_INFO;
    color.header.id = 0xdead;
    ret = DisplayConfigGetDeviceInfo(&color.header);
    printf("unknown_target_refused=%d (ret %ld)\n", ret != 0, ret);

    color.header.id = paths[0].targetInfo.id;
    color.header.size = sizeof(color.header);
    ret = DisplayConfigGetDeviceInfo(&color.header);
    printf("short_packet_refused=%d (ret %ld)\n", ret != 0, ret);

    if (FAILED(CreateDXGIFactory1(&IID_IDXGIFactory1, (void **)&factory))
            || FAILED(IDXGIFactory1_EnumAdapters1(factory, 0, &adapter))
            || FAILED(IDXGIAdapter1_EnumOutputs(adapter, 0, &output))
            || FAILED(IDXGIOutput_QueryInterface(output, &IID_IDXGIOutput6, (void **)&output6)))
    {
        printf("desc1=0 (no output)\n");
        return 0;
    }
    memset(&desc, 0, sizeof(desc));
    IDXGIOutput6_GetDesc1(output6, &desc);
    printf("desc1=%d (bits %u space %d red %.3f,%.3f white %.4f,%.4f lum %.1f-%.1f)\n",
           desc.BitsPerColor == 8 && desc.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709
           && desc.RedPrimary[0] > 0.63f && desc.RedPrimary[0] < 0.65f
           && desc.WhitePoint[0] > 0.312f && desc.WhitePoint[0] < 0.313f && desc.MaxLuminance > 0.0f,
           desc.BitsPerColor, desc.ColorSpace, desc.RedPrimary[0], desc.RedPrimary[1],
           desc.WhitePoint[0], desc.WhitePoint[1], desc.MinLuminance, desc.MaxLuminance);
    printf("done=1\n");
    return 0;
}
