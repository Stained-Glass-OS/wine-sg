#include <windows.h>
#include <stdio.h>
#include <string.h>
static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
typedef int (__cdecl *parse_t)(const char *, int *, char *, char *, char *, char *);
typedef void *(__cdecl *open_t)(const char *, int, int, int, void *, char *);
typedef int (__cdecl *findex_t)(char *, void *, void **, char *);
static int try(parse_t parse, const char *src, int *type, char *host, char *port, char *name, char *err)
{
    *type = -1; strcpy(host, "x"); strcpy(port, "x"); strcpy(name, "x"); err[0] = 0;
    return parse(src, type, host, port, name, err);
}
int main(void)
{
    HMODULE m = LoadLibraryA("wpcap.dll");
    parse_t parse; open_t op; findex_t fx;
    char host[1024], port[1024], name[1024], err[512]; int type, r; void *devs;
    if (!m) { printf("FAIL cannot load wpcap\nRESULT: FAIL\n"); return 1; }
    parse = (parse_t)GetProcAddress(m, "pcap_parsesrcstr"); op = (open_t)GetProcAddress(m, "pcap_open"); fx = (findex_t)GetProcAddress(m, "pcap_findalldevs_ex");
    r = try(parse, "rpcap://eth0", &type, host, port, name, err);
    CHECK(!r && type == 3 && !*host && !*port && !strcmp(name, "eth0"), "local: %d type %d '%s' '%s' '%s'", r, type, host, port, name);
    r = try(parse, "rpcap://remote.example:2002/\\Device\\NPF_{A-B}", &type, host, port, name, err);
    CHECK(!r && type == 4 && !strcmp(host, "remote.example") && !strcmp(port, "2002") && !strcmp(name, "\\Device\\NPF_{A-B}"), "remote: %d type %d '%s' '%s' '%s'", r, type, host, port, name);
    r = try(parse, "rpcap://10.0.0.5/eth1", &type, host, port, name, err);
    CHECK(!r && type == 4 && !strcmp(host, "10.0.0.5") && !*port && !strcmp(name, "eth1"), "remote no port: %d type %d '%s' '%s' '%s'", r, type, host, port, name);
    r = try(parse, "rpcap://[fe80::1]:2002/en0", &type, host, port, name, err);
    CHECK(!r && type == 4 && !strcmp(host, "fe80::1") && !strcmp(port, "2002") && !strcmp(name, "en0"), "ipv6: %d type %d '%s' '%s' '%s'", r, type, host, port, name);
    r = try(parse, "rpcap:///eth2", &type, host, port, name, err);
    CHECK(!r && type == 3 && !*host && !strcmp(name, "eth2"), "empty host is local: %d type %d '%s' '%s'", r, type, host, name);
    r = try(parse, "file://C:\\caps\\a.pcap", &type, host, port, name, err);
    CHECK(!r && type == 2 && !strcmp(name, "C:\\caps\\a.pcap"), "file: %d type %d '%s'", r, type, name);
    r = try(parse, "\\Device\\NPF_{1-2}", &type, host, port, name, err);
    CHECK(!r && type == 3 && !strcmp(name, "\\Device\\NPF_{1-2}"), "bare name: %d type %d '%s'", r, type, name);
    r = try(parse, "rpcap://", &type, host, port, name, err);
    CHECK(r == -1 && err[0], "no name: %d '%s'", r, err);
    r = try(parse, "file://", &type, host, port, name, err);
    CHECK(r == -1 && type == 2, "file no name: %d type %d", r, type);
    r = try(parse, "rpcap://host:2002", &type, host, port, name, err);
    CHECK(r == 0 && type == 3 && !strcmp(name, "host:2002"), "no slash is a name: %d type %d '%s'", r, type, name);
    r = try(parse, NULL, &type, host, port, name, err);
    CHECK(r == -1 && err[0], "NULL source: %d", r);

    err[0] = 0; CHECK(op("rpcap://far.example/eth0", 100, 0, 10, NULL, err) == NULL && strstr(err, "Remote"), "pcap_open remote: '%s'", err);
    err[0] = 0; CHECK(op("file://x.pcap", 100, 0, 10, NULL, err) == NULL && err[0], "pcap_open file: '%s'", err);
    err[0] = 0; r = fx("eth0", NULL, &devs, err); CHECK(r == -1 && err[0], "findalldevs_ex bad prefix: %d '%s'", r, err);
    err[0] = 0; r = fx("rpcap://far.example/", NULL, &devs, err); CHECK(r == -1 && err[0], "findalldevs_ex remote: %d '%s'", r, err);
    printf(fails ? "RESULT: FAIL\n" : "RESULT: PASS\n");
    return fails != 0;
}
