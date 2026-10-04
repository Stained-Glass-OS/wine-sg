/* tlscipher-gate.sh's probe: an schannel (SSPI) TLS client to 127.0.0.1:PORT,
 * printing the connection's cipher as SECPKG_ATTR_CONNECTION_INFO reports
 * it: CIPHER <aiCipher hex> <dwCipherStrength> PROTO <dwProtocol hex> */
#define SECURITY_WIN32
#include <winsock2.h>
#include <windows.h>
#include <security.h>
#include <schannel.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    WSADATA wsa; SOCKET s; struct sockaddr_in a = { 0 };
    SCHANNEL_CRED sc = { 0 }; CredHandle cred; CtxtHandle ctx; TimeStamp ts;
    SecBuffer ob = { 0, SECBUFFER_TOKEN, NULL }, ib[2];
    SecBufferDesc obd = { SECBUFFER_VERSION, 1, &ob }, ibd = { SECBUFFER_VERSION, 2, ib };
    ULONG attrs; SECURITY_STATUS st; static char buf[65536]; int have = 0, n, first = 1;
    SecPkgContext_ConnectionInfo ci;
    if (argc < 2) return 2;
    WSAStartup(MAKEWORD(2, 2), &wsa);
    s = socket(AF_INET, SOCK_STREAM, 0);
    a.sin_family = AF_INET; a.sin_port = htons((u_short)atoi(argv[1])); a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (connect(s, (struct sockaddr *)&a, sizeof(a))) { printf("NOCONNECT\n"); return 1; }
    sc.dwVersion = SCHANNEL_CRED_VERSION;
    sc.dwFlags = SCH_CRED_MANUAL_CRED_VALIDATION | SCH_CRED_NO_DEFAULT_CREDS;
    if (AcquireCredentialsHandleA(NULL, (char *)UNISP_NAME_A, SECPKG_CRED_OUTBOUND, NULL, &sc, NULL, NULL, &cred, &ts)) { printf("NOCRED\n"); return 1; }
    for (;;) {
        ib[0].BufferType = SECBUFFER_TOKEN; ib[0].pvBuffer = buf; ib[0].cbBuffer = have;
        ib[1].BufferType = SECBUFFER_EMPTY; ib[1].pvBuffer = NULL; ib[1].cbBuffer = 0;
        ob.pvBuffer = NULL; ob.cbBuffer = 0; ob.BufferType = SECBUFFER_TOKEN;
        st = InitializeSecurityContextA(&cred, first ? NULL : &ctx, (char *)"localhost",
                                        ISC_REQ_ALLOCATE_MEMORY | ISC_REQ_STREAM | ISC_REQ_MANUAL_CRED_VALIDATION,
                                        0, 0, first ? NULL : &ibd, 0, &ctx, &obd, &attrs, &ts);
        first = 0;
        if (ob.cbBuffer && ob.pvBuffer) { send(s, ob.pvBuffer, ob.cbBuffer, 0); FreeContextBuffer(ob.pvBuffer); }
        if (st == SEC_E_OK) break;
        if (st == SEC_E_INCOMPLETE_MESSAGE || st == SEC_I_CONTINUE_NEEDED) {
            if (st == SEC_I_CONTINUE_NEEDED) {
                if (ib[1].BufferType == SECBUFFER_EXTRA && ib[1].cbBuffer) { memmove(buf, buf + have - ib[1].cbBuffer, ib[1].cbBuffer); have = ib[1].cbBuffer; }
                else have = 0;
                if (have) continue;
            }
            if ((n = recv(s, buf + have, sizeof(buf) - have, 0)) <= 0) { printf("CLOSED %08lx\n", (unsigned long)st); return 1; }
            have += n;
            continue;
        }
        printf("HANDSHAKE %08lx\n", (unsigned long)st);
        return 1;
    }
    if (QueryContextAttributesA(&ctx, SECPKG_ATTR_CONNECTION_INFO, &ci)) { printf("NOINFO\n"); return 1; }
    printf("CIPHER %04x %lu PROTO %04lx\n", ci.aiCipher, ci.dwCipherStrength, ci.dwProtocol);
    return 0;
}
