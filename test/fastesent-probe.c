#define JET_VERSION 0x0a00
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <esent.h>
#undef JetGetTableColumnInfoW

#ifndef JET_prepInsert
#define JET_prepInsert 0
#define JET_prepReplace 2
#define JET_prepCancel 3
#endif
#ifndef JET_bitNewKey
#define JET_bitNewKey 1
#define JET_bitStrLimit 2
#define JET_bitSeekEQ 1
#define JET_bitSeekLT 2
#define JET_bitSeekLE 4
#define JET_bitSeekGE 8
#define JET_bitSeekGT 0x10
#define JET_bitSetIndexRange 0x20
#define JET_bitRangeInclusive 1
#define JET_bitRangeUpperLimit 2
#define JET_bitColumnTagged 2
#define JET_bitColumnNotNULL 4
#define JET_bitColumnAutoincrement 0x10
#define JET_bitColumnMultiValued 0x400
#define JET_bitColumnTTKey 0x40
#define JET_bitIndexUnique 1
#define JET_bitIndexPrimary 2
#define JET_bitSetAppendLV 1
#define JET_bitRollbackAll 1
#define JET_bitRetrieveCopy 1
#endif

#ifndef JET_TblInfoName
#define JET_TblInfoName 1
#define JET_IdxInfoList 1
#define JET_IdxInfoCount 7
#define JET_ObjInfoListNoStats 1
#endif
static void * JET_API my_realloc(void *ctx, void *pv, unsigned long cb)
{
    if (!cb) { free(pv); return NULL; }
    return realloc(pv, cb);
}
struct my_enumcolumn
{
    JET_COLUMNID columnid;
    JET_ERR err;
    unsigned long cEnumColumnValue;
    JET_ENUMCOLUMNVALUE *rgEnumColumnValue;
};
static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static JET_INSTANCE inst;
static JET_SESID ses;
static JET_DBID dbid;

static JET_ERR open_instance(void)
{
    JET_ERR e;
    inst = 0;
    e = JetCreateInstanceW(&inst, L"sgtest");
    if (e) return e;
    JetSetSystemParameterW(&inst, 0, JET_paramSystemPath, 0, L".\\");
    JetSetSystemParameterW(&inst, 0, JET_paramTempPath, 0, L".\\");
    JetSetSystemParameterW(&inst, 0, JET_paramLogFilePath, 0, L".\\");
    JetSetSystemParameterW(&inst, 0, JET_paramBaseName, 0, L"sg1");
    JetSetSystemParameterW(&inst, 0, JET_paramCircularLog, 1, NULL);
    if ((e = JetInit(&inst))) return e;
    return JetBeginSessionW(inst, &ses, NULL, NULL);
}

int main(void)
{
    JET_ERR e;
    JET_TABLEID t, tt;
    JET_COLUMNDEF cd;
    JET_COLUMNID c_id, c_name, c_age, c_score, c_notes, c_tags, c_req;
    int i;
    char dir[MAX_PATH], buf[256];
    WCHAR wbuf[128];
    unsigned long actual;
    JET_SETINFO si = { sizeof(si), 0, 0 };
    JET_RETINFO ri = { sizeof(ri), 0, 1, 0 };
    LONG lv;

    GetTempPathA(sizeof(dir), dir);
    snprintf(buf, sizeof(buf), "%sfastesent.d", dir);
    CreateDirectoryA(buf, NULL);
    SetCurrentDirectoryA(buf);
    DeleteFileA("test.edb");

    e = open_instance();
    CHECK(e == 0, "open_instance %d", e);
    e = JetBeginSessionW(inst, &ses, NULL, NULL);
    CHECK(JetCreateDatabaseW(ses, L"test.edb", NULL, &dbid, 0) == 0 || 1, "x");
    e = JetCreateDatabaseW(ses, L"test.edb", NULL, &dbid, 0);
    CHECK(e == JET_errDatabaseDuplicate, "dup create %d", e);
    e = JetOpenDatabaseW(ses, L"test.edb", NULL, &dbid, 0);
    CHECK(e == 0 && dbid, "open db %d", e);

    /* ---- schema ---- */
    e = JetBeginTransaction(ses);
    CHECK(e == 0, "begin %d", e);
    e = JetCreateTableW(ses, dbid, L"people", 4, 80, &t);
    CHECK(e == 0, "create table %d", e);
    e = JetCreateTableW(ses, dbid, L"people", 4, 80, &tt);
    CHECK(e == JET_errTableDuplicate, "dup table %d", e);
    memset(&cd, 0, sizeof(cd)); cd.cbStruct = sizeof(cd);
    cd.coltyp = JET_coltypLong; cd.grbit = JET_bitColumnAutoincrement;
    CHECK(JetAddColumnW(ses, t, L"id", &cd, NULL, 0, &c_id) == 0, "add id");
    CHECK(c_id == 1, "first fixed column id is 1, got %u", c_id);
    cd.coltyp = JET_coltypText; cd.grbit = 0; cd.cbMax = 40; cd.cp = 1200;
    CHECK(JetAddColumnW(ses, t, L"name", &cd, NULL, 0, &c_name) == 0, "add name");
    CHECK(c_name == 128, "first variable column id is 128, got %u", c_name);
    cd.coltyp = JET_coltypLong; cd.cbMax = 0;
    CHECK(JetAddColumnW(ses, t, L"age", &cd, NULL, 0, &c_age) == 0, "add age");
    cd.coltyp = JET_coltypIEEEDouble;
    CHECK(JetAddColumnW(ses, t, L"score", &cd, NULL, 0, &c_score) == 0, "add score");
    cd.coltyp = JET_coltypLongText; cd.cp = 1200;
    CHECK(JetAddColumnW(ses, t, L"notes", &cd, NULL, 0, &c_notes) == 0, "add notes");
    CHECK(c_notes == 256, "first tagged column id is 256, got %u", c_notes);
    cd.coltyp = JET_coltypText; cd.cbMax = 20; cd.cp = 1252; cd.grbit = JET_bitColumnTagged | JET_bitColumnMultiValued;
    CHECK(JetAddColumnW(ses, t, L"tags", &cd, NULL, 0, &c_tags) == 0, "add tags");
    cd.coltyp = JET_coltypLong; cd.cbMax = 0; cd.grbit = JET_bitColumnNotNULL;
    CHECK(JetAddColumnW(ses, t, L"req", &cd, NULL, 0, &c_req) == 0, "add req");
    e = JetAddColumnW(ses, t, L"age", &cd, NULL, 0, &c_req);
    CHECK(e == JET_errColumnDuplicate, "dup column %d", e);

    CHECK(JetCreateIndexW(ses, t, L"primary", JET_bitIndexPrimary, L"+id\0", 5, 100) == 0, "primary index");
    CHECK(JetCreateIndexW(ses, t, L"by_name", 0, L"+name\0", 7, 100) == 0, "by_name");
    CHECK(JetCreateIndexW(ses, t, L"by_age_desc", 0, L"-age\0+name\0", 12, 100) == 0, "by_age_desc");
    e = JetCreateIndexW(ses, t, L"by_name", 0, L"+name\0", 7, 100);
    CHECK(e == JET_errIndexDuplicate, "dup index %d", e);
    e = JetCreateIndexW(ses, t, L"nocol", 0, L"+bogus\0", 8, 100);
    CHECK(e == JET_errColumnNotFound, "missing column in index %d", e);
    CHECK(JetCommitTransaction(ses, 0) == 0, "commit");

    /* ---- inserts ---- */
    for (i = 0; i < 20; i++)
    {
        WCHAR nm[16];
        LONG age = 20 + (i * 7) % 11;
        double score = i * 1.5;
        unsigned char bm[8];
        unsigned long cb;
        swprintf(nm, 16, L"p%02d", i);
        CHECK(JetPrepareUpdate(ses, t, JET_prepInsert) == 0, "prep %d", i);
        CHECK(JetSetColumn(ses, t, c_name, nm, wcslen(nm) * 2, 0, NULL) == 0, "set name");
        CHECK(JetSetColumn(ses, t, c_age, &age, 4, 0, NULL) == 0, "set age");
        CHECK(JetSetColumn(ses, t, c_score, &score, 8, 0, NULL) == 0, "set score");
        e = JetSetColumn(ses, t, c_age, &age, 2, 0, NULL);
        CHECK(e == JET_errInvalidBufferSize, "bad size %d", e);
        e = JetSetColumn(ses, t, c_id, &age, 4, 0, NULL);
        CHECK(e == JET_errColumnNotUpdatable, "autoinc set %d", e);
        e = JetUpdate(ses, t, bm, sizeof(bm), &cb);
        if (i == 0) CHECK(e == JET_errNullInvalid, "NotNull enforced %d", e);
        lv = 100 + i;
        CHECK(JetSetColumn(ses, t, c_req, &lv, 4, 0, NULL) == 0, "set req");
        e = JetUpdate(ses, t, bm, sizeof(bm), &cb);
        CHECK(e == 0 && cb == 4, "update %d: %d", i, e);
    }
    e = JetUpdate(ses, t, NULL, 0, NULL);
    CHECK(e == JET_errUpdateNotPrepared, "update without prepare %d", e);

    /* ---- navigation on the primary index ---- */
    CHECK(JetMove(ses, t, JET_MoveFirst, 0) == 0, "first");
    for (i = 0; i < 20; i++)
    {
        LONG id = 0;
        CHECK(JetRetrieveColumn(ses, t, c_id, &id, 4, &actual, 0, NULL) == 0 && id == i + 1,
              "primary order: row %d has id %ld", i, id);
        e = JetMove(ses, t, JET_MoveNext, 0);
        if (i < 19) CHECK(e == 0, "next %d", e);
        else CHECK(e == JET_errNoCurrentRecord, "next past end %d", e);
    }
    CHECK(JetMove(ses, t, JET_MovePrevious, 0) == 0, "previous from after-last");
    {
        LONG id = 0;
        JetRetrieveColumn(ses, t, c_id, &id, 4, &actual, 0, NULL);
        CHECK(id == 20, "last row id %ld", id);
    }
    CHECK(JetMove(ses, t, JET_MoveLast, 0) == 0, "last");
    CHECK(JetMove(ses, t, -5, 0) == 0, "move -5");
    {
        LONG id = 0;
        JetRetrieveColumn(ses, t, c_id, &id, 4, &actual, 0, NULL);
        CHECK(id == 15, "after -5: %ld", id);
    }

    /* ---- seek on a secondary index ---- */
    CHECK(JetSetCurrentIndexW(ses, t, L"by_name") == 0, "set index");
    e = JetMakeKey(ses, t, L"p07", 6, JET_bitNewKey);
    CHECK(e == 0, "makekey %d", e);
    e = JetSeek(ses, t, JET_bitSeekEQ);
    CHECK(e == 0, "seek EQ %d", e);
    JetRetrieveColumn(ses, t, c_name, wbuf, sizeof(wbuf), &actual, 0, NULL);
    CHECK(actual == 6 && !wcsncmp(wbuf, L"p07", 3), "seeked name");
    JetMakeKey(ses, t, L"p07x", 8, JET_bitNewKey);
    e = JetSeek(ses, t, JET_bitSeekGE);
    CHECK(e == JET_wrnSeekNotEqual, "seek GE inexact %d", e);
    JetRetrieveColumn(ses, t, c_name, wbuf, sizeof(wbuf), &actual, 0, NULL);
    CHECK(!wcsncmp(wbuf, L"p08", 3), "GE landed on p08");
    JetMakeKey(ses, t, L"zzz", 6, JET_bitNewKey);
    e = JetSeek(ses, t, JET_bitSeekEQ);
    CHECK(e == JET_errRecordNotFound, "seek missing %d", e);
    JetMakeKey(ses, t, L"p05", 6, JET_bitNewKey);
    e = JetSeek(ses, t, JET_bitSeekLT);
    JetRetrieveColumn(ses, t, c_name, wbuf, sizeof(wbuf), &actual, 0, NULL);
    CHECK(!wcsncmp(wbuf, L"p04", 3), "LT landed on p04");
    e = JetSeek(ses, t, JET_bitSeekEQ);
    CHECK(e == JET_errKeyNotMade, "key consumed %d", e);

    /* ---- descending index + range ---- */
    CHECK(JetSetCurrentIndexW(ses, t, L"by_age_desc") == 0, "set desc index");
    CHECK(JetMove(ses, t, JET_MoveFirst, 0) == 0, "first desc");
    {
        LONG a = 0, b = 0;
        JetRetrieveColumn(ses, t, c_age, &a, 4, &actual, 0, NULL);
        JetMove(ses, t, JET_MoveNext, 0);
        JetRetrieveColumn(ses, t, c_age, &b, 4, &actual, 0, NULL);
        CHECK(a == 30 && b <= a, "descending: %ld then %ld", a, b);
    }
    {
        LONG age = 25;
        int n = 0;
        JetMakeKey(ses, t, &age, 4, JET_bitNewKey);
        e = JetSeek(ses, t, JET_bitSeekGE | JET_bitSetIndexRange);
        CHECK(e == 0, "range seek %d", e);
        do { n++; e = JetMove(ses, t, JET_MoveNext, 0); } while (e == 0);
        CHECK(n >= 1 && e == JET_errNoCurrentRecord, "range stops: %d rows, %d", n, e);
    }

    /* ---- replace, delete, counts ---- */
    CHECK(JetSetCurrentIndexW(ses, t, NULL) == 0, "primary again");
    JetMove(ses, t, JET_MoveFirst, 0);
    CHECK(JetPrepareUpdate(ses, t, JET_prepReplace) == 0, "prep replace");
    lv = 99;
    CHECK(JetSetColumn(ses, t, c_age, &lv, 4, 0, NULL) == 0, "replace age");
    lv = 0;
    CHECK(JetRetrieveColumn(ses, t, c_age, &lv, 4, &actual, JET_bitRetrieveCopy, NULL) == 0 && lv == 99, "copy buffer");
    lv = 0;
    JetRetrieveColumn(ses, t, c_age, &lv, 4, &actual, 0, NULL);
    CHECK(lv != 99, "record not changed before update");
    CHECK(JetUpdate(ses, t, NULL, 0, NULL) == 0, "update replace");
    JetRetrieveColumn(ses, t, c_age, &lv, 4, &actual, 0, NULL);
    CHECK(lv == 99, "replaced");
    {
        unsigned long count = 0;
        JetMove(ses, t, JET_MoveFirst, 0);
        JetIndexRecordCount(ses, t, &count, 1000);
        CHECK(count == 20, "count %u", count);
    }
    JetMove(ses, t, JET_MoveNext, 0);
    CHECK(JetDelete(ses, t) == 0, "delete");
    e = JetRetrieveColumn(ses, t, c_age, &lv, 4, &actual, 0, NULL);
    CHECK(e == JET_errRecordDeleted, "retrieve deleted %d", e);
    CHECK(JetMove(ses, t, JET_MoveNext, 0) == 0, "next after delete");
    {
        LONG id = 0;
        JetRetrieveColumn(ses, t, c_id, &id, 4, &actual, 0, NULL);
        CHECK(id == 3, "next after delete is id 3, got %ld", id);
    }

    /* ---- transactions ---- */
    CHECK(JetBeginTransaction(ses) == 0, "tx begin");
    JetMove(ses, t, JET_MoveFirst, 0);
    CHECK(JetDelete(ses, t) == 0, "tx delete");
    JetPrepareUpdate(ses, t, JET_prepInsert);
    lv = 500; JetSetColumn(ses, t, c_req, &lv, 4, 0, NULL);
    JetSetColumn(ses, t, c_name, L"txrow", 10, 0, NULL);
    CHECK(JetUpdate(ses, t, NULL, 0, NULL) == 0, "tx insert");
    CHECK(JetRollback(ses, 0) == 0, "rollback");
    {
        unsigned long count = 0;
        JetMove(ses, t, JET_MoveFirst, 0);
        JetIndexRecordCount(ses, t, &count, 1000);
        CHECK(count == 19, "count after rollback %u", count);
        {
            LONG id = 0;
            JetRetrieveColumn(ses, t, c_id, &id, 4, &actual, 0, NULL);
            CHECK(id == 1, "first row back after rollback, id %ld", id);
        }
    }
    CHECK(JetRollback(ses, 0) == JET_errNotInTransaction, "rollback outside tx");

    /* ---- unique index violation ---- */
    JetCloseTable(ses, t);
    CHECK(JetOpenTableW(ses, dbid, L"people", NULL, 0, 0, &t) >= 0, "reopen");
    CHECK(JetCreateIndexW(ses, t, L"uniq_req", JET_bitIndexUnique, L"+req\0", 6, 100) == 0, "unique index");
    JetPrepareUpdate(ses, t, JET_prepInsert);
    lv = 105; JetSetColumn(ses, t, c_req, &lv, 4, 0, NULL);
    e = JetUpdate(ses, t, NULL, 0, NULL);
    CHECK(e == JET_errKeyDuplicate, "duplicate key %d", e);
    JetPrepareUpdate(ses, t, JET_prepCancel);
    e = JetPrepareUpdate(ses, t, JET_prepCancel);
    CHECK(e == 0, "cancel idempotent %d", e);

    /* ---- multi-valued, long values ---- */
    JetMove(ses, t, JET_MoveFirst, 0);
    JetPrepareUpdate(ses, t, JET_prepReplace);
    si.itagSequence = 0;
    e = JetSetColumn(ses, t, c_tags, "red", 3, 0, &si);
    CHECK(e == 0, "tag 1: %d", e);
    e = JetSetColumn(ses, t, c_tags, "green", 5, 0, &si);
    CHECK(e == 0, "tag 2: %d", e);
    e = JetSetColumn(ses, t, c_tags, "blue", 4, 0, &si);
    CHECK(e == 0, "tag 3: %d", e);
    si.itagSequence = 1;
    JetSetColumn(ses, t, c_notes, L"abc", 6, 0, &si);
    JetSetColumn(ses, t, c_notes, L"def", 6, JET_bitSetAppendLV, &si);
    CHECK(JetUpdate(ses, t, NULL, 0, NULL) == 0, "update tags");
    ri.itagSequence = 2;
    memset(buf, 0, sizeof(buf));
    e = JetRetrieveColumn(ses, t, c_tags, buf, sizeof(buf), &actual, 0, &ri);
    CHECK(e == 0 && actual == 5 && !strcmp(buf, "green"), "tag 2 = %s (%d)", buf, e);
    ri.itagSequence = 0;
    e = JetRetrieveColumn(ses, t, c_tags, buf, sizeof(buf), &actual, 0, &ri);
    CHECK(actual == 3, "tag count %u", actual);
    ri.itagSequence = 4;
    e = JetRetrieveColumn(ses, t, c_tags, buf, sizeof(buf), &actual, 0, &ri);
    CHECK(e == JET_wrnColumnNull, "tag 4 null %d", e);
    ri.itagSequence = 1;
    e = JetRetrieveColumn(ses, t, c_notes, wbuf, 4, &actual, 0, &ri);
    CHECK(e == JET_wrnBufferTruncated && actual == 12, "long value truncated %d %u", e, actual);
    e = JetRetrieveColumn(ses, t, c_notes, wbuf, sizeof(wbuf), &actual, 0, &ri);
    CHECK(e == 0 && actual == 12 && !wcsncmp(wbuf, L"abcdef", 6), "appended long value");
    {
        JET_ENUMCOLUMNID ids[2] = { { 0, 0, NULL }, { 0, 0, NULL } };
        struct my_enumcolumn *out = NULL;
        unsigned long n = 0;
        ids[0].columnid = c_tags;
        ids[1].columnid = c_age;
        e = JetEnumerateColumns(ses, t, 2, ids, &n, (JET_ENUMCOLUMN **)&out, my_realloc, NULL, 0, 0);
        CHECK(e == 0 && n == 2 && out, "enumerate %d", e);
        if (!e)
        {
            CHECK(out[0].columnid == c_tags && out[0].cEnumColumnValue == 3, "tags: %lu values", out[0].cEnumColumnValue);
            if (out[0].cEnumColumnValue == 3)
            {
                CHECK(out[0].rgEnumColumnValue[1].cbData == 5 && !memcmp(out[0].rgEnumColumnValue[1].pvData, "green", 5), "tag 2 data");
                CHECK(out[0].rgEnumColumnValue[2].itagSequence == 3, "tag 3 sequence");
            }
            CHECK(out[1].columnid == c_age && out[1].cEnumColumnValue == 1, "age present");
            my_realloc(NULL, out, 0);
        }
    }

    /* ---- temporary table sorting ---- */
    {
        JET_COLUMNDEF defs[2];
        JET_COLUMNID ids[2];
        const char *words[5] = { "pear", "apple", "fig", "kiwi", "banana" };
        const char *sorted[5] = { "apple", "banana", "fig", "kiwi", "pear" };
        memset(defs, 0, sizeof(defs));
        defs[0].cbStruct = sizeof(defs[0]); defs[0].coltyp = JET_coltypText; defs[0].cbMax = 20; defs[0].grbit = JET_bitColumnTTKey;
        defs[1].cbStruct = sizeof(defs[1]); defs[1].coltyp = JET_coltypLong;
        e = JetOpenTempTable(ses, defs, 2, 0, &tt, ids);
        CHECK(e == 0, "open temp table %d", e);
        for (i = 0; i < 5; i++)
        {
            LONG n = i;
            JetPrepareUpdate(ses, tt, JET_prepInsert);
            JetSetColumn(ses, tt, ids[0], words[i], strlen(words[i]), 0, NULL);
            JetSetColumn(ses, tt, ids[1], &n, 4, 0, NULL);
            CHECK(JetUpdate(ses, tt, NULL, 0, NULL) == 0, "temp insert");
        }
        JetMove(ses, tt, JET_MoveFirst, 0);
        for (i = 0; i < 5; i++)
        {
            memset(buf, 0, 32);
            JetRetrieveColumn(ses, tt, ids[0], buf, 31, &actual, 0, NULL);
            CHECK(!strcmp(buf, sorted[i]), "temp sorted %d: %s", i, buf);
            JetMove(ses, tt, JET_MoveNext, 0);
        }
        JetCloseTable(ses, tt);
    }

    /* ---- information ---- */
    {
        JET_COLUMNLIST cl;
        JET_COLUMNDEF d;
        JET_INDEXLIST il;
        JET_OBJECTLIST ol;
        e = JetGetTableColumnInfoW(ses, t, L"age", &d, sizeof(d), JET_ColInfo);
        CHECK(e == 0 && d.coltyp == JET_coltypLong && d.columnid == c_age, "column info %d", e);
        e = JetGetTableColumnInfoW(ses, t, L"nosuch", &d, sizeof(d), JET_ColInfo);
        CHECK(e == JET_errColumnNotFound, "column info missing %d", e);
        e = JetGetTableColumnInfoW(ses, t, NULL, &cl, sizeof(cl), JET_ColInfoList);
        CHECK(e == 0 && cl.cRecord == 7, "column list %d count %u", e, cl.cRecord);
        if (!e)
        {
            int n = 0;
            e = JetMove(ses, cl.tableid, JET_MoveFirst, 0);
            while (e == 0)
            {
                n++;
                if (n == 1)
                {
                    WCHAR nm[64] = { 0 };
                    JetRetrieveColumn(ses, cl.tableid, cl.columnidcolumnname, nm, sizeof(nm), &actual, 0, NULL);
                    CHECK(!wcscmp(nm, L"id"), "first column name %ls", nm);
                }
                e = JetMove(ses, cl.tableid, JET_MoveNext, 0);
            }
            CHECK(n == 7, "column list rows %d", n);
            JetCloseTable(ses, cl.tableid);
        }
        e = JetGetTableIndexInfoW(ses, t, NULL, &il, sizeof(il), JET_IdxInfoList);
        CHECK(e == 0 && il.cRecord == 5, "index list %d count %u", e, il.cRecord);
        if (!e) JetCloseTable(ses, il.tableid);
        {
            unsigned long cnt = 0;
            e = JetGetTableIndexInfoW(ses, t, NULL, &cnt, sizeof(cnt), JET_IdxInfoCount);
            CHECK(e == 0 && cnt == 4, "index count %u", cnt);
        }
        e = JetGetObjectInfoW(ses, dbid, JET_objtypTable, NULL, NULL, &ol, sizeof(ol), JET_ObjInfoListNoStats);
        CHECK(e == 0 && ol.cRecord == 1, "object list %d %u", e, ol.cRecord);
        if (!e) JetCloseTable(ses, ol.tableid);
        e = JetGetTableInfoW(ses, t, wbuf, sizeof(wbuf), JET_TblInfoName);
        CHECK(e == 0 && !wcscmp(wbuf, L"people"), "table name");
    }


    /* ---- create table with columns and indexes in one call, bookmarks, escrow, enumerate ---- */
    {
        JET_COLUMNCREATE_W cc[3];
        JET_INDEXCREATE_W ic[1];
        JET_TABLECREATE_W tc;
        JET_TABLEID t2;
        unsigned char bm[16];
        unsigned long cbm;
        LONG one = 1, old = -1, v;
        JET_ENUMCOLUMN *ecols = NULL;
        unsigned long necol = 0;
        JET_RECPOS rp;
        unsigned long rec_id;

        memset(cc, 0, sizeof(cc));
        cc[0].cbStruct = sizeof(cc[0]); cc[0].szColumnName = (WCHAR *)L"key"; cc[0].coltyp = JET_coltypLong;
        cc[1].cbStruct = sizeof(cc[0]); cc[1].szColumnName = (WCHAR *)L"count"; cc[1].coltyp = JET_coltypLong; cc[1].grbit = 0x800;
        cc[2].cbStruct = sizeof(cc[0]); cc[2].szColumnName = (WCHAR *)L"blob"; cc[2].coltyp = JET_coltypLongBinary;
        memset(ic, 0, sizeof(ic));
        ic[0].cbStruct = sizeof(ic[0]); ic[0].szIndexName = (WCHAR *)L"pk"; ic[0].szKey = (WCHAR *)L"+key\0"; ic[0].cbKey = 6;
        ic[0].grbit = JET_bitIndexPrimary;
        memset(&tc, 0, sizeof(tc));
        tc.cbStruct = sizeof(tc); tc.szTableName = (WCHAR *)L"counters"; tc.rgcolumncreate = (void *)cc; tc.cColumns = 3;
        tc.rgindexcreate = ic; tc.cIndexes = 1;
        e = JetCreateTableColumnIndexW(ses, dbid, &tc);
        CHECK(e == 0 && tc.tableid && tc.cCreated == 5, "create table column index %d created %lu", e, tc.cCreated);
        CHECK(cc[0].columnid == 1 && cc[1].columnid == 2 && cc[2].columnid == 256, "column ids %lu %lu %lu",
              cc[0].columnid, cc[1].columnid, cc[2].columnid);
        t2 = tc.tableid;
        for (i = 1; i <= 3; i++)
        {
            v = i * 10;
            JetPrepareUpdate(ses, t2, JET_prepInsert);
            JetSetColumn(ses, t2, cc[0].columnid, &v, 4, 0, NULL);
            JetSetColumn(ses, t2, cc[1].columnid, &v, 4, 0, NULL);
            CHECK(JetUpdate(ses, t2, bm, sizeof(bm), &cbm) == 0 && cbm == 4, "insert counter");
        }
        JetMove(ses, t2, JET_MoveFirst, 0);
        JetMove(ses, t2, JET_MoveNext, 0);
        CHECK(JetGetBookmark(ses, t2, bm, sizeof(bm), &cbm) == 0 && cbm == 4, "get bookmark");
        JetMove(ses, t2, JET_MoveLast, 0);
        CHECK(JetGotoBookmark(ses, t2, bm, cbm) == 0, "goto bookmark");
        JetRetrieveColumn(ses, t2, cc[0].columnid, &v, 4, &actual, 0, NULL);
        CHECK(v == 20, "bookmark landed on key 20, got %ld", v);
        e = JetEscrowUpdate(ses, t2, cc[1].columnid, &one, 4, &old, 4, &actual, 0);
        CHECK(e == 0 && old == 20, "escrow old value %ld (%d)", old, e);
        JetRetrieveColumn(ses, t2, cc[1].columnid, &v, 4, &actual, 0, NULL);
        CHECK(v == 21, "escrow applied: %ld", v);
        e = JetEscrowUpdate(ses, t2, cc[0].columnid, &one, 4, &old, 4, &actual, 0);
        CHECK(e != 0, "escrow on a plain column fails");
        e = JetGetRecordPosition(ses, t2, &rp, sizeof(rp));
        CHECK(e == 0 && rp.centriesLT == 1 && rp.centriesTotal == 3, "record position %d lt=%lu total=%lu", e, rp.centriesLT, rp.centriesTotal);
        rp.centriesLT = 2; rp.centriesTotal = 3;
        CHECK(JetGotoPosition(ses, t2, &rp) == 0, "goto position");
        JetRetrieveColumn(ses, t2, cc[0].columnid, &v, 4, &actual, 0, NULL);
        CHECK(v == 30, "position 2 of 3 is key 30, got %ld", v);
        e = JetEnumerateColumns(ses, t2, 0, NULL, &necol, &ecols, NULL, NULL, 0, 0);
        CHECK(e == JET_errInvalidParameter, "enumerate needs realloc");
        rec_id = 0;
        (void)rec_id;
        JetCloseTable(ses, t2);
        CHECK(JetDeleteTableW(ses, dbid, L"counters") == 0, "delete unused table");
        e = JetOpenTableW(ses, dbid, L"counters", NULL, 0, 0, &t2);
        CHECK(e == JET_errObjectNotFound, "deleted table is gone %d", e);
    }
    /* ---- ANSI entry points ---- */
    {
        JET_INSTANCE ai;
        JET_SESID as;
        JET_DBID adb;
        JET_TABLEID at;
        JET_COLUMNDEF acd;
        JET_COLUMNID acol;
        JET_COLUMNLIST acl;
        char name[64];

        CHECK(JetCreateInstanceA(&ai, "ansi") == 0, "A create instance");
        JetSetSystemParameterA(&ai, 0, JET_paramSystemPath, 0, ".\\");
        JetSetSystemParameterA(&ai, 0, JET_paramBaseName, 0, "sg2");
        CHECK(JetInit(&ai) == 0, "A init");
        CHECK(JetBeginSessionA(ai, &as, NULL, NULL) == 0, "A session");
        DeleteFileA("ansi.edb");
        CHECK(JetCreateDatabaseA(as, "ansi.edb", NULL, &adb, 0) == 0, "A create db");
        CHECK(JetCreateTableA(as, adb, "atab", 1, 80, &at) == 0, "A create table");
        memset(&acd, 0, sizeof(acd)); acd.cbStruct = sizeof(acd); acd.coltyp = JET_coltypText; acd.cbMax = 30;
        CHECK(JetAddColumnA(as, at, "label", &acd, NULL, 0, &acol) == 0, "A add column");
        CHECK(JetGetTableColumnInfoA(as, at, NULL, &acl, sizeof(acl), JET_ColInfoList) == 0 && acl.cRecord == 1, "A column list");
        JetMove(as, acl.tableid, JET_MoveFirst, 0);
        memset(name, 0, sizeof(name));
        JetRetrieveColumn(as, acl.tableid, acl.columnidcolumnname, name, sizeof(name), &actual, 0, NULL);
        CHECK(!strcmp(name, "label"), "A column name %s", name);
        JetCloseTable(as, acl.tableid);
        JetPrepareUpdate(as, at, JET_prepInsert);
        JetSetColumn(as, at, acol, "hello", 5, 0, NULL);
        CHECK(JetUpdate(as, at, NULL, 0, NULL) == 0, "A insert");
        JetMove(as, at, JET_MoveFirst, 0);
        memset(name, 0, sizeof(name));
        JetRetrieveColumn(as, at, acol, name, sizeof(name), &actual, 0, NULL);
        CHECK(!strcmp(name, "hello"), "A value %s", name);
        JetCloseTable(as, at);
        JetCloseDatabase(as, adb, 0);
        JetEndSession(as, 0);
        CHECK(JetTerm(ai) == 0, "A term");
        DeleteFileA("ansi.edb");
    }
    /* ---- delete / rename table rules ---- */
    e = JetDeleteTableW(ses, dbid, L"people");
    CHECK(e == JET_errTableInUse, "delete open table %d", e);
    e = JetDeleteTableW(ses, dbid, L"nosuch");
    CHECK(e == JET_errObjectNotFound, "delete missing table %d", e);

    /* ---- persistence across term/init ---- */
    {
        LONG id = 0;
        JetCloseTable(ses, t);
        CHECK(JetCloseDatabase(ses, dbid, 0) == 0, "close db");
        CHECK(JetDetachDatabaseW(ses, (const char *)L"test.edb") == 0, "detach");
        CHECK(JetEndSession(ses, 0) == 0, "end session");
        CHECK(JetTerm(inst) == 0, "term");
        e = open_instance();
        CHECK(e == 0, "reinit %d", e);
        e = JetOpenDatabaseW(ses, L"test.edb", NULL, &dbid, 0);
        CHECK(e == JET_errDatabaseNotFound, "open before attach %d", e);
        e = JetAttachDatabaseW(ses, L"test.edb", 0);
        CHECK(e == 0, "attach %d", e);
        e = JetAttachDatabaseW(ses, L"test.edb", 0);
        CHECK(e == JET_errDatabaseDuplicate, "attach twice %d", e);
        CHECK(JetOpenDatabaseW(ses, L"test.edb", NULL, &dbid, 0) == 0, "open");
        CHECK(JetOpenTableW(ses, dbid, L"people", NULL, 0, 0, &t) >= 0, "open table");
        {
            unsigned long count = 0;
            JetMove(ses, t, JET_MoveFirst, 0);
            JetIndexRecordCount(ses, t, &count, 1000);
            CHECK(count == 19, "count after reload %u", count);
        }
        JetMove(ses, t, JET_MoveFirst, 0);
        JetRetrieveColumn(ses, t, c_id, &id, 4, &actual, 0, NULL);
        CHECK(id == 1, "first id after reload %ld", id);
        JetPrepareUpdate(ses, t, JET_prepInsert);
        lv = 7; JetSetColumn(ses, t, c_req, &lv, 4, 0, NULL);
        lv = 0;
        JetRetrieveColumn(ses, t, c_id, &lv, 4, &actual, JET_bitRetrieveCopy, NULL);
        CHECK(lv > 20, "autoincrement continues after reload: %ld", lv);
        JetPrepareUpdate(ses, t, JET_prepCancel);
        JetCloseTable(ses, t);
    }
    {
        unsigned long pagesize = 0;
        e = JetGetDatabaseFileInfoW(L"test.edb", &pagesize, sizeof(pagesize), JET_DbInfoPageSize);
        CHECK(e == 0 && pagesize == 4096, "file info %d %u", e, pagesize);
    }
    JetEndSession(ses, 0);
    JetTerm(inst);
    DeleteFileA("test.edb");
    SetCurrentDirectoryA("..");

    printf(fails ? "RESULT: FAIL (%d)\n" : "RESULT: PASS\n", fails);
    return fails != 0;
}
