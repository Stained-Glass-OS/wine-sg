/* dwmapi functions that were stubs (patches/sg/1612). */
#include <windows.h>
#include <dwmapi.h>
#include <stdio.h>

static int failures;

static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) failures++;
}

int main(void)
{
    HWND hwnd = CreateWindowW(L"static", L"dwm", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 400, 300, 0, 0, 0, 0);
    DWORD color = 0, value, pol, gen;
    BOOL opaque = 2, b, remoting, connected;
    RECT rc;
    UINT thick = 0;
    DWM_BLURBEHIND bb = { DWM_BB_ENABLE, TRUE, NULL, FALSE };
    DWM_TIMING_INFO ti = { sizeof(ti) };
    HKEY key;
    HRESULT hr;

    RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\DWM", 0, NULL, 0, KEY_SET_VALUE, NULL, &key, NULL);
    value = 0xc4123456;
    RegSetValueExW(key, L"ColorizationColor", 0, REG_DWORD, (BYTE *)&value, sizeof(value));
    RegCloseKey(key);
    hr = DwmGetColorizationColor(&color, &opaque);
    printf("colorization: hr %#lx color %#lx opaque %d\n", hr, color, opaque);
    check(hr == S_OK && color == 0xc4123456 && (opaque == 0 || opaque == 1), "DwmGetColorizationColor gives the accent colour");

    b = TRUE;
    hr = DwmSetWindowAttribute(hwnd, DWMWA_TRANSITIONS_FORCEDISABLED, &b, sizeof(b));
    check(hr == S_OK, "DwmSetWindowAttribute(DWMWA_TRANSITIONS_FORCEDISABLED)");
    hr = DwmSetWindowAttribute(hwnd, 999, &b, sizeof(b));
    check(hr == E_INVALIDARG, "an unknown attribute is refused");
    pol = 7;
    hr = DwmSetWindowAttribute(hwnd, DWMWA_NCRENDERING_POLICY, &pol, sizeof(pol));
    check(hr == E_INVALIDARG, "a bad non-client rendering policy is refused");
    b = 2;
    hr = DwmGetWindowAttribute(hwnd, DWMWA_NCRENDERING_ENABLED, &b, sizeof(b));
    check(hr == S_OK && b == TRUE, "DWMWA_NCRENDERING_ENABLED: on");
    pol = DWMNCRP_DISABLED;
    DwmSetWindowAttribute(hwnd, DWMWA_NCRENDERING_POLICY, &pol, sizeof(pol));
    hr = DwmGetWindowAttribute(hwnd, DWMWA_NCRENDERING_ENABLED, &b, sizeof(b));
    check(hr == S_OK && b == FALSE, "... off once the policy disables it");
    memset(&rc, 0, sizeof(rc));
    hr = DwmGetWindowAttribute(hwnd, DWMWA_CAPTION_BUTTON_BOUNDS, &rc, sizeof(rc));
    printf("caption buttons: hr %#lx %ld,%ld-%ld,%ld\n", hr, rc.left, rc.top, rc.right, rc.bottom);
    check(hr == S_OK && rc.right > rc.left && rc.right <= 400 && rc.bottom > rc.top, "DWMWA_CAPTION_BUTTON_BOUNDS");
    hr = DwmGetWindowAttribute(hwnd, DWMWA_VISIBLE_FRAME_BORDER_THICKNESS, &thick, sizeof(thick));
    check(hr == S_OK && thick == 1, "DWMWA_VISIBLE_FRAME_BORDER_THICKNESS");
    hr = DwmGetWindowAttribute(hwnd, DWMWA_CAPTION_COLOR, &value, sizeof(value));
    check(hr == E_INVALIDARG, "a set-only attribute cannot be read");

    hr = DwmEnableBlurBehindWindow(hwnd, &bb);
    check(hr == S_OK, "DwmEnableBlurBehindWindow");
    hr = DwmGetTransportAttributes(&remoting, &connected, &gen);
    check(hr == S_OK && !remoting && connected, "DwmGetTransportAttributes: local, connected");
    hr = DwmGetCompositionTimingInfo(NULL, &ti);
    printf("timing: hr %#lx refresh %llu frame %llu\n", hr, (ULONGLONG)ti.cRefresh, (ULONGLONG)ti.cFrame);
    check(hr == S_OK && ti.cRefresh && ti.cFrame, "DwmGetCompositionTimingInfo counts refreshes");
    check(DwmInvalidateIconicBitmaps(hwnd) == S_OK, "DwmInvalidateIconicBitmaps");

    DestroyWindow(hwnd);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
