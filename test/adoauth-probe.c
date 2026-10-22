/* adoauth-probe: ADO Connection.Open passes its user and password to the data
 * source; msdasql gives them to the ODBC driver; Connection.OpenSchema gives
 * the tables and columns of the driver's catalog (Stained Glass OS wine-sg 6011).
 *   adoauth-probe.exe LOGFILE   (a stand-in driver, adoauth-driver.dll, writes the
 *   connection string it gets to LOGFILE) */
#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <stdio.h>
#include <string.h>

static const CLSID CLSID_Connection_ = {0x00000514,0x0000,0x0010,{0x80,0x00,0x00,0xaa,0x00,0x6d,0x2e,0xa4}};
static int failures;

static void check( int ok, const char *what, long got )
{
    if (ok) printf( "ok    %s\n", what );
    else { printf( "FAIL  %s (got %#lx)\n", what, got ); failures++; }
}

static HRESULT open_connection( IDispatch *conn, const WCHAR *str, const WCHAR *user, const WCHAR *pass )
{
    OLECHAR *name = (OLECHAR *)L"Open";
    DISPID id;
    VARIANT args[4];
    DISPPARAMS params = { args, NULL, 4, 0 };
    HRESULT hr;

    if (FAILED(hr = IDispatch_GetIDsOfNames( conn, &IID_NULL, &name, 1, 0, &id ))) return hr;
    V_VT(&args[3]) = VT_BSTR; V_BSTR(&args[3]) = SysAllocString( str );
    V_VT(&args[2]) = VT_BSTR; V_BSTR(&args[2]) = SysAllocString( user );
    V_VT(&args[1]) = VT_BSTR; V_BSTR(&args[1]) = SysAllocString( pass );
    V_VT(&args[0]) = VT_I4; V_I4(&args[0]) = -1;
    hr = IDispatch_Invoke( conn, id, &IID_NULL, 0, DISPATCH_METHOD, &params, NULL, NULL, NULL );
    VariantClear( &args[3] ); VariantClear( &args[2] ); VariantClear( &args[1] );
    return hr;
}

static HRESULT invoke( IDispatch *disp, const WCHAR *member, WORD flags, VARIANT *args, UINT nargs, VARIANT *result )
{
    OLECHAR *name = (OLECHAR *)member;
    DISPPARAMS params = { args, NULL, nargs, 0 };
    DISPID id;
    HRESULT hr;

    if (result) VariantInit( result );
    if (FAILED(hr = IDispatch_GetIDsOfNames( disp, &IID_NULL, &name, 1, 0, &id ))) return hr;
    return IDispatch_Invoke( disp, id, &IID_NULL, 0, flags, &params, result, NULL, NULL );
}

/* rs.Fields(name).Value as a string (NULL when Null) */
static BOOL field_str( IDispatch *rs, const WCHAR *name, WCHAR *out, int size )
{
    VARIANT fields, field, value, arg;
    BOOL ret = FALSE;

    out[0] = 0;
    if (FAILED(invoke( rs, L"Fields", DISPATCH_PROPERTYGET, NULL, 0, &fields ))) return FALSE;
    V_VT(&arg) = VT_BSTR;
    V_BSTR(&arg) = SysAllocString( name );
    if (SUCCEEDED(invoke( V_DISPATCH(&fields), L"Item", DISPATCH_PROPERTYGET | DISPATCH_METHOD, &arg, 1, &field )))
    {
        if (SUCCEEDED(invoke( V_DISPATCH(&field), L"Value", DISPATCH_PROPERTYGET, NULL, 0, &value )))
        {
            if (V_VT(&value) == VT_NULL) ret = TRUE;
            else if (SUCCEEDED(VariantChangeType( &value, &value, VARIANT_ALPHABOOL, VT_BSTR )))
            {
                lstrcpynW( out, V_BSTR(&value), size );
                ret = TRUE;
            }
            VariantClear( &value );
        }
        VariantClear( &field );
    }
    VariantClear( &arg );
    VariantClear( &fields );
    return ret;
}

static BOOL rs_eof( IDispatch *rs )
{
    VARIANT v;
    if (FAILED(invoke( rs, L"EOF", DISPATCH_PROPERTYGET, NULL, 0, &v ))) return TRUE;
    return V_BOOL(&v) != VARIANT_FALSE;
}

static void read_log( const char *path, char *buf, int size )
{
    FILE *f = fopen( path, "r" );
    buf[0] = 0;
    if (!f) return;
    if (!fgets( buf, size, f )) buf[0] = 0;
    fclose( f );
}

int main( int argc, char **argv )
{
    IDispatch *conn;
    char log[1024];
    HRESULT hr;

    if (argc < 2) return 2;
    CoInitialize( NULL );
    hr = CoCreateInstance( &CLSID_Connection_, NULL, CLSCTX_INPROC_SERVER, &IID_IDispatch, (void **)&conn );
    check( hr == S_OK, "ADODB.Connection", hr );
    if (FAILED(hr)) { printf( "RESULT: FAIL\n" ); return 1; }

    DeleteFileA( argv[1] );
    hr = open_connection( conn, L"Provider=MSDASQL;DSN=sgmock", L"alice", L"s3cret" );
    read_log( argv[1], log, sizeof(log) );
    printf( "      the driver got: %s", log );
    check( log[0] != 0, "the driver was connected to", 0 );
    check( strstr( log, "UID=alice" ) != NULL, "with the user given to Connection.Open (UID=alice)", 0 );
    check( strstr( log, "PWD=s3cret" ) != NULL, "and its password (PWD=...)", 0 );

    /* the catalog through OpenSchema */
    {
        VARIANT args[3], rs, crit;
        WCHAR name[64], type[64], desc[64];
        SAFEARRAY *sa;
        LONG idx;
        int n = 0, found = 0;

        V_VT(&args[2]) = VT_I4; V_I4(&args[2]) = 20; /* adSchemaTables */
        V_VT(&args[1]) = VT_ERROR; V_ERROR(&args[1]) = DISP_E_PARAMNOTFOUND;
        V_VT(&args[0]) = VT_ERROR; V_ERROR(&args[0]) = DISP_E_PARAMNOTFOUND;
        hr = invoke( conn, L"OpenSchema", DISPATCH_METHOD, args, 3, &rs );
        check( hr == S_OK && V_VT(&rs) == VT_DISPATCH, "OpenSchema(adSchemaTables)", hr );
        while (hr == S_OK && !rs_eof( V_DISPATCH(&rs) ) && n < 10)
        {
            field_str( V_DISPATCH(&rs), L"TABLE_NAME", name, 64 );
            field_str( V_DISPATCH(&rs), L"TABLE_TYPE", type, 64 );
            field_str( V_DISPATCH(&rs), L"DESCRIPTION", desc, 64 );
            if (!lstrcmpW( name, L"Customers" ) && !lstrcmpW( type, L"TABLE" ) && !lstrcmpW( desc, L"the customers" )) found++;
            n++;
            invoke( V_DISPATCH(&rs), L"MoveNext", DISPATCH_METHOD, NULL, 0, NULL );
        }
        check( n == 2 && found == 1, "its rows: TABLE_NAME, TABLE_TYPE, DESCRIPTION of the driver's two tables", n );
        if (hr == S_OK) VariantClear( &rs );

        /* restricted to one table name: Array(Empty, Empty, "Orders") */
        sa = SafeArrayCreateVector( VT_VARIANT, 0, 3 );
        V_VT(&crit) = VT_BSTR; V_BSTR(&crit) = SysAllocString( L"Orders" );
        idx = 2; SafeArrayPutElement( sa, &idx, &crit );
        VariantClear( &crit );
        V_VT(&args[1]) = VT_ARRAY | VT_VARIANT; V_ARRAY(&args[1]) = sa;
        hr = invoke( conn, L"OpenSchema", DISPATCH_METHOD, args, 3, &rs );
        n = found = 0;
        while (hr == S_OK && !rs_eof( V_DISPATCH(&rs) ) && n < 10)
        {
            field_str( V_DISPATCH(&rs), L"TABLE_NAME", name, 64 );
            if (!lstrcmpW( name, L"Orders" )) found++;
            n++;
            invoke( V_DISPATCH(&rs), L"MoveNext", DISPATCH_METHOD, NULL, 0, NULL );
        }
        check( hr == S_OK && n == 1 && found == 1, "OpenSchema with a table-name restriction", n );
        if (hr == S_OK) VariantClear( &rs );
        SafeArrayDestroy( sa );

        /* the columns of a table */
        sa = SafeArrayCreateVector( VT_VARIANT, 0, 3 );
        V_VT(&crit) = VT_BSTR; V_BSTR(&crit) = SysAllocString( L"Customers" );
        idx = 2; SafeArrayPutElement( sa, &idx, &crit );
        VariantClear( &crit );
        V_VT(&args[2]) = VT_I4; V_I4(&args[2]) = 4; /* adSchemaColumns */
        V_VT(&args[1]) = VT_ARRAY | VT_VARIANT; V_ARRAY(&args[1]) = sa;
        hr = invoke( conn, L"OpenSchema", DISPATCH_METHOD, args, 3, &rs );
        check( hr == S_OK, "OpenSchema(adSchemaColumns)", hr );
        n = found = 0;
        while (hr == S_OK && !rs_eof( V_DISPATCH(&rs) ) && n < 10)
        {
            field_str( V_DISPATCH(&rs), L"COLUMN_NAME", name, 64 );
            field_str( V_DISPATCH(&rs), L"DATA_TYPE", type, 64 );
            field_str( V_DISPATCH(&rs), L"IS_NULLABLE", desc, 64 );
            /* Name: DBTYPE_WSTR (130), nullable; Id: DBTYPE_I4 (3), not */
            if (!lstrcmpW( name, L"Name" ) && !lstrcmpW( type, L"130" ) && !lstrcmpiW( desc, L"True" )) found++;
            if (!lstrcmpW( name, L"Id" ) && !lstrcmpW( type, L"3" ) && !lstrcmpiW( desc, L"False" )) found++;
            n++;
            invoke( V_DISPATCH(&rs), L"MoveNext", DISPATCH_METHOD, NULL, 0, NULL );
        }
        check( n == 2 && found == 2, "its rows: names, OLE DB data types, nullability", found );
        if (hr == S_OK) VariantClear( &rs );
        SafeArrayDestroy( sa );
    }
    IDispatch_Release( conn );

    CoCreateInstance( &CLSID_Connection_, NULL, CLSCTX_INPROC_SERVER, &IID_IDispatch, (void **)&conn );
    DeleteFileA( argv[1] );
    open_connection( conn, L"Provider=MSDASQL;DSN=sgmock", L"", L"" );
    read_log( argv[1], log, sizeof(log) );
    check( log[0] && !strstr( log, "UID=" ) && !strstr( log, "PWD=" ), "no user given: none is passed", 0 );
    IDispatch_Release( conn );

    printf( "RESULT: %s\n", failures ? "FAIL" : "PASS" );
    return failures != 0;
}
