/* sapiaudioout-probe.c (wine-sg 6092): the default audio output token, as ScummVM's text-to-speech finds it:
 * AudioOutput's default token id, an SpObjectToken set to it, and an ISpAudio created from it. KEY=VALUE lines. */
#define COBJMACROS
#include <initguid.h>
#include <windows.h>
#include <sapi.h>
#include <stdio.h>

int main(void)
{
    ISpObjectTokenCategory *cat;
    ISpObjectToken *token, *other;
    ISpAudio *audio = NULL;
    WCHAR *id = NULL, *clsid = NULL, *name = NULL;
    HRESULT hr;

    CoInitialize(NULL);
    printf("bits=%d\n", (int)sizeof(void *) * 8);
    hr = CoCreateInstance(&CLSID_SpObjectTokenCategory, NULL, CLSCTX_ALL, &IID_ISpObjectTokenCategory, (void **)&cat);
    if (SUCCEEDED(hr)) hr = ISpObjectTokenCategory_SetId(cat, SPCAT_AUDIOOUT, TRUE);
    if (SUCCEEDED(hr)) hr = ISpObjectTokenCategory_GetDefaultTokenId(cat, &id);
    printf("default_id=%#lx %d\n", hr, id && !wcscmp(id, L"HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Speech\\AudioOutput\\TokenEnums\\MMAudioOut\\"));
    hr = CoCreateInstance(&CLSID_SpObjectToken, NULL, CLSCTX_ALL, &IID_ISpObjectToken, (void **)&token);
    if (SUCCEEDED(hr)) hr = ISpObjectToken_SetId(token, NULL, id, FALSE);
    printf("set_id=%#lx\n", hr);
    if (SUCCEEDED(hr)) hr = ISpObjectToken_GetStringValue(token, L"CLSID", &clsid);
    printf("clsid=%#lx %d\n", hr, clsid && !_wcsicmp(clsid, L"{A8C680EB-3D32-11D2-9EE7-00C04F797396}"));
    if (SUCCEEDED(ISpObjectToken_GetStringValue(token, NULL, &name))) printf("has_name=%d\n", name && *name);
    hr = ISpObjectToken_CreateInstance(token, NULL, CLSCTX_ALL, &IID_ISpAudio, (void **)&audio);
    printf("create_audio=%#lx %d\n", hr, audio != NULL);
    if (audio) ISpAudio_Release(audio);
    /* a device that is not there */
    hr = CoCreateInstance(&CLSID_SpObjectToken, NULL, CLSCTX_ALL, &IID_ISpObjectToken, (void **)&other);
    if (SUCCEEDED(hr)) hr = ISpObjectToken_SetId(other, NULL,
        L"HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Speech\\AudioOutput\\TokenEnums\\MMAudioOut\\999", FALSE);
    printf("missing_device=%#lx\n", hr);
    printf("done=1\n");
    return 0;
}
