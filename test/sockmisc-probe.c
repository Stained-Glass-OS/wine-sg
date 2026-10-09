/* Winsock odds and ends (patches/sg/1688), run by test/sockmisc-gate.sh:
 * setsockopt(SO_ERROR) is kept and getsockopt(SO_ERROR) gives it back
 * (it was a stub that dropped it); TransmitPackets, from
 * SIO_GET_EXTENSION_FUNCTION_POINTER (it was not there), sends memory and
 * file elements in order, synchronously and overlapped (with the bytes
 * sent in the result), and TF_DISCONNECT ends the connection. */
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mswsock.h>
#include <stdio.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static void make_pair(SOCKET *client, SOCKET *server)
{
    SOCKET listener = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr = { AF_INET };
    int len = sizeof(addr);

    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    bind(listener, (struct sockaddr *)&addr, sizeof(addr));
    listen(listener, 1);
    getsockname(listener, (struct sockaddr *)&addr, &len);
    *client = socket(AF_INET, SOCK_STREAM, 0);
    connect(*client, (struct sockaddr *)&addr, sizeof(addr));
    *server = accept(listener, NULL, NULL);
    closesocket(listener);
}

static int recv_all(SOCKET s, char *buf, int want)
{
    int got = 0, ret;
    while (got < want && (ret = recv(s, buf + got, want - got, 0)) > 0) got += ret;
    return got;
}

int main(void)
{
    WSADATA wsa;
    GUID guid = WSAID_TRANSMITPACKETS;
    LPFN_TRANSMITPACKETS transmit = NULL;
    TRANSMIT_PACKETS_ELEMENT el[4];
    SOCKET client, server, s;
    WCHAR path[MAX_PATH];
    char buf[64] = {0};
    HANDLE file;
    OVERLAPPED ov = {0};
    DWORD bytes = 0, flags = 0, size;
    int value, len;

    WSAStartup(MAKEWORD(2, 2), &wsa);

    /* SO_ERROR */
    s = socket(AF_INET, SOCK_STREAM, 0);
    value = 1234;
    check(!setsockopt(s, SOL_SOCKET, SO_ERROR, (char *)&value, sizeof(value)), "setsockopt(SO_ERROR, 1234)");
    value = 0;
    len = sizeof(value);
    check(!getsockopt(s, SOL_SOCKET, SO_ERROR, (char *)&value, &len) && value == 1234,
          "getsockopt(SO_ERROR): 1234 back");
    value = 0;
    setsockopt(s, SOL_SOCKET, SO_ERROR, (char *)&value, sizeof(value));
    value = 99;
    getsockopt(s, SOL_SOCKET, SO_ERROR, (char *)&value, &len);
    check(value == 0, "set to 0: 0");
    check(setsockopt(s, SOL_SOCKET, SO_ERROR, (char *)&value, 1) == SOCKET_ERROR && WSAGetLastError() == WSAEFAULT,
          "a short value: WSAEFAULT");
    closesocket(s);

    /* TransmitPackets */
    make_pair(&client, &server);
    check(!WSAIoctl(client, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid, sizeof(guid), &transmit, sizeof(transmit),
                    &size, NULL, NULL) && transmit, "WSAID_TRANSMITPACKETS: a function");
    if (!transmit) goto done;

    GetTempPathW(MAX_PATH, path);
    lstrcatW(path, L"sg-transmit.txt");
    file = CreateFileW(path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS,
                       FILE_FLAG_DELETE_ON_CLOSE, NULL);
    WriteFile(file, "0123456789", 10, &bytes, NULL);

    memset(el, 0, sizeof(el));
    el[0].dwElFlags = TP_ELEMENT_MEMORY;
    el[0].pBuffer = "Hello ";
    el[0].cLength = 6;
    el[1].dwElFlags = TP_ELEMENT_FILE;
    el[1].hFile = file;
    el[1].nFileOffset.QuadPart = 2;
    el[1].cLength = 4;
    el[2].dwElFlags = TP_ELEMENT_MEMORY | TP_ELEMENT_EOP;
    el[2].pBuffer = "!";
    el[2].cLength = 1;
    check(transmit(client, el, 3, 0, NULL, 0), "TransmitPackets: memory, a file range, memory");
    check(recv_all(server, buf, 11) == 11 && !memcmp(buf, "Hello 2345!", 11), "they arrive in order: \"Hello 2345!\"");

    /* a whole file from an offset, overlapped */
    el[0].dwElFlags = TP_ELEMENT_FILE;
    el[0].hFile = file;
    el[0].nFileOffset.QuadPart = 7;
    el[0].cLength = 0;
    ov.hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    bytes = 0;
    if (!transmit(client, el, 1, 0, &ov, 0) && WSAGetLastError() != WSA_IO_PENDING)
        check(0, "TransmitPackets overlapped");
    check(WaitForSingleObject(ov.hEvent, 5000) == WAIT_OBJECT_0 &&
          WSAGetOverlappedResult(client, &ov, &bytes, FALSE, &flags) && bytes == 3,
          "overlapped: the rest of the file, three bytes sent");
    memset(buf, 0, sizeof(buf));
    check(recv_all(server, buf, 3) == 3 && !memcmp(buf, "789", 3), "\"789\" arrives");

    /* TF_DISCONNECT */
    el[0].dwElFlags = TP_ELEMENT_MEMORY;
    el[0].pBuffer = "bye";
    el[0].cLength = 3;
    check(transmit(client, el, 1, 0, NULL, TF_DISCONNECT), "TF_DISCONNECT");
    memset(buf, 0, sizeof(buf));
    check(recv_all(server, buf, 3) == 3 && recv(server, buf + 3, 10, 0) == 0, "the data, then the connection ends");
    CloseHandle(file);
    closesocket(client);
    closesocket(server);

done:
    WSACleanup();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
