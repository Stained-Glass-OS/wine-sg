/* adoschema-probe: MSDASQL's IDBSchemaRowset gives the foreign keys, indexes and provider types of an ODBC
 * data source (wine-sg 8224; test/adoschema-gate.sh) through SQLForeignKeys, SQLStatistics and SQLGetTypeInfo
 * of the gate's mock driver, and (8230) its views and procedure parameters through SQLTables and
 * SQLProcedureColumns, and (8232) statistics and table and column privileges through SQLStatistics,
 * SQLTablePrivileges and SQLColumnPrivileges. They were E_INVALIDARG (unknown schema). Prints the rows as
 * name=value lines.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#define COBJMACROS
#define DBINITCONSTANTS
#include <windows.h>
#include <oledb.h>
#include <oledberr.h>
#include <stdio.h>
#include <stddef.h>

static const GUID DBSCHEMA_PROCEDURE_PARAMETERS_ = {0xc8b522b8,0x5cf3,0x11ce,{0xad,0xe5,0x00,0xaa,0x00,0x44,0x77,0x3d}};
static const GUID DBSCHEMA_STATISTICS_ = {0xc8b52227,0x5cf3,0x11ce,{0xad,0xe5,0x00,0xaa,0x00,0x44,0x77,0x3d}};
static const GUID DBSCHEMA_TABLE_PRIVILEGES_ = {0xc8b52222,0x5cf3,0x11ce,{0xad,0xe5,0x00,0xaa,0x00,0x44,0x77,0x3d}};
static const GUID DBSCHEMA_COLUMN_PRIVILEGES_ = {0xc8b52221,0x5cf3,0x11ce,{0xad,0xe5,0x00,0xaa,0x00,0x44,0x77,0x3d}};
static const CLSID CLSID_MSDASQL_ = {0xc8b522cb,0x5cf3,0x11ce,{0xad,0xe5,0x00,0xaa,0x00,0x44,0x77,0x3d}};

struct cellbuf { DBSTATUS status; DBLENGTH len; union { WCHAR text[128]; LONG num; } u; };

/* print the named columns of every row: text as it is, numbers as numbers */
static int dump( IRowset *rowset, const char *tag, const WCHAR *const *names, int count )
{
    IColumnsInfo *info;
    IAccessor *acc;
    DBCOLUMNINFO *colinfo = NULL;
    OLECHAR *strings = NULL;
    DBORDINAL ncols = 0, c;
    DBBINDING binds[16];
    HACCESSOR hacc;
    HROW rows[16], *prow = rows;
    DBCOUNTITEM got = 0, r;
    struct cellbuf *buf;
    int i, nrows = 0;
    HRESULT hr;

    IRowset_QueryInterface( rowset, &IID_IColumnsInfo, (void **)&info );
    IColumnsInfo_GetColumnInfo( info, &ncols, &colinfo, &strings );
    memset( binds, 0, sizeof(binds) );
    for (i = 0; i < count; i++)
    {
        for (c = 0; c < ncols; c++) if (colinfo[c].pwszName && !lstrcmpW( colinfo[c].pwszName, names[i] )) break;
        if (c == ncols) { printf( "%s_nocolumn=%ls\n", tag, names[i] ); return -1; }
        binds[i].iOrdinal = colinfo[c].iOrdinal;
        binds[i].obStatus = i * sizeof(struct cellbuf) + offsetof(struct cellbuf, status);
        binds[i].obLength = i * sizeof(struct cellbuf) + offsetof(struct cellbuf, len);
        binds[i].obValue = i * sizeof(struct cellbuf) + offsetof(struct cellbuf, u);
        binds[i].dwPart = DBPART_VALUE | DBPART_STATUS | DBPART_LENGTH;
        binds[i].dwMemOwner = DBMEMOWNER_CLIENTOWNED;
        binds[i].eParamIO = DBPARAMIO_NOTPARAM;
        binds[i].wType = colinfo[c].wType == DBTYPE_WSTR ? DBTYPE_WSTR : DBTYPE_I4;
        binds[i].cbMaxLen = binds[i].wType == DBTYPE_WSTR ? sizeof(((struct cellbuf *)0)->u.text) : 4;
    }
    IRowset_QueryInterface( rowset, &IID_IAccessor, (void **)&acc );
    hr = IAccessor_CreateAccessor( acc, DBACCESSOR_ROWDATA, count, binds, count * sizeof(struct cellbuf), &hacc, NULL );
    if (FAILED(hr)) { printf( "%s_accessor=%08lx\n", tag, hr ); return -1; }
    buf = calloc( count, sizeof(struct cellbuf) );
    hr = IRowset_GetNextRows( rowset, DB_NULL_HCHAPTER, 0, 16, &got, &prow );
    for (r = 0; r < got; r++)
    {
        memset( buf, 0, count * sizeof(struct cellbuf) );
        IRowset_GetData( rowset, rows[r], hacc, buf );
        for (i = 0; i < count; i++)
        {
            if (buf[i].status == DBSTATUS_S_ISNULL) printf( "%s%Iu.%ls=NULL\n", tag, r, names[i] );
            else if (binds[i].wType == DBTYPE_WSTR) printf( "%s%Iu.%ls=%ls\n", tag, r, names[i], buf[i].u.text );
            else printf( "%s%Iu.%ls=%ld\n", tag, r, names[i], buf[i].u.num );
        }
        nrows++;
    }
    printf( "%s_rows=%d\n", tag, nrows );
    if (got) IRowset_ReleaseRows( rowset, got, rows, NULL, NULL, NULL );
    free( buf );
    return nrows;
}

int main( void )
{
    static const WCHAR *const fk_cols[] = { L"PK_TABLE_NAME", L"PK_COLUMN_NAME", L"FK_TABLE_NAME", L"FK_COLUMN_NAME",
        L"ORDINAL", L"UPDATE_RULE", L"DELETE_RULE", L"PK_NAME", L"FK_NAME", L"DEFERRABILITY" };
    static const WCHAR *const index_cols[] = { L"TABLE_NAME", L"INDEX_NAME", L"UNIQUE", L"CLUSTERED", L"TYPE",
        L"ORDINAL_POSITION", L"COLUMN_NAME", L"COLLATION", L"CARDINALITY" };
    static const WCHAR *const type_cols[] = { L"TYPE_NAME", L"DATA_TYPE", L"COLUMN_SIZE", L"LITERAL_PREFIX",
        L"IS_NULLABLE", L"CASE_SENSITIVE", L"SEARCHABLE", L"IS_LONG" };
    IDBInitialize *init = NULL;
    IDBProperties *props;
    IDBCreateSession *cs;
    IDBSchemaRowset *schema = NULL;
    IRowset *rowset;
    DBPROP prop;
    DBPROPSET set;
    VARIANT restr[5];
    GUID *guids = NULL;
    ULONG *support = NULL, n = 0, i;
    static const WCHAR *const view_cols[] = { L"TABLE_NAME", L"DESCRIPTION", L"VIEW_DEFINITION" };
    static const WCHAR *const param_cols[] = { L"PROCEDURE_NAME", L"PARAMETER_NAME", L"ORDINAL_POSITION",
        L"PARAMETER_TYPE", L"PARAMETER_HASDEFAULT", L"PARAMETER_DEFAULT", L"IS_NULLABLE", L"DATA_TYPE",
        L"CHARACTER_MAXIMUM_LENGTH", L"NUMERIC_PRECISION", L"TYPE_NAME" };
    static const WCHAR *const stat_cols[] = { L"TABLE_NAME", L"CARDINALITY" };
    static const WCHAR *const tpriv_cols[] = { L"GRANTOR", L"GRANTEE", L"TABLE_NAME", L"PRIVILEGE_TYPE", L"IS_GRANTABLE" };
    static const WCHAR *const cpriv_cols[] = { L"GRANTOR", L"GRANTEE", L"TABLE_NAME", L"COLUMN_NAME", L"PRIVILEGE_TYPE",
        L"IS_GRANTABLE" };
    int fk = 0, idx = 0, types = 0, views = 0, params = 0, stats = 0, privs = 0;
    HRESULT hr;

    CoInitialize( NULL );
    setvbuf( stdout, NULL, _IONBF, 0 );
    if (FAILED(CoCreateInstance( &CLSID_MSDASQL_, NULL, CLSCTX_INPROC_SERVER, &IID_IDBInitialize, (void **)&init )))
        return 1;
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
    IDBCreateSession_CreateSession( cs, NULL, &IID_IDBSchemaRowset, (IUnknown **)&schema );
    if (!schema) { printf( "no_schema_rowset\n" ); return 1; }

    IDBSchemaRowset_GetSchemas( schema, &n, &guids, &support );
    for (i = 0; i < n; i++)
    {
        if (IsEqualGUID( &guids[i], &DBSCHEMA_FOREIGN_KEYS )) fk = 1;
        if (IsEqualGUID( &guids[i], &DBSCHEMA_INDEXES )) idx = 1;
        if (IsEqualGUID( &guids[i], &DBSCHEMA_PROVIDER_TYPES )) types = 1;
        if (IsEqualGUID( &guids[i], &DBSCHEMA_VIEWS )) views = 1;
        if (IsEqualGUID( &guids[i], &DBSCHEMA_PROCEDURE_PARAMETERS_ )) params = 1;
        if (IsEqualGUID( &guids[i], &DBSCHEMA_STATISTICS_ )) stats = 1;
        if (IsEqualGUID( &guids[i], &DBSCHEMA_TABLE_PRIVILEGES_ )) privs++;
        if (IsEqualGUID( &guids[i], &DBSCHEMA_COLUMN_PRIVILEGES_ )) privs++;
    }
    printf( "schemas_listed=%d%d%d\n", fk, idx, types );
    printf( "schemas_listed2=%d%d\n", views, params );
    printf( "schemas_listed3=%d%d\n", stats, privs );

    for (i = 0; i < 5; i++) VariantInit( &restr[i] );
    V_VT(&restr[2]) = VT_BSTR; V_BSTR(&restr[2]) = SysAllocString( L"People" );   /* PK_TABLE_NAME */
    rowset = NULL;
    hr = IDBSchemaRowset_GetRowset( schema, NULL, &DBSCHEMA_FOREIGN_KEYS, 3, restr, &IID_IRowset, 0, NULL, (IUnknown **)&rowset );
    printf( "foreign_keys=%08lx\n", hr );
    if (rowset) dump( rowset, "fk", fk_cols, ARRAYSIZE(fk_cols) );

    VariantClear( &restr[2] );
    V_VT(&restr[4]) = VT_BSTR; V_BSTR(&restr[4]) = SysAllocString( L"People" );   /* TABLE_NAME */
    rowset = NULL;
    hr = IDBSchemaRowset_GetRowset( schema, NULL, &DBSCHEMA_INDEXES, 5, restr, &IID_IRowset, 0, NULL, (IUnknown **)&rowset );
    printf( "indexes=%08lx\n", hr );
    if (rowset) dump( rowset, "ix", index_cols, ARRAYSIZE(index_cols) );

    /* an index-name restriction is not supported */
    V_VT(&restr[2]) = VT_BSTR; V_BSTR(&restr[2]) = SysAllocString( L"PK_People" );
    rowset = NULL;
    hr = IDBSchemaRowset_GetRowset( schema, NULL, &DBSCHEMA_INDEXES, 5, restr, &IID_IRowset, 0, NULL, (IUnknown **)&rowset );
    printf( "index_name_restriction=%08lx\n", hr );

    rowset = NULL;
    hr = IDBSchemaRowset_GetRowset( schema, NULL, &DBSCHEMA_PROVIDER_TYPES, 0, NULL, &IID_IRowset, 0, NULL, (IUnknown **)&rowset );
    printf( "provider_types=%08lx\n", hr );
    if (rowset) dump( rowset, "pt", type_cols, ARRAYSIZE(type_cols) );

    rowset = NULL;
    hr = IDBSchemaRowset_GetRowset( schema, NULL, &DBSCHEMA_VIEWS, 0, NULL, &IID_IRowset, 0, NULL, (IUnknown **)&rowset );
    printf( "views=%08lx\n", hr );
    if (rowset) dump( rowset, "vw", view_cols, ARRAYSIZE(view_cols) );

    for (i = 0; i < 5; i++) VariantClear( &restr[i] );
    V_VT(&restr[2]) = VT_BSTR; V_BSTR(&restr[2]) = SysAllocString( L"GetPets" );   /* PROCEDURE_NAME */
    rowset = NULL;
    hr = IDBSchemaRowset_GetRowset( schema, NULL, &DBSCHEMA_PROCEDURE_PARAMETERS_, 3, restr, &IID_IRowset, 0, NULL, (IUnknown **)&rowset );
    printf( "procedure_parameters=%08lx\n", hr );
    if (rowset) dump( rowset, "pp", param_cols, ARRAYSIZE(param_cols) );

    for (i = 0; i < 5; i++) VariantClear( &restr[i] );
    V_VT(&restr[2]) = VT_BSTR; V_BSTR(&restr[2]) = SysAllocString( L"People" );   /* TABLE_NAME */
    rowset = NULL;
    hr = IDBSchemaRowset_GetRowset( schema, NULL, &DBSCHEMA_STATISTICS_, 3, restr, &IID_IRowset, 0, NULL, (IUnknown **)&rowset );
    printf( "statistics=%08lx\n", hr );
    if (rowset) dump( rowset, "st", stat_cols, ARRAYSIZE(stat_cols) );
    rowset = NULL;
    hr = IDBSchemaRowset_GetRowset( schema, NULL, &DBSCHEMA_TABLE_PRIVILEGES_, 3, restr, &IID_IRowset, 0, NULL, (IUnknown **)&rowset );
    printf( "table_privileges=%08lx\n", hr );
    if (rowset) dump( rowset, "tp", tpriv_cols, ARRAYSIZE(tpriv_cols) );
    rowset = NULL;
    hr = IDBSchemaRowset_GetRowset( schema, NULL, &DBSCHEMA_COLUMN_PRIVILEGES_, 3, restr, &IID_IRowset, 0, NULL, (IUnknown **)&rowset );
    printf( "column_privileges=%08lx\n", hr );
    if (rowset) dump( rowset, "cp", cpriv_cols, ARRAYSIZE(cpriv_cols) );
    /* a grantor restriction is not supported (ODBC has none) */
    V_VT(&restr[3]) = VT_BSTR; V_BSTR(&restr[3]) = SysAllocString( L"dbo" );
    rowset = NULL;
    hr = IDBSchemaRowset_GetRowset( schema, NULL, &DBSCHEMA_TABLE_PRIVILEGES_, 4, restr, &IID_IRowset, 0, NULL, (IUnknown **)&rowset );
    printf( "grantor_restriction=%08lx\n", hr );
    printf( "DONE\n" );
    return 0;
}
