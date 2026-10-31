/* bitsver-probe: the versioned BITS manager classes (patches/sg/6223).
 *
 * Only CLSID_BackgroundCopyManager was served; newer clients create BackgroundCopyManager1_5 ...
 * 3_0 (OneDrive asks for 3_0) and got "class not registered". They are the same manager.
 *
 * Prints name=value lines; see test/bitsver-gate.sh.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <bits.h>
#include <stdio.h>

DEFINE_GUID(CLSID_v1_5, 0xf087771f, 0xd74f, 0x4c1a, 0xbb, 0x8a, 0xe1, 0x6a, 0xca, 0x91, 0x24, 0xea);
DEFINE_GUID(CLSID_v2_0, 0x6d18ad12, 0xbde3, 0x4393, 0xb3, 0x11, 0x09, 0x9c, 0x34, 0x6e, 0x6d, 0xf9);
DEFINE_GUID(CLSID_v2_5, 0x03ca98d6, 0xff5d, 0x49b8, 0xab, 0xc6, 0x03, 0xdd, 0x84, 0x12, 0x70, 0x20);
DEFINE_GUID(CLSID_v3_0, 0x659cdea7, 0x489e, 0x11d9, 0xa9, 0xcd, 0x00, 0x0d, 0x56, 0x96, 0x52, 0x51);
DEFINE_GUID(IID_Job3, 0x443c8934, 0x90ff, 0x48ed, 0xbc, 0xde, 0x26, 0xf5, 0xc7, 0x45, 0x00, 0x42);

static void try(const char *name, const CLSID *clsid)
{
    IBackgroundCopyManager *mgr = NULL;
    IBackgroundCopyJob *job = NULL;
    IUnknown *job3 = NULL;
    GUID id;
    HRESULT hr = CoCreateInstance(clsid, NULL, CLSCTX_LOCAL_SERVER, &IID_IBackgroundCopyManager, (void **)&mgr);
    HRESULT jhr = E_FAIL, qhr = E_FAIL;

    if (SUCCEEDED(hr))
    {
        jhr = IBackgroundCopyManager_CreateJob(mgr, L"probe", BG_JOB_TYPE_DOWNLOAD, &id, &job);
        if (SUCCEEDED(jhr))
        {
            qhr = IBackgroundCopyJob_QueryInterface(job, &IID_Job3, (void **)&job3);
            if (job3) IUnknown_Release(job3);
            IBackgroundCopyJob_Cancel(job);
            IBackgroundCopyJob_Release(job);
        }
        IBackgroundCopyManager_Release(mgr);
    }
    printf("%s=%08lx,%08lx,%08lx\n", name, hr, jhr, qhr);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    try("v1_0", &CLSID_BackgroundCopyManager);
    try("v1_5", &CLSID_v1_5);
    try("v2_0", &CLSID_v2_0);
    try("v2_5", &CLSID_v2_5);
    try("v3_0", &CLSID_v3_0);
    printf("done=1\n");
    return 0;
}
