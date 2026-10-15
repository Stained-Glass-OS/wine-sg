/* atl100: the container's sites seen from a hosted control */
#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <ocidl.h>
#include <stdio.h>

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

typedef BOOL (WINAPI *PAtlAxWinInit)(void);
typedef HRESULT (WINAPI *PAtlAxGetControl)(HWND, IUnknown **);

int main(void)
{
    HMODULE atl = LoadLibraryA("atl100.dll");
    PAtlAxWinInit init;
    PAtlAxGetControl get_control;
    HWND hwnd;
    IUnknown *unk = NULL;
    IOleObject *control = NULL;
    IOleClientSite *site = NULL;
    IOleContainer *container = NULL;
    IOleInPlaceSiteWindowless *ips = NULL;
    IOleInPlaceFrame *frame = NULL;
    IOleControlSite *cs = NULL;
    IEnumUnknown *en = NULL, *en2 = NULL;
    IUnknown *obj = NULL;
    HRESULT hr;
    ULONG n;

    CoInitialize(NULL);
    CHECK(atl != NULL, "atl100 not loaded");
    if (!atl) return 1;
    init = (PAtlAxWinInit)GetProcAddress(atl, "AtlAxWinInit");
    get_control = (PAtlAxGetControl)GetProcAddress(atl, "AtlAxGetControl");
    CHECK(init && init(), "AtlAxWinInit");
    hwnd = CreateWindowExW(0, L"AtlAxWin100", L"{0E59F1D5-1FBE-11D0-8FF2-00A0D10038BC}", WS_CHILD | WS_VISIBLE,
                           0, 0, 100, 50, GetDesktopWindow(), NULL, GetModuleHandleW(NULL), NULL);
    CHECK(hwnd != NULL, "window");
    hr = get_control(hwnd, &unk);
    CHECK(hr == S_OK && unk, "AtlAxGetControl %lx", hr);
    if (!unk) return 1;
    hr = IUnknown_QueryInterface(unk, &IID_IOleObject, (void **)&control);
    CHECK(hr == S_OK, "IOleObject %lx", hr);
    hr = IOleObject_GetClientSite(control, &site);
    CHECK(hr == S_OK && site, "GetClientSite %lx", hr);
    if (!site) return 1;

    hr = IOleClientSite_QueryInterface(site, &IID_IOleContainer, (void **)&container);
    CHECK(hr == S_OK, "IOleContainer %lx", hr);
    hr = IOleClientSite_QueryInterface(site, &IID_IOleInPlaceSiteWindowless, (void **)&ips);
    CHECK(hr == S_OK, "IOleInPlaceSiteWindowless %lx", hr);
    hr = IOleClientSite_QueryInterface(site, &IID_IOleInPlaceFrame, (void **)&frame);
    CHECK(hr == S_OK, "IOleInPlaceFrame %lx", hr);
    hr = IOleClientSite_QueryInterface(site, &IID_IOleControlSite, (void **)&cs);
    CHECK(hr == S_OK, "IOleControlSite %lx", hr);

    /* client site and container */
    hr = IOleClientSite_OnShowWindow(site, TRUE);
    CHECK(hr == S_OK, "OnShowWindow %lx", hr);
    hr = IOleContainer_LockContainer(container, TRUE);
    CHECK(hr == S_OK, "LockContainer %lx", hr);
    hr = IOleContainer_LockContainer(container, FALSE);
    CHECK(hr == S_OK, "unlock %lx", hr);
    hr = IOleContainer_EnumObjects(container, OLECONTF_EMBEDDINGS, &en);
    CHECK(hr == S_OK && en, "EnumObjects %lx", hr);
    if (en)
    {
        hr = IEnumUnknown_Next(en, 1, &obj, &n);
        CHECK(hr == S_OK && n == 1 && obj, "enum Next %lx %lu", hr, n);
        if (obj)
        {
            IOleObject *again = NULL;
            hr = IUnknown_QueryInterface(obj, &IID_IOleObject, (void **)&again);
            CHECK(hr == S_OK && again == control, "enumerated object is the control");
            if (again) IOleObject_Release(again);
            IUnknown_Release(obj);
        }
        hr = IEnumUnknown_Next(en, 1, &obj, &n);
        CHECK(hr == S_FALSE && n == 0, "second Next %lx", hr);
        IEnumUnknown_Reset(en);
        hr = IEnumUnknown_Clone(en, &en2);
        CHECK(hr == S_OK && en2, "enum Clone %lx", hr);
        if (en2)
        {
            hr = IEnumUnknown_Next(en2, 1, &obj, &n);
            CHECK(hr == S_OK && n == 1, "clone Next %lx", hr);
            if (obj) IUnknown_Release(obj);
            IEnumUnknown_Release(en2);
        }
        IEnumUnknown_Release(en);
    }
    hr = IOleContainer_EnumObjects(container, OLECONTF_ONLYUSER, &en);
    CHECK(hr == S_OK && en, "EnumObjects(none) %lx", hr);
    if (en)
    {
        hr = IEnumUnknown_Next(en, 1, &obj, &n);
        CHECK(hr == S_FALSE && n == 0, "empty enum Next %lx", hr);
        IEnumUnknown_Release(en);
    }

    /* the windowless site */
    {
        RECT r = { 1, 2, 30, 40 };
        HDC hdc = NULL;
        LRESULT res = 7;

        hr = IOleInPlaceSiteWindowless_CanWindowlessActivate(ips);
        CHECK(hr == S_OK, "CanWindowlessActivate %lx", hr);
        hr = IOleInPlaceSiteWindowless_GetCapture(ips);
        CHECK(hr == S_FALSE, "GetCapture before %lx", hr);
        hr = IOleInPlaceSiteWindowless_SetCapture(ips, TRUE);
        CHECK(hr == S_OK && GetCapture() == hwnd, "SetCapture %lx", hr);
        hr = IOleInPlaceSiteWindowless_GetCapture(ips);
        CHECK(hr == S_OK, "GetCapture after %lx", hr);
        IOleInPlaceSiteWindowless_SetCapture(ips, FALSE);
        CHECK(GetCapture() != hwnd, "capture released");
        hr = IOleInPlaceSiteWindowless_SetFocus(ips, TRUE);
        CHECK(hr == S_OK, "SetFocus %lx", hr);
        hr = IOleInPlaceSiteWindowless_GetDC(ips, &r, 0, &hdc);
        CHECK(hr == S_OK && hdc, "GetDC %lx", hr);
        if (hdc)
        {
            hr = IOleInPlaceSiteWindowless_ReleaseDC(ips, hdc);
            CHECK(hr == S_OK, "ReleaseDC %lx", hr);
        }
        ValidateRect(hwnd, NULL);
        hr = IOleInPlaceSiteWindowless_InvalidateRect(ips, &r, FALSE);
        CHECK(hr == S_OK && GetUpdateRect(hwnd, NULL, FALSE), "InvalidateRect %lx", hr);
        ValidateRect(hwnd, NULL);
        hr = IOleInPlaceSiteWindowless_InvalidateRgn(ips, NULL, FALSE);
        CHECK(hr == S_OK && GetUpdateRect(hwnd, NULL, FALSE), "InvalidateRgn %lx", hr);
        hr = IOleInPlaceSiteWindowless_AdjustRect(ips, &r);
        CHECK(hr == S_OK && r.left == 1 && r.bottom == 40, "AdjustRect %lx", hr);
        hr = IOleInPlaceSiteWindowless_OnDefWindowMessage(ips, WM_GETTEXTLENGTH, 0, 0, &res);
        CHECK(hr == S_OK && res != 7, "OnDefWindowMessage %lx %Id", hr, res);
        hr = IOleInPlaceSiteWindowless_OnPosRectChange(ips, &r);
        CHECK(hr == S_OK, "OnPosRectChange %lx", hr);
        {
            RECT pos, clip;
            OLEINPLACEFRAMEINFO info = { sizeof(info) };
            IOleInPlaceFrame *f2 = NULL;
            IOleInPlaceUIWindow *doc = NULL;
            hr = IOleInPlaceSiteWindowless_GetWindowContext(ips, &f2, &doc, &pos, &clip, &info);
            CHECK(hr == S_OK && pos.left == 1 && pos.right == 30, "size kept by OnPosRectChange: %lx %ld..%ld", hr, pos.left, pos.right);
            if (f2) IOleInPlaceFrame_Release(f2);
        }
        hr = IOleInPlaceSiteWindowless_OnUIDeactivate(ips, FALSE);
        CHECK(hr == S_OK, "OnUIDeactivate %lx", hr);
        hr = IOleInPlaceSiteWindowless_DiscardUndoState(ips);
        CHECK(hr == S_OK, "DiscardUndoState %lx", hr);
    }

    /* the frame */
    {
        RECT border;
        MSG msg = {0};

        hr = IOleInPlaceFrame_GetBorder(frame, &border);
        CHECK(hr == INPLACE_E_NOTOOLSPACE, "GetBorder %lx", hr);
        hr = IOleInPlaceFrame_RequestBorderSpace(frame, NULL);
        CHECK(hr == INPLACE_E_NOTOOLSPACE, "RequestBorderSpace %lx", hr);
        hr = IOleInPlaceFrame_SetStatusText(frame, L"text");
        CHECK(hr == S_OK, "SetStatusText %lx", hr);
        hr = IOleInPlaceFrame_EnableModeless(frame, TRUE);
        CHECK(hr == S_OK, "EnableModeless %lx", hr);
        hr = IOleInPlaceFrame_TranslateAccelerator(frame, &msg, 1);
        CHECK(hr == S_FALSE, "TranslateAccelerator %lx", hr);
    }

    /* the control site */
    {
        POINTL himetric = { 2540, 5080 };
        POINTF container_pt = {0};
        HDC dc = GetDC(NULL);
        int dpi = GetDeviceCaps(dc, LOGPIXELSX);

        ReleaseDC(NULL, dc);
        hr = IOleControlSite_OnControlInfoChanged(cs);
        CHECK(hr == S_OK, "OnControlInfoChanged %lx", hr);
        hr = IOleControlSite_LockInPlaceActive(cs, TRUE);
        CHECK(hr == S_OK, "LockInPlaceActive %lx", hr);
        hr = IOleControlSite_OnFocus(cs, TRUE);
        CHECK(hr == S_OK, "OnFocus %lx", hr);
        hr = IOleControlSite_TransformCoords(cs, &himetric, &container_pt, XFORMCOORDS_POSITION | XFORMCOORDS_HIMETRICTOCONTAINER);
        CHECK(hr == S_OK && (int)container_pt.x == dpi && (int)container_pt.y == 2 * dpi, "TransformCoords to container %lx %f %f (dpi %d)", hr, container_pt.x, container_pt.y, dpi);
        himetric.x = himetric.y = 0;
        hr = IOleControlSite_TransformCoords(cs, &himetric, &container_pt, XFORMCOORDS_POSITION | XFORMCOORDS_CONTAINERTOHIMETRIC);
        CHECK(hr == S_OK && himetric.x == 2540 && himetric.y == 5080, "TransformCoords back %lx %ld %ld", hr, himetric.x, himetric.y);
        hr = IOleControlSite_TransformCoords(cs, &himetric, &container_pt, XFORMCOORDS_POSITION);
        CHECK(hr == E_INVALIDARG, "TransformCoords without direction %lx", hr);
    }

    IOleControlSite_Release(cs);
    IOleInPlaceFrame_Release(frame);
    IOleInPlaceSiteWindowless_Release(ips);
    IOleContainer_Release(container);
    IOleClientSite_Release(site);
    IOleObject_Release(control);
    IUnknown_Release(unk);
    DestroyWindow(hwnd);
    printf("RESULT: %s\n", fails ? "FAIL" : "PASS");
    return fails;
}
