/* dnsroot-probe: the DNS search list and each adapter's DNS suffix (patches/sg/9660) */
#include <winsock2.h>
#include <windows.h>
#include <windns.h>
#include <iphlpapi.h>
#include <stdio.h>
int main(void)
{
    WCHAR buf[512]; DWORD size = sizeof(buf); ULONG len = 0;
    IP_ADAPTER_ADDRESSES *aa, *p;
    if (!DnsQueryConfig(DnsConfigSearchList, 0, NULL, NULL, buf, &size)) {
        WCHAR *s; printf("SEARCH");
        for (s = buf; *s; s += wcslen(s) + 1) printf(" [%ls]", s);
        printf("\n");
    } else printf("SEARCH error\n");
    GetAdaptersAddresses(AF_UNSPEC, 0, NULL, NULL, &len);
    aa = malloc(len);
    if (!GetAdaptersAddresses(AF_UNSPEC, 0, NULL, aa, &len))
        for (p = aa; p; p = p->Next) printf("SUFFIX %ls [%ls]\n", p->FriendlyName, p->DnsSuffix);
    return 0;
}
