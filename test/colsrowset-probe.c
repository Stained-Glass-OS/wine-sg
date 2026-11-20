/* colsrowset-probe: OLE DB IColumnsRowset::GetColumnsRowset in MSDASQL (wine-sg 8221; test/colsrowset-gate.sh).
 * A rowset of "SELECT * FROM People" from the gate's mock ODBC driver; its columns rowset has the mandatory
 * columns (DBCOLUMN_IDNAME first) plus the base column and table names asked for, one row per column:
 * Id, Name, Age with their numbers, types and base table People. It was E_NOTIMPL.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#define COBJMACROS
#define DBINITCONSTANTS
#include <windows.h>
#include <oledb.h>
#include <oledberr.h>
#include <stdio.h>
#include <stddef.h>

static const CLSID CLSID_MSDASQL_ = {0xc8b522cb,0x5cf3,0x11ce,{0xad,0xe5,0x00,0xaa,0x00,0x44,0x77,0x3d}};

struct out_row
{
    DBSTATUS name_status; DBLENGTH name_len; WCHAR name[64];
    DBSTATUS number_status; ULONG number;
    DBSTATUS type_status; USHORT type;
    DBSTATUS table_status; DBLENGTH table_len; WCHAR table[64];
    DBSTATUS column_status; DBLENGTH column_len; WCHAR column[64];
};

static void make_binding( DBBINDING *b, DBORDINAL ordinal, DBTYPE type, size_t value, size_t status, size_t len, DBLENGTH max )
{
    memset( b, 0, sizeof(*b) );
    b->iOrdinal = ordinal;
    b->obValue = value;
    b->obStatus = status;
    b->obLength = len;
    b->dwPart = DBPART_VALUE | DBPART_STATUS | (len != (size_t)-1 ? DBPART_LENGTH : 0);
    b->dwMemOwner = DBMEMOWNER_CLIENTOWNED;
    b->eParamIO = DBPARAMIO_NOTPARAM;
    b->cbMaxLen = max;
    b->wType = type;
}

int main( void )
{
    IDBInitialize *init = NULL;
    IDBProperties *props;
    IDBCreateSession *cs;
    IDBCreateCommand *cc;
    ICommandText *text;
    IRowset *rowset = NULL, *cols = NULL;
    IColumnsRowset *colsrs;
    IColumnsInfo *info;
    IAccessor *acc;
    DBPROP prop;
    DBPROPSET set;
    DBID opt[2];
    DBORDINAL ncols = 0, i, n_ord = 0, t_ord = 0, b_ord = 0, num_ord = 0, type_ord = 0;
    DBCOLUMNINFO *colinfo = NULL;
    OLECHAR *strings = NULL;
    DBBINDING binds[5];
    HACCESSOR hacc;
    HROW rows[8], *prow = rows;
    DBCOUNTITEM got = 0;
    DBORDINAL avail = 0;
    DBID *avail_ids = NULL;
    HRESULT hr;
    int ok_rows = 1;

    CoInitialize( NULL );
    setvbuf( stdout, NULL, _IONBF, 0 );
    hr = CoCreateInstance( &CLSID_MSDASQL_, NULL, CLSCTX_INPROC_SERVER, &IID_IDBInitialize, (void **)&init );
    if (FAILED(hr)) { printf( "provider=%08lx\n", hr ); return 1; }
    IDBInitialize_QueryInterface( init, &IID_IDBProperties, (void **)&props );
    memset( &prop, 0, sizeof(prop) );
    prop.dwPropertyID = DBPROP_INIT_PROVIDERSTRING;
    V_VT(&prop.vValue) = VT_BSTR;
    V_BSTR(&prop.vValue) = SysAllocString( L"Driver={SG ADO Mock}" );
    set.guidPropertySet = DBPROPSET_DBINIT;
    set.cProperties = 1;
    set.rgProperties = &prop;
    IDBProperties_SetProperties( props, 1, &set );
    hr = IDBInitialize_Initialize( init );
    printf( "initialize=%08lx\n", hr );
    if (FAILED(hr)) return 1;
    IDBInitialize_QueryInterface( init, &IID_IDBCreateSession, (void **)&cs );
    IDBCreateSession_CreateSession( cs, NULL, &IID_IDBCreateCommand, (IUnknown **)&cc );
    IDBCreateCommand_CreateCommand( cc, NULL, &IID_ICommandText, (IUnknown **)&text );
    ICommandText_SetCommandText( text, &DBGUID_DEFAULT, L"SELECT * FROM People" );
    hr = ICommandText_Execute( text, NULL, &IID_IRowset, NULL, NULL, (IUnknown **)&rowset );
    printf( "execute=%08lx\n", hr );
    if (!rowset) return 1;

    hr = IRowset_QueryInterface( rowset, &IID_IColumnsRowset, (void **)&colsrs );
    if (FAILED(hr)) { printf( "colsrowset_iface=%08lx\n", hr ); return 1; }
    hr = IColumnsRowset_GetAvailableColumns( colsrs, &avail, &avail_ids );
    printf( "available=%d (%08lx %Iu)\n", SUCCEEDED(hr) && avail >= 2, hr, avail );
    CoTaskMemFree( avail_ids );

    opt[0] = DBCOLUMN_BASECOLUMNNAME;
    opt[1] = DBCOLUMN_BASETABLENAME;
    hr = IColumnsRowset_GetColumnsRowset( colsrs, NULL, 2, opt, &IID_IRowset, 0, NULL, (IUnknown **)&cols );
    printf( "columns_rowset=%d (%08lx)\n", SUCCEEDED(hr) && cols, hr );
    if (!cols) { printf( "DONE\n" ); return 0; }

    IRowset_QueryInterface( cols, &IID_IColumnsInfo, (void **)&info );
    IColumnsInfo_GetColumnInfo( info, &ncols, &colinfo, &strings );
    for (i = 0; i < ncols; i++)
    {
        const WCHAR *name = colinfo[i].pwszName ? colinfo[i].pwszName : L"";
        if (!lstrcmpW( name, L"DBCOLUMN_NAME" )) n_ord = colinfo[i].iOrdinal;
        if (!lstrcmpW( name, L"DBCOLUMN_NUMBER" )) num_ord = colinfo[i].iOrdinal;
        if (!lstrcmpW( name, L"DBCOLUMN_TYPE" )) type_ord = colinfo[i].iOrdinal;
        if (!lstrcmpW( name, L"DBCOLUMN_BASETABLENAME" )) t_ord = colinfo[i].iOrdinal;
        if (!lstrcmpW( name, L"DBCOLUMN_BASECOLUMNNAME" )) b_ord = colinfo[i].iOrdinal;
    }
    printf( "columns_info=%d (%Iu cols, first %ls, name %Iu number %Iu type %Iu basecol %Iu basetable %Iu)\n",
            ncols == 13 && colinfo && !lstrcmpW( colinfo[0].pwszName, L"DBCOLUMN_IDNAME" ) &&
            colinfo[0].columnid.eKind == DBKIND_GUID_PROPID && n_ord && num_ord && type_ord && t_ord && b_ord,
            ncols, ncols ? colinfo[0].pwszName : L"-", n_ord, num_ord, type_ord, b_ord, t_ord );
    if (!n_ord || !num_ord || !type_ord || !t_ord || !b_ord) { printf( "DONE\n" ); return 0; }

    IRowset_QueryInterface( cols, &IID_IAccessor, (void **)&acc );
    make_binding( &binds[0], n_ord, DBTYPE_WSTR, offsetof(struct out_row, name), offsetof(struct out_row, name_status),
          offsetof(struct out_row, name_len), sizeof(((struct out_row *)0)->name) );
    make_binding( &binds[1], num_ord, DBTYPE_UI4, offsetof(struct out_row, number), offsetof(struct out_row, number_status), -1, 4 );
    make_binding( &binds[2], type_ord, DBTYPE_UI2, offsetof(struct out_row, type), offsetof(struct out_row, type_status), -1, 2 );
    make_binding( &binds[3], t_ord, DBTYPE_WSTR, offsetof(struct out_row, table), offsetof(struct out_row, table_status),
          offsetof(struct out_row, table_len), sizeof(((struct out_row *)0)->table) );
    make_binding( &binds[4], b_ord, DBTYPE_WSTR, offsetof(struct out_row, column), offsetof(struct out_row, column_status),
          offsetof(struct out_row, column_len), sizeof(((struct out_row *)0)->column) );
    hr = IAccessor_CreateAccessor( acc, DBACCESSOR_ROWDATA, 5, binds, sizeof(struct out_row), &hacc, NULL );
    printf( "accessor=%08lx\n", hr );
    hr = IRowset_GetNextRows( cols, DB_NULL_HCHAPTER, 0, 8, &got, &prow );
    printf( "rows=%d (%08lx %Iu)\n", got == 3, hr, got );
    for (i = 0; i < got; i++)
    {
        static const WCHAR *const names[] = { L"Id", L"Name", L"Age" };
        static const USHORT types[] = { DBTYPE_I4, DBTYPE_WSTR, DBTYPE_I4 };
        struct out_row r;
        memset( &r, 0, sizeof(r) );
        IRowset_GetData( cols, rows[i], hacc, &r );
        printf( "      row %Iu: %ls #%lu type %u table %ls column %ls\n", i, r.name, r.number, r.type, r.table, r.column );
        if (i >= 3 || lstrcmpW( r.name, names[i] ) || r.number != i + 1 || r.type != types[i] ||
            lstrcmpW( r.table, L"People" ) || lstrcmpW( r.column, names[i] ))
            ok_rows = 0;
    }
    printf( "row_values=%d\n", got == 3 && ok_rows );
    if (got) IRowset_ReleaseRows( cols, got, rows, NULL, NULL, NULL );
    hr = IRowset_RestartPosition( cols, DB_NULL_HCHAPTER );
    got = 0;
    IRowset_GetNextRows( cols, DB_NULL_HCHAPTER, 0, 8, &got, &prow );
    printf( "restart=%d (%08lx %Iu)\n", SUCCEEDED(hr) && got == 3, hr, got );
    if (got) IRowset_ReleaseRows( cols, got, rows, NULL, NULL, NULL );
    printf( "DONE\n" );
    return 0;
}
