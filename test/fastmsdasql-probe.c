#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <oledb.h>
#include <oledberr.h>
#include <stdio.h>
#include <stddef.h>

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static const GUID CLSID_ENUM = {0xc8b522cd,0x5cf3,0x11ce,{0xad,0xe5,0x00,0xaa,0x00,0x44,0x77,0x3d}};
static const GUID IID_SRC = {0x0c733a1e,0x2a1c,0x11ce,{0xad,0xe5,0x00,0xaa,0x00,0x44,0x77,0x3d}};
static const GUID IID_RS = {0x0c733a7c,0x2a1c,0x11ce,{0xad,0xe5,0x00,0xaa,0x00,0x44,0x77,0x3d}};
static const GUID IID_ACC = {0x0c733a8c,0x2a1c,0x11ce,{0xad,0xe5,0x00,0xaa,0x00,0x44,0x77,0x3d}};
static const GUID IID_CI = {0x0c733a11,0x2a1c,0x11ce,{0xad,0xe5,0x00,0xaa,0x00,0x44,0x77,0x3d}};
static const GUID IID_RI = {0x0c733a55,0x2a1c,0x11ce,{0xad,0xe5,0x00,0xaa,0x00,0x44,0x77,0x3d}};

struct row
{
    DBSTATUS name_status;
    DBLENGTH name_len;
    WCHAR name[64];
    USHORT type;
    DBSTATUS type_status;
    VARIANT_BOOL isparent;
};

int main(void)
{
    ISourcesRowset *src;
    IRowset *rs;
    IAccessor *acc;
    IColumnsInfo *ci;
    IRowsetInfo *ri;
    DBBINDING b[3];
    DBBINDSTATUS bs[3];
    HACCESSOR h = 0, h2 = 0;
    DBREFCOUNT refs;
    DBCOUNTITEM got = 0;
    HROW *rows = NULL;
    struct row row;
    DBORDINAL cols, mapped;
    DBCOLUMNINFO *info;
    OLECHAR *buf;
    DBID id;
    DBACCESSORFLAGS flags;
    DBCOUNTITEM nb;
    DBBINDING *back;
    HRESULT hr;
    int found = 0;

    CoInitialize(NULL);
    hr = CoCreateInstance(&CLSID_ENUM, NULL, CLSCTX_INPROC_SERVER, &IID_SRC, (void **)&src);
    CHECK(hr == S_OK, "enumerator %08lx", hr);
    if (hr != S_OK) goto done;
    hr = ISourcesRowset_GetSourcesRowset(src, NULL, &IID_RS, 0, NULL, (IUnknown **)&rs);
    CHECK(hr == S_OK, "rowset %08lx", hr);

    hr = IRowset_QueryInterface(rs, &IID_CI, (void **)&ci);
    CHECK(hr == S_OK, "colinfo");
    hr = IColumnsInfo_GetColumnInfo(ci, &cols, &info, &buf);
    CHECK(hr == S_OK && cols == 6, "columns %08lx %Iu", hr, cols);
    id.eKind = DBKIND_NAME;
    id.uName.pwszName = (WCHAR *)L"sources_type";
    hr = IColumnsInfo_MapColumnIDs(ci, 1, &id, &mapped);
    CHECK(hr == S_OK && mapped == 4, "map %08lx %Iu", hr, mapped);
    id.uName.pwszName = (WCHAR *)L"nonexistent";
    hr = IColumnsInfo_MapColumnIDs(ci, 1, &id, &mapped);
    CHECK(FAILED(hr) || hr == DB_S_ERRORSOCCURRED, "map bad %08lx", hr);
    CoTaskMemFree(info);
    CoTaskMemFree(buf);
    IColumnsInfo_Release(ci);

    hr = IRowset_QueryInterface(rs, &IID_RI, (void **)&ri);
    CHECK(hr == S_OK, "rowsetinfo");
    if (hr == S_OK) IRowsetInfo_Release(ri);

    hr = IRowset_QueryInterface(rs, &IID_ACC, (void **)&acc);
    CHECK(hr == S_OK, "accessor");
    memset(b, 0, sizeof(b));
    b[0].iOrdinal = 1; b[0].dwPart = DBPART_VALUE | DBPART_LENGTH | DBPART_STATUS; b[0].wType = DBTYPE_WSTR;
    b[0].obValue = offsetof(struct row, name); b[0].obLength = offsetof(struct row, name_len);
    b[0].obStatus = offsetof(struct row, name_status); b[0].cbMaxLen = sizeof(row.name);
    b[1].iOrdinal = 4; b[1].dwPart = DBPART_VALUE | DBPART_STATUS; b[1].wType = DBTYPE_UI2;
    b[1].obValue = offsetof(struct row, type); b[1].obStatus = offsetof(struct row, type_status); b[1].cbMaxLen = 2;
    b[2].iOrdinal = 5; b[2].dwPart = DBPART_VALUE; b[2].wType = DBTYPE_BOOL;
    b[2].obValue = offsetof(struct row, isparent); b[2].cbMaxLen = 2;

    hr = IAccessor_CreateAccessor(acc, DBACCESSOR_ROWDATA, 3, b, sizeof(row), &h, bs);
    CHECK(hr == S_OK && h, "create %08lx", hr);
    hr = IAccessor_GetBindings(acc, h, &flags, &nb, &back);
    CHECK(hr == S_OK && flags == DBACCESSOR_ROWDATA && nb == 3 && back[1].iOrdinal == 4 && back[0].wType == DBTYPE_WSTR,
          "getbindings %08lx", hr);
    CoTaskMemFree(back);
    hr = IAccessor_AddRefAccessor(acc, h, &refs);
    CHECK(hr == S_OK && refs == 2, "addref accessor %08lx %lu", hr, (unsigned long)refs);
    hr = IAccessor_ReleaseAccessor(acc, h, &refs);
    CHECK(hr == S_OK && refs == 1, "release 1 %08lx", hr);

    /* invalid bindings are reported per binding */
    b[2].iOrdinal = 99;
    hr = IAccessor_CreateAccessor(acc, DBACCESSOR_ROWDATA, 3, b, sizeof(row), &h2, bs);
    CHECK(hr == DB_E_ERRORSOCCURRED && bs[2] == DBBINDSTATUS_BADORDINAL && bs[0] == DBBINDSTATUS_OK, "bad ordinal %08lx", hr);
    hr = IAccessor_CreateAccessor(acc, 0, 1, b, sizeof(row), &h2, NULL);
    CHECK(hr == DB_E_BADACCESSORFLAGS, "bad flags %08lx", hr);
    hr = IAccessor_ReleaseAccessor(acc, 12345, NULL);
    CHECK(hr == DB_E_BADACCESSORHANDLE, "bad handle %08lx", hr);

    hr = IRowset_GetData(rs, 77, h, &row);
    CHECK(hr == DB_E_BADROWHANDLE, "bad row %08lx", hr);
    hr = IRowset_GetNextRows(rs, 0, 0, -1, &got, &rows);
    CHECK(hr == DB_E_CANTFETCHBACKWARDS, "backwards %08lx", hr);

    for (;;)
    {
        hr = IRowset_GetNextRows(rs, 0, 0, 5, &got, &rows);
        CHECK(hr == S_OK || hr == DB_S_ENDOFROWSET, "next %08lx", hr);
        if (got)
        {
            DBCOUNTITEM i;
            for (i = 0; i < got; i++)
            {
                memset(&row, 0xcc, sizeof(row));
                hr = IRowset_GetData(rs, rows[i], h, &row);
                CHECK(hr == S_OK, "getdata %08lx", hr);
                CHECK(row.name_status == DBSTATUS_S_OK && row.name_len == lstrlenW(row.name) * sizeof(WCHAR) && row.name[0],
                      "name %ls len %Iu st %lu", row.name, row.name_len, (unsigned long)row.name_status);
                CHECK(row.type == DBSOURCETYPE_DATASOURCE && row.type_status == DBSTATUS_S_OK, "type %u", row.type);
                CHECK(row.isparent == VARIANT_FALSE, "isparent");
                found++;
            }
            hr = IRowset_ReleaseRows(rs, got, rows, NULL, NULL, NULL);
            CHECK(hr == S_OK, "release rows %08lx", hr);
            hr = IRowset_ReleaseRows(rs, 1, rows, NULL, NULL, NULL);
            CHECK(hr == DB_E_ERRORSOCCURRED, "double release %08lx", hr);
            CoTaskMemFree(rows);
            rows = NULL;
        }
        if (hr == DB_S_ENDOFROWSET || !got) break;
    }
    printf("sources found: %d\n", found);
    CHECK(found == 2, "expected the two registered data sources, got %d", found);
    hr = IRowset_RestartPosition(rs, 0);
    CHECK(hr == S_OK || hr == DB_E_CANTSCROLLBACKWARDS, "restart %08lx", hr);
    if (hr == S_OK && found)
    {
        hr = IRowset_GetNextRows(rs, 0, 0, 1, &got, &rows);
        CHECK(got == 1, "again");
        IRowset_ReleaseRows(rs, got, rows, NULL, NULL, NULL);
        CoTaskMemFree(rows);
    }
    IAccessor_ReleaseAccessor(acc, h, &refs);
    CHECK(refs == 0, "accessor gone");
    IAccessor_Release(acc);
    IRowset_Release(rs);
    ISourcesRowset_Release(src);
done:
    CoUninitialize();
    printf(fails ? "RESULT: FAIL (%d)\n" : "RESULT: PASS\n", fails);
    return fails != 0;
}
