/* adoauth-driver: a stand-in ODBC driver for the adoauth gate. It accepts every
 * connection and writes the connection string it was given to the file named
 * by SG_MOCK_ODBC_LOG; its catalog (SQLTables, SQLColumns) is two tables of
 * database "sgdb" (Stained Glass OS wine-sg 6011). */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

typedef short SQLRETURN;
typedef void *SQLHANDLE;
typedef short SQLSMALLINT;
typedef unsigned short SQLUSMALLINT;
typedef int SQLINTEGER;
typedef WCHAR SQLWCHAR;
typedef long long SQLLEN64;
#define SQL_SUCCESS 0
#define SQL_NO_DATA 100
#define SQL_ERROR (-1)
#define SQL_NULL_DATA (-1)
#define SQL_NTS (-3)
#define SQL_HANDLE_STMT 3
#define SQL_C_WCHAR (-8)
#define SQL_C_SLONG (-16)
#define SQL_C_SSHORT (-15)
#define SQL_WVARCHAR (-9)
#define SQL_INTEGER 4

/* a statement: rows of cells, a string or a number or NULL */
struct cellv { const WCHAR *str; int num; int is_num, is_null; };
struct stmt { int magic, ncols, nrows, row; const WCHAR *const *names; struct cellv cells[8][20]; };

static int handles[16], next;
static struct stmt stmts[8];
static int next_stmt;

__declspec(dllexport) SQLRETURN WINAPI SQLAllocHandle( SQLSMALLINT type, SQLHANDLE in, SQLHANDLE *out )
{
    if (type == SQL_HANDLE_STMT)
    {
        struct stmt *st = &stmts[next_stmt++ % 8];
        memset( st, 0, sizeof(*st) );
        st->magic = 0x5354;
        *out = st;
        return SQL_SUCCESS;
    }
    *out = &handles[next++ % 16];
    return SQL_SUCCESS;
}

static BOOL matches( const WCHAR *filter, SQLSMALLINT len, const WCHAR *value )
{
    if (!filter) return TRUE;
    if (len == SQL_NTS) len = lstrlenW( filter );
    return len == lstrlenW( value ) && !memcmp( filter, value, len * sizeof(WCHAR) );
}

static void set_str( struct cellv *c, const WCHAR *s ) { c->str = s; c->is_null = !s; }
static void set_num( struct cellv *c, int n ) { c->num = n; c->is_num = 1; }

static const WCHAR *const table_names[] = { L"TABLE_CAT", L"TABLE_SCHEM", L"TABLE_NAME", L"TABLE_TYPE", L"REMARKS" };

__declspec(dllexport) SQLRETURN WINAPI SQLTablesW( SQLHANDLE h, SQLWCHAR *cat, SQLSMALLINT cl, SQLWCHAR *sch, SQLSMALLINT sl,
                                                   SQLWCHAR *tab, SQLSMALLINT tl, SQLWCHAR *type, SQLSMALLINT ty )
{
    static const WCHAR *const tables[2] = { L"Customers", L"Orders" };
    struct stmt *st = h;
    int i;

    st->ncols = 5;
    st->names = table_names;
    st->nrows = st->row = 0;
    for (i = 0; i < 2; i++)
    {
        struct cellv *r;
        if (!matches( tab, tl, tables[i] )) continue;
        r = st->cells[st->nrows++];
        set_str( &r[0], L"sgdb" ); set_str( &r[1], L"dbo" ); set_str( &r[2], tables[i] );
        set_str( &r[3], L"TABLE" ); set_str( &r[4], i ? NULL : L"the customers" );
    }
    return SQL_SUCCESS;
}

static const WCHAR *const column_names[] = { L"TABLE_CAT", L"TABLE_SCHEM", L"TABLE_NAME", L"COLUMN_NAME", L"DATA_TYPE",
    L"TYPE_NAME", L"COLUMN_SIZE", L"BUFFER_LENGTH", L"DECIMAL_DIGITS", L"NUM_PREC_RADIX", L"NULLABLE", L"REMARKS",
    L"COLUMN_DEF", L"SQL_DATA_TYPE", L"SQL_DATETIME_SUB", L"CHAR_OCTET_LENGTH", L"ORDINAL_POSITION", L"IS_NULLABLE" };

__declspec(dllexport) SQLRETURN WINAPI SQLColumnsW( SQLHANDLE h, SQLWCHAR *cat, SQLSMALLINT cl, SQLWCHAR *sch, SQLSMALLINT sl,
                                                    SQLWCHAR *tab, SQLSMALLINT tl, SQLWCHAR *col, SQLSMALLINT co )
{
    struct stmt *st = h;
    struct cellv *r;
    int i;

    st->ncols = 18;
    st->names = column_names;
    st->nrows = st->row = 0;
    if (!matches( tab, tl, L"Customers" )) return SQL_SUCCESS;
    for (i = 0; i < 2; i++)
    {
        r = st->cells[st->nrows++];
        memset( r, 0, sizeof(st->cells[0]) );
        set_str( &r[0], L"sgdb" ); set_str( &r[1], L"dbo" ); set_str( &r[2], L"Customers" );
        set_str( &r[3], i ? L"Id" : L"Name" ); set_num( &r[4], i ? SQL_INTEGER : SQL_WVARCHAR );
        set_str( &r[5], i ? L"int" : L"nvarchar" ); set_num( &r[6], i ? 10 : 50 ); set_num( &r[7], i ? 4 : 100 );
        r[8].is_null = 1; r[9].is_null = 1; set_num( &r[10], i ? 0 : 1 ); r[11].is_null = 1; r[12].is_null = 1;
        set_num( &r[13], i ? SQL_INTEGER : SQL_WVARCHAR ); r[14].is_null = 1; set_num( &r[15], 100 );
        set_num( &r[16], i + 1 ); set_str( &r[17], i ? L"NO" : L"YES" );
    }
    return SQL_SUCCESS;
}

__declspec(dllexport) SQLRETURN WINAPI SQLNumResultCols( SQLHANDLE h, SQLSMALLINT *n )
{
    *n = ((struct stmt *)h)->ncols;
    return SQL_SUCCESS;
}

__declspec(dllexport) SQLRETURN WINAPI SQLDescribeColW( SQLHANDLE h, SQLUSMALLINT col, SQLWCHAR *name, SQLSMALLINT size,
                                                        SQLSMALLINT *len, SQLSMALLINT *type, unsigned long long *colsize,
                                                        SQLSMALLINT *digits, SQLSMALLINT *nullable )
{
    struct stmt *st = h;
    if (!col || col > st->ncols) return SQL_ERROR;
    if (name) lstrcpynW( name, st->names[col - 1], size );
    if (len) *len = lstrlenW( st->names[col - 1] );
    if (type) *type = SQL_WVARCHAR;
    if (colsize) *colsize = 128;
    if (digits) *digits = 0;
    if (nullable) *nullable = 1;
    return SQL_SUCCESS;
}

__declspec(dllexport) SQLRETURN WINAPI SQLColAttributesW( SQLHANDLE h, SQLUSMALLINT col, SQLUSMALLINT field, void *ptr,
                                                          SQLSMALLINT len, SQLSMALLINT *out_len, SQLLEN64 *num )
{
    if (num) *num = 0;
    return SQL_SUCCESS;
}

__declspec(dllexport) SQLRETURN WINAPI SQLColAttributeW( SQLHANDLE h, SQLUSMALLINT col, SQLUSMALLINT field, void *ptr,
                                                         SQLSMALLINT len, SQLSMALLINT *out_len, SQLLEN64 *num )
{
    if (num) *num = 0;
    return SQL_SUCCESS;
}

__declspec(dllexport) SQLRETURN WINAPI SQLFetch( SQLHANDLE h )
{
    struct stmt *st = h;
    if (st->row >= st->nrows) return SQL_NO_DATA;
    st->row++;
    return SQL_SUCCESS;
}

__declspec(dllexport) SQLRETURN WINAPI SQLGetData( SQLHANDLE h, SQLUSMALLINT col, SQLSMALLINT ctype, void *buf,
                                                   SQLLEN64 size, SQLLEN64 *ind )
{
    struct stmt *st = h;
    struct cellv *c;

    if (!st->row || !col || col > st->ncols) return SQL_ERROR;
    c = &st->cells[st->row - 1][col - 1];
    if (c->is_null || (!c->is_num && !c->str)) { if (ind) *ind = SQL_NULL_DATA; return SQL_SUCCESS; }
    if (ctype == SQL_C_WCHAR)
    {
        WCHAR tmp[32];
        const WCHAR *s = c->str;
        if (c->is_num) { wsprintfW( tmp, L"%d", c->num ); s = tmp; }
        if (ind) *ind = lstrlenW( s ) * sizeof(WCHAR);
        if (size >= (SQLLEN64)sizeof(WCHAR)) lstrcpynW( buf, s, size / sizeof(WCHAR) );
        return SQL_SUCCESS;
    }
    if (ctype == SQL_C_SLONG) *(int *)buf = c->is_num ? c->num : _wtoi( c->str );
    else if (ctype == SQL_C_SSHORT) *(short *)buf = c->is_num ? c->num : _wtoi( c->str );
    else return SQL_ERROR;
    if (ind) *ind = ctype == SQL_C_SLONG ? 4 : 2;
    return SQL_SUCCESS;
}

__declspec(dllexport) SQLRETURN WINAPI SQLRowCount( SQLHANDLE h, SQLLEN64 *n ) { *n = -1; return SQL_SUCCESS; }
__declspec(dllexport) SQLRETURN WINAPI SQLFreeStmt( SQLHANDLE h, SQLUSMALLINT opt ) { return SQL_SUCCESS; }

__declspec(dllexport) SQLRETURN WINAPI SQLFreeHandle( SQLSMALLINT type, SQLHANDLE h ) { return SQL_SUCCESS; }
__declspec(dllexport) SQLRETURN WINAPI SQLSetEnvAttr( SQLHANDLE h, SQLINTEGER a, void *v, SQLINTEGER l ) { return SQL_SUCCESS; }
__declspec(dllexport) SQLRETURN WINAPI SQLSetConnectAttrW( SQLHANDLE h, SQLINTEGER a, void *v, SQLINTEGER l ) { return SQL_SUCCESS; }
__declspec(dllexport) SQLRETURN WINAPI SQLDisconnect( SQLHANDLE h ) { return SQL_SUCCESS; }
__declspec(dllexport) SQLRETURN WINAPI SQLGetDiagRecW( SQLSMALLINT t, SQLHANDLE h, SQLSMALLINT r, SQLWCHAR *s, SQLINTEGER *n,
                                                       SQLWCHAR *m, SQLSMALLINT l, SQLSMALLINT *ll ) { return SQL_NO_DATA; }

static void log_string( const WCHAR *str, int len )
{
    char path[MAX_PATH];
    FILE *f;

    if (!GetEnvironmentVariableA( "SG_MOCK_ODBC_LOG", path, MAX_PATH )) return;
    if (!(f = fopen( path, "w" ))) return;
    if (len < 0) len = lstrlenW( str );
    fprintf( f, "%.*ls\n", len, str );
    fclose( f );
}

__declspec(dllexport) SQLRETURN WINAPI SQLDriverConnectW( SQLHANDLE h, HWND w, SQLWCHAR *in, SQLSMALLINT in_len,
                                                          SQLWCHAR *out, SQLSMALLINT out_len, SQLSMALLINT *len2,
                                                          SQLUSMALLINT completion )
{
    log_string( in, in_len == -3 ? -1 : in_len );
    if (out && out_len > 0) { lstrcpynW( out, in, out_len ); if (len2) *len2 = lstrlenW( out ); }
    return SQL_SUCCESS;
}

BOOL WINAPI DllMain( HINSTANCE h, DWORD reason, void *reserved ) { return TRUE; }
