#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <esent.h>
#undef JetGetTableColumnInfoW

#ifndef JET_bitIndexPrimary
#define JET_bitIndexPrimary 2
#define JET_prepInsert 0
#define JET_bitSeekEQ 1
#define JET_bitNewKey 1
#define JET_MoveFirst 0x80000000
#endif
static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
#define N 50

int main(int argc, char **argv)
{
    int write = argc > 1 && !strcmp(argv[1], "write");
    JET_INSTANCE inst = 0; JET_SESID ses; JET_DBID db; JET_TABLEID t;
    JET_COLUMNDEF cd; JET_COLUMNID cid, cname; JET_ERR e;
    unsigned long act; int i; long v;
    char name[32];

    JetCreateInstanceW(&inst, L"x2");
    JetSetSystemParameterW(&inst, 0, JET_paramSystemPath, 0, L".\\");
    JetSetSystemParameterW(&inst, 0, JET_paramBaseName, 0, L"sg3");
    CHECK(JetInit(&inst) == 0, "init");
    JetBeginSessionW(inst, &ses, NULL, NULL);
    if (write)
    {
        DeleteFileA("x2.edb");
        CHECK(JetCreateDatabaseW(ses, L"x2.edb", NULL, &db, 0) == 0, "create");
        JetCreateTableW(ses, db, L"t", 1, 80, &t);
        memset(&cd, 0, sizeof(cd)); cd.cbStruct = sizeof(cd); cd.coltyp = JET_coltypLong;
        JetAddColumnW(ses, t, L"id", &cd, NULL, 0, &cid);
        cd.coltyp = JET_coltypText; cd.cbMax = 30; cd.cp = 1252;
        JetAddColumnW(ses, t, L"name", &cd, NULL, 0, &cname);
        CHECK(JetCreateIndexW(ses, t, L"pk", JET_bitIndexPrimary, L"+id\0", 5, 100) == 0, "index");
        for (i = 0; i < N; i++)
        {
            v = (i * 17) % N + 1000;
            snprintf(name, sizeof(name), "n%03d", (int)v);
            JetPrepareUpdate(ses, t, JET_prepInsert);
            JetSetColumn(ses, t, cid, &v, 4, 0, NULL);
            JetSetColumn(ses, t, cname, name, strlen(name), 0, NULL);
            CHECK(JetUpdate(ses, t, NULL, 0, NULL) == 0, "insert %d", i);
        }
        JetCloseTable(ses, t); JetCloseDatabase(ses, db, 0);
        JetDetachDatabaseW(ses, (const char *)L"x2.edb");
    }
    else
    {
        CHECK(JetAttachDatabaseW(ses, L"x2.edb", 0) == 0, "attach");
        CHECK(JetOpenDatabaseW(ses, L"x2.edb", NULL, &db, 0) == 0, "open");
        CHECK(JetOpenTableW(ses, db, L"t", NULL, 0, 0, &t) >= 0, "open table");
        JetGetTableColumnInfoW(ses, t, L"id", &cd, sizeof(cd), JET_ColInfo); cid = cd.columnid;
        JetGetTableColumnInfoW(ses, t, L"name", &cd, sizeof(cd), JET_ColInfo); cname = cd.columnid;
        e = JetMove(ses, t, JET_MoveFirst, 0);
        for (i = 0; i < N; i++)
        {
            CHECK(e == 0, "move %d", i);
            JetRetrieveColumn(ses, t, cid, &v, 4, &act, 0, NULL);
            CHECK(v == 1000 + i, "sorted id %d got %ld", i, v);
            memset(name, 0, sizeof(name));
            JetRetrieveColumn(ses, t, cname, name, 31, &act, 0, NULL);
            { char exp[32]; snprintf(exp, sizeof(exp), "n%03ld", v); CHECK(!strcmp(name, exp), "name %s vs %s", name, exp); }
            e = JetMove(ses, t, 1, 0);
        }
        v = 1017;
        JetMakeKey(ses, t, &v, 4, JET_bitNewKey);
        CHECK(JetSeek(ses, t, JET_bitSeekEQ) == 0, "seek");
        JetCloseTable(ses, t); JetCloseDatabase(ses, db, 0);
    }
    JetEndSession(ses, 0);
    JetTerm(inst);
    printf(fails ? "RESULT: FAIL (%d)\n" : "RESULT: PASS\n", fails);
    return fails != 0;
}
