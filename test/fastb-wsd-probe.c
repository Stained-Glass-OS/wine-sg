/* wsdapi, fast mode: IWSDUdpAddress exclusive flag, TTL and alias */
#include <windows.h>
#include <stdio.h>

/* the table of IWSDUdpAddress, by position: the mingw header does not compile as C here */
typedef struct addr_vtbl
{
    void *query_interface, *add_ref;
    ULONG (STDMETHODCALLTYPE *release)(void *);
    void *serialize, *deserialize, *get_port, *set_port, *get_transport_address, *get_transport_address_ex,
         *set_transport_address, *set_sockaddr, *get_sockaddr;
    HRESULT (STDMETHODCALLTYPE *set_exclusive)(void *, BOOL);
    HRESULT (STDMETHODCALLTYPE *get_exclusive)(void *);
    void *set_message_type, *get_message_type;
    HRESULT (STDMETHODCALLTYPE *set_ttl)(void *, DWORD);
    HRESULT (STDMETHODCALLTYPE *get_ttl)(void *, DWORD *);
    HRESULT (STDMETHODCALLTYPE *set_alias)(void *, const GUID *);
    HRESULT (STDMETHODCALLTYPE *get_alias)(void *, GUID *);
} addr_vtbl;
struct addr { const addr_vtbl *vtbl; };

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

int main(void)
{
    HRESULT (WINAPI *create)(void **) = (void *)GetProcAddress(LoadLibraryA("wsdapi.dll"), "WSDCreateUdpAddress");
    struct addr *addr = NULL;
    GUID alias = { 0x12345678, 0x1234, 0x5678, { 1, 2, 3, 4, 5, 6, 7, 8 } }, got;
    DWORD ttl = 99;

    if (!create || FAILED(create((void **)&addr))) { printf("FAIL  create\nRESULT: FAIL\n"); return 1; }
    check(addr->vtbl->get_exclusive(addr) == S_FALSE, "not exclusive at first");
    check(addr->vtbl->set_exclusive(addr, TRUE) == S_OK && addr->vtbl->get_exclusive(addr) == S_OK, "exclusive");
    check(addr->vtbl->set_exclusive(addr, FALSE) == S_OK && addr->vtbl->get_exclusive(addr) == S_FALSE, "not exclusive again");
    check(addr->vtbl->set_ttl(addr, 4) == S_OK && addr->vtbl->get_ttl(addr, &ttl) == S_OK && ttl == 4, "TTL");
    check(addr->vtbl->get_ttl(addr, NULL) == E_POINTER, "TTL without pointer");
    check(addr->vtbl->set_alias(addr, &alias) == S_OK && addr->vtbl->get_alias(addr, &got) == S_OK && IsEqualGUID(&alias, &got), "alias");
    check(addr->vtbl->set_alias(addr, NULL) == E_POINTER, "alias without pointer");
    addr->vtbl->release(addr);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
