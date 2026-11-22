/* wine-sg 8416 gate: a TLS 1.3 server that sends before the client's Finished. GnuTLS's
 * GNUTLS_ENABLE_EARLY_START returns from the handshake once the server's Finished is out; the
 * HTTP reply then follows at once, so it arrives with the server's last handshake flight -- as
 * session tickets do from servers like GitHub's. Host program: tls13-earlyserver PORT CERT KEY. */
#include <gnutls/gnutls.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    static const char reply[] = "HTTP/1.1 200 OK\r\nContent-Length: 5\r\nConnection: close\r\n\r\nearly";
    gnutls_certificate_credentials_t cred;
    struct sockaddr_in sa = { .sin_family = AF_INET };
    int ls, one = 1;

    if (argc < 4) return 2;
    gnutls_global_init();
    gnutls_certificate_allocate_credentials(&cred);
    if (gnutls_certificate_set_x509_key_file(cred, argv[2], argv[3], GNUTLS_X509_FMT_PEM) < 0) return 1;
    ls = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    sa.sin_port = htons(atoi(argv[1]));
    sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(ls, (struct sockaddr *)&sa, sizeof(sa)) || listen(ls, 8)) return 1;
    for (;;)
    {
        gnutls_session_t s;
        char buf[4096];
        int fd = accept(ls, NULL, NULL), r;

        if (fd < 0) continue;
        gnutls_init(&s, GNUTLS_SERVER | GNUTLS_ENABLE_EARLY_START);
        gnutls_priority_set_direct(s, "NORMAL:-VERS-ALL:+VERS-TLS1.3", NULL);
        gnutls_credentials_set(s, GNUTLS_CRD_CERTIFICATE, cred);
        gnutls_transport_set_int(s, fd);
        /* corked: the handshake flight and the reply leave as one segment, so the client reads
         * them together and gets the reply as SECBUFFER_EXTRA of its last handshake step */
        setsockopt(fd, IPPROTO_TCP, TCP_CORK, &one, sizeof(one));
        do r = gnutls_handshake(s); while (r < 0 && !gnutls_error_is_fatal(r));
        if (r == 0)
        {
            int zero = 0;
            gnutls_record_send(s, reply, sizeof(reply) - 1);       /* before the client's Finished */
            setsockopt(fd, IPPROTO_TCP, TCP_CORK, &zero, sizeof(zero));
            do r = gnutls_record_recv(s, buf, sizeof(buf)); while (r == GNUTLS_E_AGAIN || r == GNUTLS_E_INTERRUPTED);
            gnutls_bye(s, GNUTLS_SHUT_WR);
        }
        gnutls_deinit(s);
        close(fd);
    }
}
