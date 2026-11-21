/* avifileexit-probe: avifil32 AVIFileExit undoes AVIFileInit (Stained Glass OS wine-sg 8263;
 * test/avifileexit-gate.sh): AVIFileInit initialises OLE on the thread (single-threaded); after AVIFileExit the
 * thread can join the multithreaded apartment again. AVIFileExit did nothing, so it could not.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <objbase.h>
#include <stdio.h>

int main( void )
{
    HMODULE avi = LoadLibraryA( "avifil32.dll" );
    void (WINAPI *pInit)(void) = (void *)GetProcAddress( avi, "AVIFileInit" );
    void (WINAPI *pExit)(void) = (void *)GetProcAddress( avi, "AVIFileExit" );
    HRESULT hr;
    int fails = 0;

    pInit();
    hr = CoInitializeEx( NULL, COINIT_MULTITHREADED );
    printf( "%s  after AVIFileInit the thread is single-threaded (%#lx)\n", hr == RPC_E_CHANGED_MODE ? "PASS" : "FAIL", hr );
    if (hr != RPC_E_CHANGED_MODE) fails++;
    if (SUCCEEDED(hr)) CoUninitialize();
    pExit();
    hr = CoInitializeEx( NULL, COINIT_MULTITHREADED );
    printf( "%s  after AVIFileExit it can be multithreaded (%#lx)\n", hr == S_OK ? "PASS" : "FAIL", hr );
    if (hr != S_OK) fails++;
    if (SUCCEEDED(hr)) CoUninitialize();
    printf( "RESULT: %s\n", fails ? "FAIL" : "PASS" );
    return fails != 0;
}
