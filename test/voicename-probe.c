/* voicename-probe: the name and attributes of the SAPI voice (Stained Glass OS wine-sg 6080), run by
 * test/voicename-gate.sh. Prints name=value lines. */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <sapi.h>
#include <stdio.h>

static void print_w(const char *key, const WCHAR *value)
{
    char buf[512];
    WideCharToMultiByte(CP_UTF8, 0, value ? value : L"(none)", -1, buf, sizeof(buf), NULL, NULL);
    printf("%s=%s\n", key, buf);
}

int main(void)
{
    static const WCHAR *attrs[] = { L"Name", L"Vendor", L"Gender", L"Age", L"Language" };
    ISpVoice *voice = NULL;
    ISpObjectToken *token = NULL;
    ISpDataKey *attributes = NULL;
    WCHAR *str = NULL;
    char key[64];
    unsigned int i;
    HRESULT hr;

    CoInitialize(NULL);
    hr = CoCreateInstance(&CLSID_SpVoice, NULL, CLSCTX_ALL, &IID_ISpVoice, (void **)&voice);
    printf("voice=%d\n", hr == S_OK);
    if (hr != S_OK) return 1;
    hr = ISpVoice_GetVoice(voice, &token);
    printf("token=%d\n", hr == S_OK);
    if (hr != S_OK) return 1;
    if (ISpObjectToken_GetId(token, &str) == S_OK) { print_w("id", str); CoTaskMemFree(str); }
    if (ISpObjectToken_GetStringValue(token, NULL, &str) == S_OK) { print_w("description", str); CoTaskMemFree(str); }
    if (ISpObjectToken_GetStringValue(token, L"409", &str) == S_OK) { print_w("description409", str); CoTaskMemFree(str); }
    if (ISpObjectToken_OpenKey(token, L"Attributes", &attributes) == S_OK)
    {
        for (i = 0; i < ARRAYSIZE(attrs); i++)
        {
            WideCharToMultiByte(CP_UTF8, 0, attrs[i], -1, key, sizeof(key), NULL, NULL);
            if (ISpDataKey_GetStringValue(attributes, attrs[i], &str) == S_OK) { print_w(key, str); CoTaskMemFree(str); }
            else printf("%s=(none)\n", key);
        }
        ISpDataKey_Release(attributes);
    }
    ISpObjectToken_Release(token);
    ISpVoice_Release(voice);
    printf("done=1\n");
    return 0;
}
