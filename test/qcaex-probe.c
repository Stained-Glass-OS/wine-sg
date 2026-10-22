/* qcaex-probe: sspicli/secur32 QueryContextAttributesExW on a TLS context (Stained Glass OS wine-sg 6021).
 * A client and a server handshake in memory with Schannel.   qcaex-probe.exe SERVER.pfx */
#define SECURITY_WIN32
#include <windows.h>
#include <wincrypt.h>
#include <sspi.h>
#include <schannel.h>
#include <stdio.h>

typedef SECURITY_STATUS (WINAPI *qcaex_fn)(PCtxtHandle, ULONG, void *, ULONG);

static int failures;

static void check( int ok, const char *what, long got )
{
    if (ok) printf( "ok    %s\n", what );
    else { printf( "FAIL  %s (got %#lx)\n", what, got ); failures++; }
}

int wmain( int argc, WCHAR **argv )
{
    CredHandle client_cred, server_cred;
    CtxtHandle client_ctx, server_ctx;
    SCHANNEL_CRED ccred = { SCHANNEL_CRED_VERSION }, scred = { SCHANNEL_CRED_VERSION };
    SecBuffer cbuf[2], sbuf[2];
    SecBufferDesc cin = { SECBUFFER_VERSION, 2, cbuf }, cout, sin = { SECBUFFER_VERSION, 2, sbuf }, sout;
    SecBuffer cout_buf, sout_buf;
    SecPkgContext_StreamSizes sizes = {0}, sizes_ex = {0};
    SecPkgContext_ConnectionInfo info = {0};
    BYTE pending[32768];
    ULONG pending_len = 0, attrs;
    SECURITY_STATUS cst = SEC_I_CONTINUE_NEEDED, sst = SEC_I_CONTINUE_NEEDED, st;
    BOOL client_started = FALSE, server_started = FALSE;
    CRYPT_DATA_BLOB pfx;
    HCERTSTORE store;
    PCCERT_CONTEXT cert;
    HANDLE file;
    DWORD got;
    qcaex_fn qcaex;
    int round;

    if (argc < 2) return 2;
    qcaex = (qcaex_fn)GetProcAddress( LoadLibraryA( "sspicli.dll" ), "QueryContextAttributesExW" );
    check( qcaex != NULL, "sspicli exports QueryContextAttributesExW", 0 );
    if (!qcaex) { printf( "RESULT: FAIL\n" ); return 1; }

    file = CreateFileW( argv[1], GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL );
    pfx.cbData = GetFileSize( file, NULL );
    pfx.pbData = malloc( pfx.cbData );
    ReadFile( file, pfx.pbData, pfx.cbData, &got, NULL );
    CloseHandle( file );
    store = PFXImportCertStore( &pfx, L"sg", 0 );
    cert = store ? CertEnumCertificatesInStore( store, NULL ) : NULL;
    check( cert != NULL, "the server certificate", GetLastError() );
    if (!cert) { printf( "RESULT: FAIL\n" ); return 1; }

    scred.cCreds = 1;
    scred.paCred = &cert;
    scred.grbitEnabledProtocols = SP_PROT_TLS1_2_SERVER;
    ccred.grbitEnabledProtocols = SP_PROT_TLS1_2_CLIENT;
    ccred.dwFlags = SCH_CRED_MANUAL_CRED_VALIDATION | SCH_CRED_NO_DEFAULT_CREDS;
    st = AcquireCredentialsHandleW( NULL, (WCHAR *)UNISP_NAME_W, SECPKG_CRED_INBOUND, NULL, &scred, NULL, NULL, &server_cred, NULL );
    check( st == SEC_E_OK, "server credentials", st );
    st = AcquireCredentialsHandleW( NULL, (WCHAR *)UNISP_NAME_W, SECPKG_CRED_OUTBOUND, NULL, &ccred, NULL, NULL, &client_cred, NULL );
    check( st == SEC_E_OK, "client credentials", st );

    /* the handshake, the tokens passed in memory */
    for (round = 0; round < 12 && (cst != SEC_E_OK || sst != SEC_E_OK); round++)
    {
        if (cst != SEC_E_OK)
        {
            cout.ulVersion = SECBUFFER_VERSION; cout.cBuffers = 1; cout.pBuffers = &cout_buf;
            cout_buf.BufferType = SECBUFFER_TOKEN; cout_buf.cbBuffer = 0; cout_buf.pvBuffer = NULL;
            cbuf[0].BufferType = SECBUFFER_TOKEN; cbuf[0].cbBuffer = pending_len; cbuf[0].pvBuffer = pending;
            cbuf[1].BufferType = SECBUFFER_EMPTY; cbuf[1].cbBuffer = 0; cbuf[1].pvBuffer = NULL;
            cst = InitializeSecurityContextW( &client_cred, client_started ? &client_ctx : NULL, (WCHAR *)L"localhost",
                    ISC_REQ_ALLOCATE_MEMORY | ISC_REQ_STREAM | ISC_REQ_CONFIDENTIALITY, 0, 0,
                    client_started ? &cin : NULL, 0, &client_ctx, &cout, &attrs, NULL );
            client_started = TRUE;
            pending_len = 0;
            if (cout_buf.cbBuffer && cout_buf.pvBuffer)
            {
                memcpy( pending, cout_buf.pvBuffer, cout_buf.cbBuffer );
                pending_len = cout_buf.cbBuffer;
                FreeContextBuffer( cout_buf.pvBuffer );
            }
            if (FAILED(cst)) break;
        }
        if (sst != SEC_E_OK && pending_len)
        {
            sout.ulVersion = SECBUFFER_VERSION; sout.cBuffers = 1; sout.pBuffers = &sout_buf;
            sout_buf.BufferType = SECBUFFER_TOKEN; sout_buf.cbBuffer = 0; sout_buf.pvBuffer = NULL;
            sbuf[0].BufferType = SECBUFFER_TOKEN; sbuf[0].cbBuffer = pending_len; sbuf[0].pvBuffer = pending;
            sbuf[1].BufferType = SECBUFFER_EMPTY; sbuf[1].cbBuffer = 0; sbuf[1].pvBuffer = NULL;
            sst = AcceptSecurityContext( &server_cred, server_started ? &server_ctx : NULL, &sin,
                    ASC_REQ_ALLOCATE_MEMORY | ASC_REQ_STREAM | ASC_REQ_CONFIDENTIALITY, 0,
                    &server_ctx, &sout, &attrs, NULL );
            server_started = TRUE;
            pending_len = 0;
            if (sout_buf.cbBuffer && sout_buf.pvBuffer)
            {
                memcpy( pending, sout_buf.pvBuffer, sout_buf.cbBuffer );
                pending_len = sout_buf.cbBuffer;
                FreeContextBuffer( sout_buf.pvBuffer );
            }
            if (FAILED(sst)) break;
        }
    }
    check( cst == SEC_E_OK && sst == SEC_E_OK, "the TLS handshake", cst != SEC_E_OK ? cst : sst );
    if (cst != SEC_E_OK) { printf( "RESULT: FAIL\n" ); return 1; }

    st = QueryContextAttributesW( &client_ctx, SECPKG_ATTR_STREAM_SIZES, &sizes );
    check( st == SEC_E_OK && sizes.cbMaximumMessage, "QueryContextAttributesW STREAM_SIZES", st );
    st = qcaex( &client_ctx, SECPKG_ATTR_STREAM_SIZES, &sizes_ex, sizeof(sizes_ex) );
    check( st == SEC_E_OK && !memcmp( &sizes, &sizes_ex, sizeof(sizes) ), "QueryContextAttributesExW: the same", st );
    st = qcaex( &client_ctx, SECPKG_ATTR_STREAM_SIZES, &sizes_ex, 4 );
    check( st == SEC_E_INSUFFICIENT_MEMORY, "a buffer that is too small: SEC_E_INSUFFICIENT_MEMORY", st );
    st = qcaex( &client_ctx, SECPKG_ATTR_STREAM_SIZES, NULL, sizeof(sizes_ex) );
    check( st == SEC_E_INVALID_PARAMETER, "no buffer: SEC_E_INVALID_PARAMETER", st );
    st = qcaex( &client_ctx, SECPKG_ATTR_CONNECTION_INFO, &info, sizeof(info) );
    check( st == SEC_E_OK && info.dwProtocol, "Schannel's CONNECTION_INFO through it", st );

    DeleteSecurityContext( &client_ctx );
    DeleteSecurityContext( &server_ctx );
    FreeCredentialsHandle( &client_cred );
    FreeCredentialsHandle( &server_cred );
    printf( "RESULT: %s\n", failures ? "FAIL" : "PASS" );
    return failures != 0;
}
