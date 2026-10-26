/* pkgclass-app: the application of test/pkgclass-gate.sh (patches/sg/6124). It activates runtime
 * classes declared only in package manifests: one by its framework package (a WinUI 3 app's
 * Microsoft.UI.Xaml classes come from the Windows App SDK framework this way), one by its own
 * package, and one nobody declares. Prints NAME=hr and NAME_class=<the factory's class name>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <winstring.h>
#include <roapi.h>
#include <activation.h>
#include <stdio.h>

static void activate(const char *tag, const WCHAR *name)
{
    IActivationFactory *factory = NULL;
    HSTRING str, cls;
    HRESULT hr;

    WindowsCreateString(name, lstrlenW(name), &str);
    hr = RoGetActivationFactory(str, &IID_IActivationFactory, (void **)&factory);
    printf("%s=0x%08lx\n", tag, hr);
    if (SUCCEEDED(hr) && SUCCEEDED(IActivationFactory_GetRuntimeClassName(factory, &cls)))
    {
        printf("%s_class=%ls\n", tag, WindowsGetStringRawBuffer(cls, NULL));
        WindowsDeleteString(cls);
    }
    if (factory) IActivationFactory_Release(factory);
    WindowsDeleteString(str);
}

int main(void)
{
    RoInitialize(RO_INIT_MULTITHREADED);
    activate("fw", L"StainedGlass.Test.FwWidget");
    activate("app", L"StainedGlass.Test.AppWidget");
    activate("none", L"StainedGlass.Test.Nobody");
    fflush(stdout);
    return 0;
}
