/* netapi32 netbios: the local name table (NCBADDNAME, NCBADDGRNAME, NCBDELNAME, the names in NCBASTAT).
 * The build environments of the gate have no NetBIOS adapter, so the functions of the table are
 * extracted by the gate script from netbios.c of the tree under test, on every run, and run here. */
#include <windows.h>
#include <nb30.h>
#include <stdio.h>
#include <string.h>
#define NB_MAX_NAMES 253
typedef struct { BOOL inUse; UCHAR state; UCHAR local_name[NCBNAMSZ]; UCHAR remote_name[NCBNAMSZ]; void *data; } NetBIOSSession;
typedef struct
{
    CRITICAL_SECTION cs;
    DWORD sessionsLen;
    NetBIOSSession *sessions;
    UCHAR nameCount;
    struct { UCHAR name[NCBNAMSZ]; UCHAR num; UCHAR flags; } names[NB_MAX_NAMES];
} NetBIOSAdapter;
#include "fastnetbios-names.inc"

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static void setname(NCB *ncb, const char *name)
{
    memset(ncb->ncb_name, ' ', NCBNAMSZ);
    memcpy(ncb->ncb_name, name, strlen(name));
}

int main(void)
{
    static NetBIOSAdapter adapter;
    NetBIOSSession sessions[2] = {0};
    NCB ncb;
    int i;
    char buf[sizeof(ADAPTER_STATUS) + 4 * sizeof(NAME_BUFFER)];
    ADAPTER_STATUS *astat = (ADAPTER_STATUS *)buf;
    NAME_BUFFER *nb = (NAME_BUFFER *)(astat + 1);

    InitializeCriticalSection(&adapter.cs);
    adapter.sessions = sessions;
    adapter.sessionsLen = 2;

    memset(&ncb, 0, sizeof(ncb));
    setname(&ncb, "ALPHA");
    CHECK(nbAddName(&adapter, &ncb, FALSE) == NRC_GOODRET && ncb.ncb_num == 2, "first name %u", ncb.ncb_num);
    setname(&ncb, "BETA");
    CHECK(nbAddName(&adapter, &ncb, TRUE) == NRC_GOODRET && ncb.ncb_num == 3, "group name number %u", ncb.ncb_num);
    setname(&ncb, "ALPHA");
    CHECK(nbAddName(&adapter, &ncb, FALSE) == NRC_DUPNAME, "duplicate name");
    setname(&ncb, "ALPHA");
    CHECK(nbAddName(&adapter, &ncb, TRUE) == NRC_DUPNAME, "unique name added as group");
    setname(&ncb, "BETA");
    ncb.ncb_num = 0;
    CHECK(nbAddName(&adapter, &ncb, TRUE) == NRC_GOODRET && ncb.ncb_num == 3, "group name can be added again, number %u", ncb.ncb_num);
    setname(&ncb, "BETA");
    CHECK(nbAddName(&adapter, &ncb, FALSE) == NRC_DUPNAME, "group name added as unique");
    memset(ncb.ncb_name, ' ', NCBNAMSZ);
    ncb.ncb_name[0] = '*';
    CHECK(nbAddName(&adapter, &ncb, FALSE) == NRC_NOWILD, "name starting with *");
    ncb.ncb_name[0] = 0;
    CHECK(nbAddName(&adapter, &ncb, FALSE) == NRC_NOWILD, "name starting with 0");
    CHECK(nbAddName(NULL, &ncb, FALSE) == NRC_BRIDGE, "no adapter");

    /* the names are in the status of the adapter */
    memset(buf, 0, sizeof(buf));
    memset(&ncb, 0, sizeof(ncb));
    ncb.ncb_buffer = (PUCHAR)buf;
    ncb.ncb_length = sizeof(buf);
    nbAStatAddNames(&adapter, &ncb);
    CHECK(astat->name_count == 2, "names in the status %u", astat->name_count);
    CHECK(!memcmp(nb[0].name, "ALPHA", 5) && nb[0].name_num == 2 && nb[0].name_flags == (REGISTERED | UNIQUE_NAME), "first status name");
    CHECK(!memcmp(nb[1].name, "BETA", 4) && nb[1].name_num == 3 && nb[1].name_flags == (REGISTERED | GROUP_NAME), "second status name flags %x", nb[1].name_flags);
    ncb.ncb_length = sizeof(ADAPTER_STATUS) + sizeof(NAME_BUFFER);
    memset(buf, 0, sizeof(buf));
    nbAStatAddNames(&adapter, &ncb);
    CHECK(astat->name_count == 1, "only the names that fit %u", astat->name_count);

    /* deleting */
    setname(&ncb, "ALPHA");
    ncb.ncb_num = 0;
    CHECK(nbDelName(&adapter, &ncb) == NRC_GOODRET && ncb.ncb_num == 2 && adapter.nameCount == 1, "delete");
    setname(&ncb, "ALPHA");
    CHECK(nbDelName(&adapter, &ncb) == NRC_NOWILD, "delete a missing name");
    setname(&ncb, "NEW");
    CHECK(nbAddName(&adapter, &ncb, FALSE) == NRC_GOODRET && ncb.ncb_num == 2, "the freed number is used again: %u", ncb.ncb_num);
    /* a name with a session */
    sessions[1].inUse = TRUE;
    memcpy(sessions[1].local_name, ncb.ncb_name, NCBNAMSZ);
    CHECK(nbDelName(&adapter, &ncb) == NRC_INUSE && adapter.nameCount == 2, "name with a session");
    {
        int j;
        for (j = 0; j < adapter.nameCount; j++)
            if (adapter.names[j].num == 2) CHECK(adapter.names[j].flags == (DEREGISTERED | UNIQUE_NAME), "deregistered state %x", adapter.names[j].flags);
    }
    sessions[1].inUse = FALSE;
    CHECK(nbDelName(&adapter, &ncb) == NRC_GOODRET, "delete after the session ended");

    /* the table is full at 253 names */
    adapter.nameCount = 0;
    for (i = 0; i < 253; i++)
    {
        char name[16];
        sprintf(name, "N%03d", i);
        setname(&ncb, name);
        if (nbAddName(&adapter, &ncb, FALSE) != NRC_GOODRET) { CHECK(0, "name %d", i); break; }
    }
    CHECK(adapter.nameCount == 253 && ncb.ncb_num == 254, "last number %u", ncb.ncb_num);
    setname(&ncb, "ONEMORE");
    CHECK(nbAddName(&adapter, &ncb, FALSE) == NRC_NAMTFUL, "table full");

    printf("RESULT: %s\n", fails ? "FAIL" : "PASS");
    return fails;
}
