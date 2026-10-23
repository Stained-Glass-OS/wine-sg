/* adomock-driver: a small ODBC driver for the ADO gates (Stained Glass OS wine-sg 6032-6034).
 *
 * One table, People (Id INTEGER primary key, Name WVARCHAR(50), Age INTEGER), kept in the file the
 * environment variable SG_ADOMOCK_DATA names ("Id\tName\tAge" lines, "\N" for NULL), so that every
 * process sees what another one wrote. It understands just the statements the gates and ADO's
 * updates use:
 *   SELECT * | col[, col...] FROM People [WHERE cond [AND cond...]] [ORDER BY col]
 *   UPDATE People SET col = value[, ...] [WHERE ...]
 *   INSERT INTO People (col, ...) VALUES (value, ...)
 *   DELETE FROM People [WHERE ...]
 * where a value is ?, an integer, 'text' or NULL and a cond is "col = value", "col > value",
 * "col < value", "col IS NULL". Parameters are bound with SQLBindParameter (input and output: an
 * output parameter of "SELECT ... " is not supported; ODBC escape "{? = call AddAge(?)}" sets its
 * output to the number of people and its input's sum with 100; "{call ListPeople}" returns the table). Every statement, with its
 * parameters, is appended to the file SG_ADOMOCK_LOG names. SQLDisconnect ends the connection:
 * statements after it fail with SQLSTATE 08003, as a real driver's do.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <wctype.h>

typedef short SQLRETURN;
typedef void *SQLHANDLE;
typedef short SQLSMALLINT;
typedef unsigned short SQLUSMALLINT;
typedef int SQLINTEGER;
typedef unsigned int SQLUINTEGER;
typedef WCHAR SQLWCHAR;
typedef long long SQLLEN;
typedef unsigned long long SQLULEN;
#define SQL_SUCCESS 0
#define SQL_SUCCESS_WITH_INFO 1
#define SQL_NO_DATA 100
#define SQL_ERROR (-1)
#define SQL_INVALID_HANDLE (-2)
#define SQL_NULL_DATA (-1)
#define SQL_NTS (-3)
#define SQL_HANDLE_ENV 1
#define SQL_HANDLE_DBC 2
#define SQL_HANDLE_STMT 3
#define SQL_C_CHAR 1
#define SQL_C_LONG 4
#define SQL_C_SHORT 5
#define SQL_C_FLOAT 7
#define SQL_C_DOUBLE 8
#define SQL_C_WCHAR (-8)
#define SQL_C_SLONG (-16)
#define SQL_C_SSHORT (-15)
#define SQL_C_ULONG (-18)
#define SQL_C_USHORT (-17)
#define SQL_C_SBIGINT (-25)
#define SQL_C_UBIGINT (-27)
#define SQL_C_TINYINT (-6)
#define SQL_C_STINYINT (-26)
#define SQL_C_UTINYINT (-28)
#define SQL_C_BIT (-7)
#define SQL_C_DEFAULT 99
#define SQL_INTEGER 4
#define SQL_WVARCHAR (-9)
#define SQL_PARAM_INPUT 1
#define SQL_PARAM_INPUT_OUTPUT 2
#define SQL_PARAM_OUTPUT 4

#define MAX_ROWS 64
#define MAX_PARAMS 16
#define NCOLS 3

static const WCHAR *const col_names[NCOLS] = { L"Id", L"Name", L"Age" };

struct value { int is_null, is_num; int num; WCHAR str[64]; };
struct row { struct value v[NCOLS]; };

struct param { SQLSMALLINT io, ctype; void *ptr; SQLLEN buflen; SQLLEN *ind; };

struct result_col { const WCHAR *name; int src; };   /* src: the table's column, or -1 for a number */

struct stmt
{
    int magic;
    struct param params[MAX_PARAMS + 1];
    struct row rows[MAX_ROWS];
    int nrows, row, ncols;
    unsigned int got;           /* the columns of the current row SQLGetData has given, as bits */
    struct result_col cols[NCOLS + 3];
    int catalog;                /* a catalog function's result: every column a string */
    SQLLEN affected;
    WCHAR *prepared;
    int is_stmt;
};

static int connected;
static WCHAR diag[64];
static int env_handle, dbc_handle;

/* ---- the table's file ---------------------------------------------------------------------- */

static BOOL data_path( char *path )
{
    return GetEnvironmentVariableA( "SG_ADOMOCK_DATA", path, MAX_PATH ) > 0;
}

static int load_table( struct row *rows )
{
    char path[MAX_PATH], bytes[512];
    WCHAR line[256], *field, *ctx;
    FILE *f;
    int n = 0, c;

    if (!data_path( path ) || !(f = fopen( path, "rb" ))) return 0;
    while (n < MAX_ROWS && fgets( bytes, sizeof(bytes), f ))
    {
        WCHAR *nl;
        MultiByteToWideChar( CP_UTF8, 0, bytes, -1, line, ARRAYSIZE(line) );
        nl = wcschr( line, '\n' );
        if (nl) *nl = 0;
        if (!line[0]) continue;
        memset( &rows[n], 0, sizeof(rows[n]) );
        field = wcstok_s( line, L"\t", &ctx );
        for (c = 0; c < NCOLS && field; c++, field = wcstok_s( NULL, L"\t", &ctx ))
        {
            struct value *v = &rows[n].v[c];
            if (!wcscmp( field, L"\\N" )) v->is_null = 1;
            else if (c != 1) { v->is_num = 1; v->num = _wtoi( field ); }
            else lstrcpynW( v->str, field, ARRAYSIZE(v->str) );
        }
        n++;
    }
    fclose( f );
    return n;
}

static void save_table( const struct row *rows, int n )
{
    char path[MAX_PATH];
    FILE *f;
    int r, c;

    if (!data_path( path ) || !(f = fopen( path, "wb" ))) return;
    for (r = 0; r < n; r++)
    {
        for (c = 0; c < NCOLS; c++)
        {
            const struct value *v = &rows[r].v[c];
            char text[256];
            if (c) fputc( '\t', f );
            if (v->is_null) fputs( "\\N", f );
            else if (v->is_num) fprintf( f, "%d", v->num );
            else
            {
                WideCharToMultiByte( CP_UTF8, 0, v->str, -1, text, sizeof(text), NULL, NULL );
                fputs( text, f );
            }
        }
        fputc( '\n', f );
    }
    fclose( f );
}

static void log_line( const WCHAR *text )
{
    char path[MAX_PATH], bytes[2048];
    FILE *f;

    if (!GetEnvironmentVariableA( "SG_ADOMOCK_LOG", path, MAX_PATH )) return;
    if (!(f = fopen( path, "ab" ))) return;
    WideCharToMultiByte( CP_UTF8, 0, text, -1, bytes, sizeof(bytes), NULL, NULL );
    fputs( bytes, f );
    fputc( '\n', f );
    fclose( f );
}

/* ---- parameters ------------------------------------------------------------------------------ */

static BOOL param_value( struct stmt *st, int index, struct value *out )
{
    struct param *p;

    memset( out, 0, sizeof(*out) );
    if (index < 1 || index > MAX_PARAMS || !(p = &st->params[index])->ptr) return FALSE;
    if (p->ind && *p->ind == SQL_NULL_DATA) { out->is_null = 1; return TRUE; }
    out->is_num = 1;
    switch (p->ctype)
    {
    case SQL_C_SLONG: case SQL_C_LONG: case SQL_C_ULONG: out->num = *(int *)p->ptr; break;
    case SQL_C_SSHORT: case SQL_C_SHORT: case SQL_C_USHORT: out->num = *(short *)p->ptr; break;
    case SQL_C_SBIGINT: case SQL_C_UBIGINT: out->num = (int)*(long long *)p->ptr; break;
    case SQL_C_TINYINT: case SQL_C_STINYINT: case SQL_C_UTINYINT: case SQL_C_BIT: out->num = *(unsigned char *)p->ptr; break;
    case SQL_C_DOUBLE: out->num = (int)*(double *)p->ptr; break;
    case SQL_C_FLOAT: out->num = (int)*(float *)p->ptr; break;
    case SQL_C_WCHAR:
        out->is_num = 0;
        if (p->ind && *p->ind >= 0) lstrcpynW( out->str, p->ptr, min( ARRAYSIZE(out->str), *p->ind / sizeof(WCHAR) + 1 ) );
        else lstrcpynW( out->str, p->ptr, ARRAYSIZE(out->str) );
        break;
    case SQL_C_CHAR:
        out->is_num = 0;
        MultiByteToWideChar( CP_ACP, 0, p->ptr, p->ind && *p->ind >= 0 ? (int)*p->ind : -1, out->str, ARRAYSIZE(out->str) - 1 );
        break;
    default: return FALSE;
    }
    return TRUE;
}

static void describe_value( const struct value *v, WCHAR *out, size_t size )
{
    if (v->is_null) lstrcpynW( out, L"NULL", size );
    else if (v->is_num) swprintf( out, size, L"%d", v->num );
    else swprintf( out, size, L"'%ls'", v->str );
}

/* ---- a tiny SQL reader ------------------------------------------------------------------------- */

struct parser { const WCHAR *p; struct stmt *st; int next_param; BOOL error; };

static void skip( struct parser *ps ) { while (iswspace( *ps->p )) ps->p++; }

static BOOL keyword( struct parser *ps, const WCHAR *word )
{
    size_t n = wcslen( word );
    skip( ps );
    if (wcsnicmp( ps->p, word, n ) || iswalnum( ps->p[n] )) return FALSE;
    ps->p += n;
    return TRUE;
}

static BOOL punct( struct parser *ps, WCHAR c )
{
    skip( ps );
    if (*ps->p != c) return FALSE;
    ps->p++;
    return TRUE;
}

/* a column name, maybe [bracketed] or "quoted": its index, or -1 */
static int column( struct parser *ps )
{
    WCHAR name[64];
    int n = 0, c;
    WCHAR close = 0;

    skip( ps );
    if (*ps->p == '[') close = ']';
    else if (*ps->p == '"') close = '"';
    if (close) ps->p++;
    while (*ps->p && n < 63 && (close ? *ps->p != close : (iswalnum( *ps->p ) || *ps->p == '_'))) name[n++] = *ps->p++;
    if (close && *ps->p == close) ps->p++;
    name[n] = 0;
    for (c = 0; c < NCOLS; c++) if (!wcsicmp( name, col_names[c] )) return c;
    ps->error = TRUE;
    return -1;
}

static BOOL table( struct parser *ps )
{
    skip( ps );
    if (*ps->p == '[' || *ps->p == '"') ps->p++;
    if (wcsnicmp( ps->p, L"People", 6 )) { ps->error = TRUE; return FALSE; }
    ps->p += 6;
    if (*ps->p == ']' || *ps->p == '"') ps->p++;
    return TRUE;
}

static void value( struct parser *ps, struct value *v )
{
    memset( v, 0, sizeof(*v) );
    skip( ps );
    if (*ps->p == '?')
    {
        ps->p++;
        if (!param_value( ps->st, ++ps->next_param, v )) ps->error = TRUE;
    }
    else if (*ps->p == '\'')
    {
        int n = 0;
        ps->p++;
        while (*ps->p && *ps->p != '\'' && n < 63) v->str[n++] = *ps->p++;
        if (*ps->p == '\'') ps->p++;
    }
    else if (keyword( ps, L"NULL" )) v->is_null = 1;
    else if (*ps->p == '-' || iswdigit( *ps->p ))
    {
        v->is_num = 1;
        v->num = wcstol( ps->p, (WCHAR **)&ps->p, 10 );
    }
    else ps->error = TRUE;
}

struct cond { int col; WCHAR op; struct value v; };

static int conditions( struct parser *ps, struct cond *conds, int max )
{
    int n = 0;

    if (!keyword( ps, L"WHERE" )) return 0;
    do
    {
        struct cond *c = &conds[n];
        if (n >= max) { ps->error = TRUE; break; }
        c->col = column( ps );
        if (keyword( ps, L"IS" ))
        {
            if (!keyword( ps, L"NULL" )) ps->error = TRUE;
            c->op = 'N';
        }
        else
        {
            skip( ps );
            c->op = *ps->p;
            if (c->op != '=' && c->op != '>' && c->op != '<') { ps->error = TRUE; break; }
            ps->p++;
            value( ps, &c->v );
        }
        n++;
    } while (!ps->error && keyword( ps, L"AND" ));
    return n;
}

static int compare( const struct value *a, const struct value *b )
{
    if (a->is_num && b->is_num) return a->num < b->num ? -1 : a->num > b->num;
    if (a->is_num) { WCHAR s[16]; swprintf( s, 16, L"%d", a->num ); return wcscmp( s, b->str ); }
    if (b->is_num) { WCHAR s[16]; swprintf( s, 16, L"%d", b->num ); return wcscmp( a->str, s ); }
    return wcscmp( a->str, b->str );
}

static BOOL matches( const struct row *row, const struct cond *conds, int n )
{
    int i;
    for (i = 0; i < n; i++)
    {
        const struct value *v = &row->v[conds[i].col];
        if (conds[i].op == 'N') { if (!v->is_null) return FALSE; continue; }
        if (v->is_null || conds[i].v.is_null) return FALSE;   /* NULL compares as unknown */
        if (conds[i].op == '=' && compare( v, &conds[i].v )) return FALSE;
        if (conds[i].op == '>' && compare( v, &conds[i].v ) <= 0) return FALSE;
        if (conds[i].op == '<' && compare( v, &conds[i].v ) >= 0) return FALSE;
    }
    return TRUE;
}

static void set_diag( const WCHAR *state ) { lstrcpynW( diag, state, ARRAYSIZE(diag) ); }

static SQLRETURN execute( struct stmt *st, const WCHAR *sql )
{
    struct parser ps = { sql, st };
    struct row table_rows[MAX_ROWS];
    struct cond conds[8];
    int nrows, ncond, r, c, i;
    WCHAR logbuf[1024], vbuf[80];

    st->catalog = 0;

    swprintf( logbuf, ARRAYSIZE(logbuf), L"SQL %ls", sql );
    for (i = 1; i <= MAX_PARAMS && st->params[i].ptr; i++)
    {
        struct value v;
        param_value( st, i, &v );
        describe_value( &v, vbuf, ARRAYSIZE(vbuf) );
        swprintf( logbuf + wcslen( logbuf ), ARRAYSIZE(logbuf) - wcslen( logbuf ), L" | %d:%ls", i, vbuf );
    }
    log_line( logbuf );

    if (!connected) { set_diag( L"08003" ); return SQL_ERROR; }
    st->nrows = st->row = st->ncols = 0;
    st->affected = -1;
    nrows = load_table( table_rows );

    if (keyword( &ps, L"SELECT" ))
    {
        int order = -1;
        if (punct( &ps, '*' ))
        {
            for (c = 0; c < NCOLS; c++) { st->cols[c].name = col_names[c]; st->cols[c].src = c; }
            st->ncols = NCOLS;
        }
        else do
        {
            if (st->ncols >= NCOLS) { ps.error = TRUE; break; }
            c = column( &ps );
            st->cols[st->ncols].name = c >= 0 ? col_names[c] : L"";
            st->cols[st->ncols++].src = c;
        } while (!ps.error && punct( &ps, ',' ));
        if (!keyword( &ps, L"FROM" ) || !table( &ps )) ps.error = TRUE;
        ncond = conditions( &ps, conds, 8 );
        if (keyword( &ps, L"ORDER" ) && keyword( &ps, L"BY" )) order = column( &ps );
        skip( &ps );
        if (ps.error || *ps.p) { set_diag( L"42000" ); return SQL_ERROR; }
        for (r = 0; r < nrows; r++) if (matches( &table_rows[r], conds, ncond )) st->rows[st->nrows++] = table_rows[r];
        if (order >= 0)
            for (r = 1; r < st->nrows; r++)
                for (i = r; i > 0 && compare( &st->rows[i - 1].v[order], &st->rows[i].v[order] ) > 0; i--)
                {
                    struct row t = st->rows[i];
                    st->rows[i] = st->rows[i - 1];
                    st->rows[i - 1] = t;
                }
        return SQL_SUCCESS;
    }
    if (keyword( &ps, L"UPDATE" ))
    {
        int set_cols[NCOLS], nset = 0;
        struct value set_vals[NCOLS];

        if (!table( &ps ) || !keyword( &ps, L"SET" )) ps.error = TRUE;
        while (!ps.error)
        {
            if (nset >= NCOLS) { ps.error = TRUE; break; }
            set_cols[nset] = column( &ps );
            if (!punct( &ps, '=' )) { ps.error = TRUE; break; }
            value( &ps, &set_vals[nset++] );
            if (!punct( &ps, ',' )) break;
        }
        ncond = conditions( &ps, conds, 8 );
        skip( &ps );
        if (ps.error || *ps.p) { set_diag( L"42000" ); return SQL_ERROR; }
        st->affected = 0;
        for (r = 0; r < nrows; r++)
            if (matches( &table_rows[r], conds, ncond ))
            {
                for (i = 0; i < nset; i++) table_rows[r].v[set_cols[i]] = set_vals[i];
                st->affected++;
            }
        save_table( table_rows, nrows );
        return SQL_SUCCESS;
    }
    if (keyword( &ps, L"INSERT" ))
    {
        int ins_cols[NCOLS], nins = 0;
        struct row row;

        memset( &row, 0, sizeof(row) );
        for (c = 0; c < NCOLS; c++) row.v[c].is_null = 1;
        if (!keyword( &ps, L"INTO" ) || !table( &ps ) || !punct( &ps, '(' )) ps.error = TRUE;
        while (!ps.error)
        {
            if (nins >= NCOLS) { ps.error = TRUE; break; }
            ins_cols[nins++] = column( &ps );
            if (!punct( &ps, ',' )) break;
        }
        if (!punct( &ps, ')' ) || !keyword( &ps, L"VALUES" ) || !punct( &ps, '(' )) ps.error = TRUE;
        for (i = 0; i < nins && !ps.error; i++)
        {
            if (i && !punct( &ps, ',' )) ps.error = TRUE;
            value( &ps, &row.v[ins_cols[i]] );
        }
        if (!punct( &ps, ')' )) ps.error = TRUE;
        skip( &ps );
        if (ps.error || *ps.p || nrows >= MAX_ROWS) { set_diag( L"42000" ); return SQL_ERROR; }
        for (r = 0; r < nrows; r++)
            if (!row.v[0].is_null && table_rows[r].v[0].is_num && table_rows[r].v[0].num == row.v[0].num)
            {
                set_diag( L"23000" );   /* the primary key is there already */
                return SQL_ERROR;
            }
        table_rows[nrows++] = row;
        save_table( table_rows, nrows );
        st->affected = 1;
        return SQL_SUCCESS;
    }
    if (keyword( &ps, L"DELETE" ))
    {
        int kept = 0;
        if (!keyword( &ps, L"FROM" ) || !table( &ps )) ps.error = TRUE;
        ncond = conditions( &ps, conds, 8 );
        skip( &ps );
        if (ps.error || *ps.p) { set_diag( L"42000" ); return SQL_ERROR; }
        st->affected = 0;
        for (r = 0; r < nrows; r++)
        {
            if (matches( &table_rows[r], conds, ncond )) st->affected++;
            else table_rows[kept++] = table_rows[r];
        }
        save_table( table_rows, kept );
        return SQL_SUCCESS;
    }
    /* { call ListPeople }: a procedure that returns the people */
    {
        struct parser call = { sql, st };
        if (punct( &call, '{' ) && keyword( &call, L"call" ) && keyword( &call, L"ListPeople" ))
        {
            for (c = 0; c < NCOLS; c++) { st->cols[c].name = col_names[c]; st->cols[c].src = c; }
            st->ncols = NCOLS;
            for (r = 0; r < nrows; r++) st->rows[st->nrows++] = table_rows[r];
            return SQL_SUCCESS;
        }
    }
    /* {? = call AddAge(?)}: the output parameter gets the number of people, the input plus 100 back */
    if (punct( &ps, '{' ) && punct( &ps, '?' ) && punct( &ps, '=' ) && keyword( &ps, L"call" ) && keyword( &ps, L"AddAge" ))
    {
        struct value in;
        struct param *ret = &st->params[1], *io = &st->params[2];

        if (!param_value( st, 2, &in ) || !ret->ptr || ret->ctype != SQL_C_SLONG) { set_diag( L"07002" ); return SQL_ERROR; }
        *(int *)ret->ptr = nrows;
        if (ret->ind) *ret->ind = 4;
        if (io->io != SQL_PARAM_INPUT && io->ctype == SQL_C_SLONG) *(int *)io->ptr = in.num + 100;
        st->affected = -1;
        return SQL_SUCCESS;
    }
    set_diag( L"42000" );
    return SQL_ERROR;
}

/* ---- the ODBC entry points ---------------------------------------------------------------------- */

__declspec(dllexport) SQLRETURN WINAPI SQLAllocHandle( SQLSMALLINT type, SQLHANDLE in, SQLHANDLE *out )
{
    if (type == SQL_HANDLE_STMT)
    {
        struct stmt *st = calloc( 1, sizeof(*st) );
        if (!st) return SQL_ERROR;
        st->is_stmt = 1;
        st->affected = -1;
        *out = st;
        return SQL_SUCCESS;
    }
    *out = type == SQL_HANDLE_ENV ? (void *)&env_handle : (void *)&dbc_handle;
    return SQL_SUCCESS;
}

__declspec(dllexport) SQLRETURN WINAPI SQLFreeHandle( SQLSMALLINT type, SQLHANDLE h )
{
    if (type == SQL_HANDLE_STMT && h) { free( ((struct stmt *)h)->prepared ); free( h ); }
    return SQL_SUCCESS;
}

__declspec(dllexport) SQLRETURN WINAPI SQLFreeStmt( SQLHANDLE h, SQLUSMALLINT opt )
{
    struct stmt *st = h;
    if (opt == 3 /* SQL_RESET_PARAMS */) memset( st->params, 0, sizeof(st->params) );
    if (opt == 0 /* SQL_CLOSE */) st->nrows = st->row = 0;
    return SQL_SUCCESS;
}

__declspec(dllexport) SQLRETURN WINAPI SQLSetEnvAttr( SQLHANDLE h, SQLINTEGER a, void *v, SQLINTEGER l ) { return SQL_SUCCESS; }
__declspec(dllexport) SQLRETURN WINAPI SQLSetConnectAttrW( SQLHANDLE h, SQLINTEGER a, void *v, SQLINTEGER l ) { return SQL_SUCCESS; }
__declspec(dllexport) SQLRETURN WINAPI SQLSetStmtAttrW( SQLHANDLE h, SQLINTEGER a, void *v, SQLINTEGER l ) { return SQL_SUCCESS; }
__declspec(dllexport) SQLRETURN WINAPI SQLGetConnectAttrW( SQLHANDLE h, SQLINTEGER a, void *v, SQLINTEGER l, SQLINTEGER *ol )
{
    if (v && l >= 4) *(int *)v = 0;
    return SQL_SUCCESS;
}

__declspec(dllexport) SQLRETURN WINAPI SQLDriverConnectW( SQLHANDLE h, HWND w, SQLWCHAR *in, SQLSMALLINT in_len,
                                                          SQLWCHAR *out, SQLSMALLINT out_len, SQLSMALLINT *len2,
                                                          SQLUSMALLINT completion )
{
    connected = 1;
    log_line( L"CONNECT" );
    if (out && out_len > 0) { lstrcpynW( out, in, out_len ); if (len2) *len2 = lstrlenW( out ); }
    return SQL_SUCCESS;
}

__declspec(dllexport) SQLRETURN WINAPI SQLDisconnect( SQLHANDLE h )
{
    connected = 0;
    log_line( L"DISCONNECT" );
    return SQL_SUCCESS;
}

__declspec(dllexport) SQLRETURN WINAPI SQLGetDiagRecW( SQLSMALLINT t, SQLHANDLE h, SQLSMALLINT r, SQLWCHAR *state, SQLINTEGER *native,
                                                       SQLWCHAR *msg, SQLSMALLINT len, SQLSMALLINT *outlen )
{
    if (r != 1 || !diag[0]) return SQL_NO_DATA;
    if (state) lstrcpynW( state, diag, 6 );
    if (native) *native = 0;
    if (msg && len > 0) lstrcpynW( msg, !wcscmp( diag, L"08003" ) ? L"not connected" : L"mock driver error", len );
    if (outlen) *outlen = msg ? lstrlenW( msg ) : 0;
    diag[0] = 0;
    return SQL_SUCCESS;
}

__declspec(dllexport) SQLRETURN WINAPI SQLExecDirectW( SQLHANDLE h, SQLWCHAR *sql, SQLINTEGER len )
{
    WCHAR buf[1024];
    lstrcpynW( buf, sql, len == SQL_NTS || len < 0 ? ARRAYSIZE(buf) : min( ARRAYSIZE(buf), len + 1 ) );
    return execute( h, buf );
}

__declspec(dllexport) SQLRETURN WINAPI SQLPrepareW( SQLHANDLE h, SQLWCHAR *sql, SQLINTEGER len )
{
    struct stmt *st = h;
    WCHAR buf[1024];
    lstrcpynW( buf, sql, len == SQL_NTS || len < 0 ? ARRAYSIZE(buf) : min( ARRAYSIZE(buf), len + 1 ) );
    free( st->prepared );
    st->prepared = _wcsdup( buf );
    return SQL_SUCCESS;
}

__declspec(dllexport) SQLRETURN WINAPI SQLExecute( SQLHANDLE h )
{
    struct stmt *st = h;
    if (!st->prepared) return SQL_ERROR;
    return execute( st, st->prepared );
}

__declspec(dllexport) SQLRETURN WINAPI SQLBindParameter( SQLHANDLE h, SQLUSMALLINT index, SQLSMALLINT io, SQLSMALLINT ctype,
                                                         SQLSMALLINT sqltype, SQLULEN size, SQLSMALLINT digits, void *ptr,
                                                         SQLLEN buflen, SQLLEN *ind )
{
    struct stmt *st = h;
    if (!index || index > MAX_PARAMS) return SQL_ERROR;
    st->params[index].io = io;
    st->params[index].ctype = ctype;
    st->params[index].ptr = ptr;
    st->params[index].buflen = buflen;
    st->params[index].ind = ind;
    return SQL_SUCCESS;
}

__declspec(dllexport) SQLRETURN WINAPI SQLNumParams( SQLHANDLE h, SQLSMALLINT *n )
{
    struct stmt *st = h;
    const WCHAR *p;
    *n = 0;
    for (p = st->prepared; p && *p; p++) if (*p == '?') (*n)++;
    return SQL_SUCCESS;
}

__declspec(dllexport) SQLRETURN WINAPI SQLNumResultCols( SQLHANDLE h, SQLSMALLINT *n )
{
    *n = ((struct stmt *)h)->ncols;
    return SQL_SUCCESS;
}

__declspec(dllexport) SQLRETURN WINAPI SQLRowCount( SQLHANDLE h, SQLLEN *n )
{
    *n = ((struct stmt *)h)->affected;
    return SQL_SUCCESS;
}

__declspec(dllexport) SQLRETURN WINAPI SQLMoreResults( SQLHANDLE h ) { return SQL_NO_DATA; }

__declspec(dllexport) SQLRETURN WINAPI SQLDescribeColW( SQLHANDLE h, SQLUSMALLINT col, SQLWCHAR *name, SQLSMALLINT size,
                                                        SQLSMALLINT *len, SQLSMALLINT *type, SQLULEN *colsize,
                                                        SQLSMALLINT *digits, SQLSMALLINT *nullable )
{
    struct stmt *st = h;
    int src;

    if (!col || col > st->ncols) return SQL_ERROR;
    src = st->cols[col - 1].src;
    if (name) lstrcpynW( name, st->cols[col - 1].name, size );
    if (len) *len = lstrlenW( st->cols[col - 1].name );
    if (st->catalog) src = 1;
    if (type) *type = src == 1 ? SQL_WVARCHAR : SQL_INTEGER;
    if (colsize) *colsize = src == 1 ? 50 : 10;
    if (digits) *digits = 0;
    if (nullable) *nullable = src == 0 ? 0 : 1;
    return SQL_SUCCESS;
}

__declspec(dllexport) SQLRETURN WINAPI SQLColAttributeW( SQLHANDLE h, SQLUSMALLINT col, SQLUSMALLINT field, void *ptr,
                                                         SQLSMALLINT len, SQLSMALLINT *out_len, SQLLEN *num )
{
    struct stmt *st = h;
    const WCHAR *text = NULL;

    if (num) *num = 0;
    if (!col || col > st->ncols) return SQL_ERROR;
    switch (field)
    {
    case 23: /* SQL_DESC_BASE_TABLE_NAME */
    case 15: /* SQL_COLUMN_TABLE_NAME */ text = L"People"; break;
    case 22: /* SQL_DESC_BASE_COLUMN_NAME */
    case 1011: /* SQL_DESC_NAME */
    case 1: /* SQL_COLUMN_NAME */ text = st->cols[col - 1].name; break;
    case 1001: /* SQL_DESC_COUNT */ if (num) *num = st->ncols; break;
    case 3: /* SQL_COLUMN_LENGTH */
    case 4: /* SQL_COLUMN_PRECISION */
    case 1003: /* SQL_DESC_LENGTH */
    case 1005: /* SQL_DESC_PRECISION */ if (num) *num = (st->catalog || st->cols[col - 1].src == 1) ? 50 : 10; break;
    case 1013: /* SQL_DESC_OCTET_LENGTH */ if (num) *num = (st->catalog || st->cols[col - 1].src == 1) ? 100 : 4; break;
    case 6: /* SQL_COLUMN_DISPLAY_SIZE */ if (num) *num = (st->catalog || st->cols[col - 1].src == 1) ? 50 : 11; break;
    case 8: /* SQL_COLUMN_UNSIGNED */
    case 5: /* SQL_COLUMN_SCALE */
    case 1006: /* SQL_DESC_SCALE */ if (num) *num = 0; break;
    case 10: /* SQL_COLUMN_UPDATABLE: SQL_ATTR_WRITE */ if (num) *num = 1; break;
    case 7: case 1008: /* nullable */ if (num) *num = st->cols[col - 1].src == 0 ? 0 : 1; break;
    }
    if (text && ptr && len > 0) lstrcpynW( ptr, text, len / sizeof(WCHAR) );
    if (out_len) *out_len = text ? lstrlenW( text ) * sizeof(WCHAR) : 0;
    return SQL_SUCCESS;
}

__declspec(dllexport) SQLRETURN WINAPI SQLColAttributesW( SQLHANDLE h, SQLUSMALLINT col, SQLUSMALLINT field, void *ptr,
                                                          SQLSMALLINT len, SQLSMALLINT *out_len, SQLLEN *num )
{
    return SQLColAttributeW( h, col, field, ptr, len, out_len, num );
}

__declspec(dllexport) SQLRETURN WINAPI SQLFetch( SQLHANDLE h )
{
    struct stmt *st = h;
    if (st->row >= st->nrows) return SQL_NO_DATA;
    st->row++;
    st->got = 0;
    return SQL_SUCCESS;
}

__declspec(dllexport) SQLRETURN WINAPI SQLGetData( SQLHANDLE h, SQLUSMALLINT col, SQLSMALLINT ctype, void *buf,
                                                   SQLLEN size, SQLLEN *ind )
{
    struct stmt *st = h;
    const struct value *v;
    int src;

    if (!st->row || !col || col > st->ncols) return SQL_ERROR;
    /* a column's data is given once: a second call for it is the end of its data */
    if (st->got & (1u << col)) return SQL_NO_DATA;
    st->got |= 1u << col;
    src = st->cols[col - 1].src;
    v = &st->rows[st->row - 1].v[src];
    if (v->is_null) { if (ind) *ind = SQL_NULL_DATA; return SQL_SUCCESS; }
    if (ctype == SQL_C_WCHAR || (ctype == SQL_C_DEFAULT && !v->is_num))
    {
        WCHAR tmp[32];
        const WCHAR *s = v->str;
        if (v->is_num) { swprintf( tmp, 32, L"%d", v->num ); s = tmp; }
        if (ind) *ind = lstrlenW( s ) * sizeof(WCHAR);
        if (size >= (SQLLEN)sizeof(WCHAR)) lstrcpynW( buf, s, size / sizeof(WCHAR) );
        return SQL_SUCCESS;
    }
    if (ctype == SQL_C_CHAR)
    {
        char tmp[64];
        if (v->is_num) sprintf( tmp, "%d", v->num ); else WideCharToMultiByte( CP_ACP, 0, v->str, -1, tmp, 64, NULL, NULL );
        if (ind) *ind = strlen( tmp );
        if (size > 0) lstrcpynA( buf, tmp, size );
        return SQL_SUCCESS;
    }
    switch (ctype)
    {
    case SQL_C_SLONG: case SQL_C_LONG: case SQL_C_ULONG: case SQL_C_DEFAULT:
        *(int *)buf = v->is_num ? v->num : _wtoi( v->str ); if (ind) *ind = 4; break;
    case SQL_C_SSHORT: case SQL_C_SHORT: case SQL_C_USHORT:
        *(short *)buf = v->is_num ? v->num : _wtoi( v->str ); if (ind) *ind = 2; break;
    case SQL_C_SBIGINT: case SQL_C_UBIGINT:
        *(long long *)buf = v->is_num ? v->num : _wtoi( v->str ); if (ind) *ind = 8; break;
    case SQL_C_DOUBLE:
        *(double *)buf = v->is_num ? v->num : _wtoi( v->str ); if (ind) *ind = 8; break;
    default: return SQL_ERROR;
    }
    return SQL_SUCCESS;
}

/* the catalog: People and its primary key */
__declspec(dllexport) SQLRETURN WINAPI SQLPrimaryKeysW( SQLHANDLE h, SQLWCHAR *cat, SQLSMALLINT cl, SQLWCHAR *sch,
                                                        SQLSMALLINT sl, SQLWCHAR *tab, SQLSMALLINT tl )
{
    /* ODBC's result: TABLE_CAT, TABLE_SCHEM, TABLE_NAME, COLUMN_NAME, KEY_SEQ, PK_NAME; the row's
     * values: 0 null, 1 the table, 2 the column */
    static const WCHAR *const names[] = { L"TABLE_CAT", L"TABLE_SCHEM", L"TABLE_NAME", L"COLUMN_NAME", L"KEY_SEQ", L"PK_NAME" };
    static const int src[] = { 0, 0, 1, 2, 0, 0 };
    struct stmt *st = h;
    int c;

    if (!connected) { set_diag( L"08003" ); return SQL_ERROR; }
    log_line( L"PRIMARYKEYS" );
    st->nrows = st->row = 0;
    st->ncols = 6;
    st->catalog = 1;
    for (c = 0; c < 6; c++) { st->cols[c].name = names[c]; st->cols[c].src = src[c]; }
    if (!tab || wcsnicmp( tab, L"People", 6 )) return SQL_SUCCESS;
    memset( &st->rows[0], 0, sizeof(st->rows[0]) );
    st->rows[0].v[0].is_null = 1;
    lstrcpyW( st->rows[0].v[1].str, L"People" );
    lstrcpyW( st->rows[0].v[2].str, L"Id" );
    st->nrows = 1;
    return SQL_SUCCESS;
}

__declspec(dllexport) SQLRETURN WINAPI SQLGetInfoW( SQLHANDLE h, SQLUSMALLINT type, void *value, SQLSMALLINT len, SQLSMALLINT *out )
{
    if (value && len >= 4) *(int *)value = 0;
    if (out) *out = 4;
    return SQL_SUCCESS;
}

BOOL WINAPI DllMain( HINSTANCE h, DWORD reason, void *reserved ) { return TRUE; }
