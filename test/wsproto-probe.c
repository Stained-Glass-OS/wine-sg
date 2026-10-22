/* wsproto-probe: the WebSocket Protocol Component API (websocket.dll) between a
 * client handle and a server handle, the "network" a buffer in this process
 * (Stained Glass OS wine-sg 6010). */
#include <windows.h>
#include <stdio.h>
#include <string.h>

/* the API, from its documentation (mingw-w64 has no websocket.h) */
DECLARE_HANDLE(WEB_SOCKET_HANDLE);

typedef enum  _WEB_SOCKET_PROPERTY_TYPE
{
    WEB_SOCKET_RECEIVE_BUFFER_SIZE_PROPERTY_TYPE,
    WEB_SOCKET_SEND_BUFFER_SIZE_PROPERTY_TYPE,
    WEB_SOCKET_DISABLE_MASKING_PROPERTY_TYPE,
    WEB_SOCKET_ALLOCATED_MASKING_PROPERTY_TYPE,
    WEB_SOCKET_DISABLE_UTF8_VERIFICATION_PROPERTY_TYPE,
    WEB_SOCKET_KEEPALIVE_INTERVAL_PROPERTY_TYPE,
    WEB_SOCKET_SUPPORTED_VERSIONS_PROPERTY_TYPE,
} WEB_SOCKET_PROPERTY_TYPE;

typedef struct _WEB_SOCKET_PROPERTY
{
    WEB_SOCKET_PROPERTY_TYPE Type;
    PVOID                    pvValue;
    ULONG                    ulValueSize;
} WEB_SOCKET_PROPERTY, *PWEB_SOCKET_PROPERTY;

#define WEB_SOCKET_MAX_CLOSE_REASON_LENGTH 123

typedef enum _WEB_SOCKET_CLOSE_STATUS
{
    WEB_SOCKET_SUCCESS_CLOSE_STATUS                = 1000,
    WEB_SOCKET_ENDPOINT_UNAVAILABLE_CLOSE_STATUS   = 1001,
    WEB_SOCKET_PROTOCOL_ERROR_CLOSE_STATUS         = 1002,
    WEB_SOCKET_INVALID_DATA_TYPE_CLOSE_STATUS      = 1003,
    WEB_SOCKET_EMPTY_CLOSE_STATUS                  = 1005,
    WEB_SOCKET_ABORTED_CLOSE_STATUS                = 1006,
    WEB_SOCKET_INVALID_PAYLOAD_CLOSE_STATUS        = 1007,
    WEB_SOCKET_POLICY_VIOLATION_CLOSE_STATUS       = 1008,
    WEB_SOCKET_MESSAGE_TOO_BIG_CLOSE_STATUS        = 1009,
    WEB_SOCKET_UNSUPPORTED_EXTENSIONS_CLOSE_STATUS = 1010,
    WEB_SOCKET_SERVER_ERROR_CLOSE_STATUS           = 1011,
    WEB_SOCKET_SECURE_HANDSHAKE_ERROR_CLOSE_STATUS = 1015,
} WEB_SOCKET_CLOSE_STATUS;

typedef enum _WEB_SOCKET_ACTION_QUEUE
{
    WEB_SOCKET_SEND_ACTION_QUEUE    = 0x1,
    WEB_SOCKET_RECEIVE_ACTION_QUEUE = 0x2,
    WEB_SOCKET_ALL_ACTION_QUEUE     = WEB_SOCKET_SEND_ACTION_QUEUE | WEB_SOCKET_RECEIVE_ACTION_QUEUE,
} WEB_SOCKET_ACTION_QUEUE;

typedef enum _WEB_SOCKET_BUFFER_TYPE
{
    WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE     = 0x80000000,
    WEB_SOCKET_UTF8_FRAGMENT_BUFFER_TYPE    = 0x80000001,
    WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE   = 0x80000002,
    WEB_SOCKET_BINARY_FRAGMENT_BUFFER_TYPE  = 0x80000003,
    WEB_SOCKET_CLOSE_BUFFER_TYPE            = 0x80000004,
    WEB_SOCKET_PING_PONG_BUFFER_TYPE        = 0x80000005,
    WEB_SOCKET_UNSOLICITED_PONG_BUFFER_TYPE = 0x80000006,
} WEB_SOCKET_BUFFER_TYPE;

typedef enum _WEB_SOCKET_ACTION
{
    WEB_SOCKET_NO_ACTION                        = 0,
    WEB_SOCKET_SEND_TO_NETWORK_ACTION           = 1,
    WEB_SOCKET_INDICATE_SEND_COMPLETE_ACTION    = 2,
    WEB_SOCKET_RECEIVE_FROM_NETWORK_ACTION      = 3,
    WEB_SOCKET_INDICATE_RECEIVE_COMPLETE_ACTION = 4,
} WEB_SOCKET_ACTION;

typedef struct _WEB_SOCKET_HTTP_HEADER
{
    PCHAR pcName;
    ULONG ulNameLength;
    PCHAR pcValue;
    ULONG ulValueLength;
} WEB_SOCKET_HTTP_HEADER, *PWEB_SOCKET_HTTP_HEADER;

typedef union _WEB_SOCKET_BUFFER
{
    struct
    {
        PBYTE pbBuffer;
        ULONG ulBufferLength;
    } Data;
    struct
    {
        PBYTE pbReason;
        ULONG ulReasonLength;
        USHORT usStatus;
    } CloseStatus;
} WEB_SOCKET_BUFFER, *PWEB_SOCKET_BUFFER;

VOID WINAPI WebSocketAbortHandle(WEB_SOCKET_HANDLE);
HRESULT WINAPI WebSocketBeginClientHandshake(WEB_SOCKET_HANDLE, PCSTR*, ULONG, PCSTR*, ULONG,
        const PWEB_SOCKET_HTTP_HEADER, ULONG, PWEB_SOCKET_HTTP_HEADER*, ULONG*);
HRESULT WINAPI WebSocketBeginServerHandshake(WEB_SOCKET_HANDLE, PCSTR, PCSTR*, ULONG,
        const PWEB_SOCKET_HTTP_HEADER, ULONG, PWEB_SOCKET_HTTP_HEADER*, ULONG*);
VOID WINAPI WebSocketCompleteAction(WEB_SOCKET_HANDLE, PVOID, ULONG);
HRESULT WINAPI WebSocketCreateClientHandle(const PWEB_SOCKET_PROPERTY, ULONG, WEB_SOCKET_HANDLE*);
HRESULT WINAPI WebSocketCreateServerHandle(const PWEB_SOCKET_PROPERTY, ULONG, WEB_SOCKET_HANDLE*);
VOID WINAPI WebSocketDeleteHandle(WEB_SOCKET_HANDLE);
HRESULT WINAPI WebSocketEndClientHandshake(WEB_SOCKET_HANDLE, const PWEB_SOCKET_HTTP_HEADER, ULONG, ULONG*, ULONG*, ULONG*);
HRESULT WINAPI WebSocketEndServerHandshake(WEB_SOCKET_HANDLE);
HRESULT WINAPI WebSocketGetAction(WEB_SOCKET_HANDLE, WEB_SOCKET_ACTION_QUEUE, WEB_SOCKET_BUFFER*, ULONG*,
        WEB_SOCKET_ACTION*, WEB_SOCKET_BUFFER_TYPE*, PVOID*, PVOID*);
HRESULT WINAPI WebSocketGetGlobalProperty(WEB_SOCKET_PROPERTY_TYPE, PVOID, ULONG*);
HRESULT WINAPI WebSocketReceive(WEB_SOCKET_HANDLE, WEB_SOCKET_BUFFER*, PVOID);
HRESULT WINAPI WebSocketSend(WEB_SOCKET_HANDLE, WEB_SOCKET_BUFFER_TYPE, WEB_SOCKET_BUFFER*, PVOID);


#ifndef E_INVALID_PROTOCOL_FORMAT
#define E_INVALID_PROTOCOL_FORMAT ((HRESULT)0x83760002)
#endif

static int failures;

static void check( int ok, const char *what, long got )
{
    if (ok) printf( "ok    %s\n", what );
    else { printf( "FAIL  %s (got %#lx)\n", what, got ); failures++; }
}

static BYTE wire[65536];          /* bytes in flight from one side to the other */
static ULONG wire_len, wire_pos;
static void *send_done;           /* app context of the last send completed */

/* everything the sender has to send goes on the wire */
static void pump_send( WEB_SOCKET_HANDLE h )
{
    WEB_SOCKET_BUFFER bufs[4];
    WEB_SOCKET_ACTION action;
    WEB_SOCKET_BUFFER_TYPE type;
    void *app, *act;
    ULONG count, i;

    for (;;)
    {
        count = 4;
        if (FAILED(WebSocketGetAction( h, WEB_SOCKET_SEND_ACTION_QUEUE, bufs, &count, &action, &type, &app, &act ))) return;
        if (action == WEB_SOCKET_NO_ACTION) return;
        if (action == WEB_SOCKET_SEND_TO_NETWORK_ACTION)
        {
            ULONG total = 0;
            for (i = 0; i < count; i++)
            {
                memcpy( wire + wire_len, bufs[i].Data.pbBuffer, bufs[i].Data.ulBufferLength );
                wire_len += bufs[i].Data.ulBufferLength;
                total += bufs[i].Data.ulBufferLength;
            }
            WebSocketCompleteAction( h, act, total );
        }
        else
        {
            if (action == WEB_SOCKET_INDICATE_SEND_COMPLETE_ACTION) send_done = app;
            WebSocketCompleteAction( h, act, 0 );
        }
    }
}

/* one receive: the data indicated (copied), its type; FALSE if none came */
static BOOL receive( WEB_SOCKET_HANDLE h, BYTE *out, ULONG *len, WEB_SOCKET_BUFFER_TYPE *out_type, USHORT *status )
{
    WEB_SOCKET_BUFFER bufs[2];
    WEB_SOCKET_ACTION action;
    WEB_SOCKET_BUFFER_TYPE type;
    void *app, *act;
    ULONG count, n, round;

    if (FAILED(WebSocketReceive( h, NULL, (void *)0x1234 ))) return FALSE;
    for (round = 0; round < 100; round++)
    {
        count = 2;
        if (FAILED(WebSocketGetAction( h, WEB_SOCKET_RECEIVE_ACTION_QUEUE, bufs, &count, &action, &type, &app, &act )))
            return FALSE;
        if (action == WEB_SOCKET_RECEIVE_FROM_NETWORK_ACTION)
        {
            n = min( bufs[0].Data.ulBufferLength, wire_len - wire_pos );
            if (!n) { WebSocketCompleteAction( h, act, 0 ); return FALSE; }
            memcpy( bufs[0].Data.pbBuffer, wire + wire_pos, n );
            wire_pos += n;
            WebSocketCompleteAction( h, act, n );
        }
        else if (action == WEB_SOCKET_INDICATE_RECEIVE_COMPLETE_ACTION)
        {
            *out_type = type;
            if (type == WEB_SOCKET_CLOSE_BUFFER_TYPE)
            {
                *status = bufs[0].CloseStatus.usStatus;
                *len = bufs[0].CloseStatus.ulReasonLength;
                if (*len) memcpy( out, bufs[0].CloseStatus.pbReason, *len );
            }
            else
            {
                *len = bufs[0].Data.ulBufferLength;
                if (*len) memcpy( out, bufs[0].Data.pbBuffer, *len );
            }
            WebSocketCompleteAction( h, act, 0 );
            return TRUE;
        }
        else return FALSE;
    }
    return FALSE;
}

static void reset_wire( void ) { wire_len = wire_pos = 0; }

static WEB_SOCKET_HANDLE connect_pair( WEB_SOCKET_HANDLE *server, WEB_SOCKET_PROPERTY *sprops, ULONG scount, BOOL tamper,
                                       HRESULT *end_hr )
{
    WEB_SOCKET_HANDLE client = NULL;
    WEB_SOCKET_HTTP_HEADER *chdr, *shdr, req[8];
    ULONG ccount, scount_out, sel_ext, sel_ext_count, sel_proto, i;
    char host[] = "Host", hostval[] = "localhost";
    HRESULT hr;

    WebSocketCreateClientHandle( NULL, 0, &client );
    WebSocketCreateServerHandle( sprops, scount, server );
    hr = WebSocketBeginClientHandshake( client, NULL, 0, NULL, 0, NULL, 0, &chdr, &ccount );
    if (FAILED(hr)) { *end_hr = hr; return client; }
    for (i = 0; i < ccount && i < 7; i++) req[i] = chdr[i];
    req[i].pcName = host; req[i].ulNameLength = 4; req[i].pcValue = hostval; req[i].ulValueLength = 9;
    hr = WebSocketBeginServerHandshake( *server, NULL, NULL, 0, req, i + 1, &shdr, &scount_out );
    if (FAILED(hr)) { *end_hr = hr; return client; }
    if (tamper)
        for (i = 0; i < scount_out; i++)
            if (!_strnicmp( shdr[i].pcName, "Sec-WebSocket-Accept", shdr[i].ulNameLength )) shdr[i].pcValue[0] ^= 1;
    *end_hr = WebSocketEndClientHandshake( client, shdr, scount_out, &sel_ext, &sel_ext_count, &sel_proto );
    WebSocketEndServerHandshake( *server );
    return client;
}

int main( void )
{
    WEB_SOCKET_HANDLE client, server, c2, s2;
    WEB_SOCKET_HTTP_HEADER *hdr;
    WEB_SOCKET_BUFFER buf;
    WEB_SOCKET_BUFFER_TYPE type;
    ULONG count, len, total, v, size, frags, i;
    USHORT status = 0;
    BYTE data[8192], got[8192];
    HRESULT hr, end_hr;
    BOOL key = FALSE, version = FALSE;

    /* the client's handshake headers */
    WebSocketCreateClientHandle( NULL, 0, &c2 );
    hr = WebSocketBeginClientHandshake( c2, NULL, 0, NULL, 0, NULL, 0, &hdr, &count );
    for (i = 0; hr == S_OK && i < count; i++)
    {
        if (!_strnicmp( hdr[i].pcName, "Sec-WebSocket-Key", hdr[i].ulNameLength ) && hdr[i].ulValueLength == 24) key = TRUE;
        if (!_strnicmp( hdr[i].pcName, "Sec-WebSocket-Version", hdr[i].ulNameLength ) && !memcmp( hdr[i].pcValue, "13", 2 ))
            version = TRUE;
    }
    check( hr == S_OK && key && version, "BeginClientHandshake: a 16-byte key, version 13", hr );
    WebSocketDeleteHandle( c2 );

    /* a server whose Sec-WebSocket-Accept is wrong is refused */
    c2 = connect_pair( &s2, NULL, 0, TRUE, &end_hr );
    check( end_hr == E_INVALID_PROTOCOL_FORMAT, "a wrong Sec-WebSocket-Accept: E_INVALID_PROTOCOL_FORMAT", end_hr );
    WebSocketDeleteHandle( c2 );
    WebSocketDeleteHandle( s2 );

    client = connect_pair( &server, NULL, 0, FALSE, &end_hr );
    check( end_hr == S_OK, "the handshake (client and server handles)", end_hr );

    /* client to server: a masked text message */
    reset_wire();
    buf.Data.pbBuffer = (BYTE *)"hello";
    buf.Data.ulBufferLength = 5;
    hr = WebSocketSend( client, WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE, &buf, (void *)0x77 );
    pump_send( client );
    check( hr == S_OK && wire_len == 2 + 4 + 5 && wire[0] == 0x81 && (wire[1] & 0x80) && memcmp( wire + 6, "hello", 5 ),
           "the client's frame: FIN text, masked", wire_len );
    pump_send( client );
    check( send_done == (void *)0x77, "the send is indicated complete with its context", (long)(ULONG_PTR)send_done );
    check( receive( server, got, &len, &type, &status ) && type == WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE &&
           len == 5 && !memcmp( got, "hello", 5 ), "the server receives \"hello\" (a UTF-8 message)", len );

    /* server to client: binary, unmasked */
    reset_wire();
    for (i = 0; i < 3000; i++) data[i] = i * 7;
    buf.Data.pbBuffer = data;
    buf.Data.ulBufferLength = 3000;
    hr = WebSocketSend( server, WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE, &buf, (void *)0x88 );
    pump_send( server );
    check( hr == S_OK && wire[0] == 0x82 && !(wire[1] & 0x80) && wire[1] == 126, "the server's frame: binary, unmasked", wire[1] );
    check( receive( client, got, &len, &type, &status ) && type == WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE &&
           len == 3000 && !memcmp( got, data, 3000 ), "the client receives 3000 bytes (a binary message)", len );
    buf.Data.ulBufferLength = 5000;
    check( WebSocketSend( server, WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE, &buf, NULL ) == E_INVALIDARG,
           "a send larger than the send buffer: E_INVALIDARG", 0 );

    /* ping: answered with a pong by the receiver */
    reset_wire();
    WebSocketSend( client, WEB_SOCKET_PING_PONG_BUFFER_TYPE, NULL, NULL );
    WebSocketSend( client, WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE, &(WEB_SOCKET_BUFFER){ .Data = { (BYTE *)"after", 5 } }, NULL );
    pump_send( client );
    check( receive( server, got, &len, &type, &status ) && type == WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE && len == 5,
           "the ping is no message; the next one is", len );
    reset_wire();
    pump_send( server );
    check( wire_len == 2 && wire[0] == 0x8a, "the server answered the ping with a pong", wire_len ? wire[0] : 0 );

    /* close */
    reset_wire();
    buf.CloseStatus.usStatus = WEB_SOCKET_SUCCESS_CLOSE_STATUS;
    buf.CloseStatus.pbReason = (BYTE *)"bye";
    buf.CloseStatus.ulReasonLength = 3;
    WebSocketSend( client, WEB_SOCKET_CLOSE_BUFFER_TYPE, &buf, NULL );
    pump_send( client );
    check( receive( server, got, &len, &type, &status ) && type == WEB_SOCKET_CLOSE_BUFFER_TYPE && status == 1000 &&
           len == 3 && !memcmp( got, "bye", 3 ), "the server receives the close (1000, \"bye\")", status );
    WebSocketDeleteHandle( client );
    WebSocketDeleteHandle( server );

    /* a receive buffer of 256 bytes: a 1000-byte message comes in fragments */
    {
        WEB_SOCKET_PROPERTY prop;
        ULONG small = 256;
        prop.Type = WEB_SOCKET_RECEIVE_BUFFER_SIZE_PROPERTY_TYPE;
        prop.pvValue = &small;
        prop.ulValueSize = sizeof(small);
        client = connect_pair( &server, &prop, 1, FALSE, &end_hr );
        reset_wire();
        buf.Data.pbBuffer = data;
        buf.Data.ulBufferLength = 1000;
        WebSocketSend( client, WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE, &buf, NULL );
        pump_send( client );
        total = frags = 0;
        while (receive( server, got + total, &len, &type, &status ))
        {
            total += len;
            if (type == WEB_SOCKET_BINARY_FRAGMENT_BUFFER_TYPE) frags++;
            if (type == WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE) break;
        }
        check( frags >= 3 && total == 1000 && type == WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE && !memcmp( got, data, 1000 ),
               "fragments of at most the receive buffer, then the message's end", frags );
        WebSocketDeleteHandle( client );
        WebSocketDeleteHandle( server );
        small = 100;
        check( WebSocketCreateClientHandle( &prop, 1, &client ) == E_INVALIDARG, "a receive buffer under 256: E_INVALIDARG", 0 );
    }

    size = sizeof(v);
    hr = WebSocketGetGlobalProperty( WEB_SOCKET_SUPPORTED_VERSIONS_PROPERTY_TYPE, &v, &size );
    check( hr == S_OK && v == 13, "WebSocketGetGlobalProperty: version 13", v );

    printf( "RESULT: %s\n", failures ? "FAIL" : "PASS" );
    return failures != 0;
}
