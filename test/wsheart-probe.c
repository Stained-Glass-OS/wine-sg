/* wsheart-probe: websocket.dll checks that text messages it receives are UTF-8
 * (E_INVALID_PROTOCOL_FORMAT, the handle aborted; not with
 * WEB_SOCKET_DISABLE_UTF8_VERIFICATION_PROPERTY_TYPE) and sends an unsolicited
 * pong when a connection has been quiet for its keep-alive interval
 * (Stained Glass OS wine-sg 8000). */
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

#ifndef E_INVALID_PROTOCOL_OPERATION
#define E_INVALID_PROTOCOL_OPERATION ((HRESULT)0x83760001)
#endif

static int failures;

static void check( int ok, const char *what, long got )
{
    if (ok) printf( "ok    %s\n", what );
    else { printf( "FAIL  %s (got %#lx)\n", what, got ); failures++; }
}

static BYTE wire[65536];
static ULONG wire_len, wire_pos;

static void pump_send( WEB_SOCKET_HANDLE h )
{
    WEB_SOCKET_BUFFER bufs[4];
    WEB_SOCKET_ACTION action;
    WEB_SOCKET_BUFFER_TYPE type;
    void *app, *act;
    ULONG count, i, total;

    for (;;)
    {
        count = 4;
        if (FAILED(WebSocketGetAction( h, WEB_SOCKET_SEND_ACTION_QUEUE, bufs, &count, &action, &type, &app, &act ))) return;
        if (action == WEB_SOCKET_NO_ACTION) return;
        total = 0;
        if (action == WEB_SOCKET_SEND_TO_NETWORK_ACTION)
            for (i = 0; i < count; i++)
            {
                memcpy( wire + wire_len, bufs[i].Data.pbBuffer, bufs[i].Data.ulBufferLength );
                wire_len += bufs[i].Data.ulBufferLength;
                total += bufs[i].Data.ulBufferLength;
            }
        WebSocketCompleteAction( h, act, total );
    }
}

/* one receive: S_OK and the indication, S_FALSE when nothing came, or GetAction's error */
static HRESULT receive( WEB_SOCKET_HANDLE h, BYTE *out, ULONG *len, WEB_SOCKET_BUFFER_TYPE *out_type )
{
    WEB_SOCKET_BUFFER bufs[2];
    WEB_SOCKET_ACTION action;
    WEB_SOCKET_BUFFER_TYPE type;
    void *app, *act;
    ULONG count, n, round;
    HRESULT hr;

    if (FAILED(hr = WebSocketReceive( h, NULL, NULL ))) return hr;
    for (round = 0; round < 100; round++)
    {
        count = 2;
        if (FAILED(hr = WebSocketGetAction( h, WEB_SOCKET_RECEIVE_ACTION_QUEUE, bufs, &count, &action, &type, &app, &act )))
            return hr;
        if (action == WEB_SOCKET_RECEIVE_FROM_NETWORK_ACTION)
        {
            n = min( bufs[0].Data.ulBufferLength, wire_len - wire_pos );
            memcpy( bufs[0].Data.pbBuffer, wire + wire_pos, n );
            wire_pos += n;
            WebSocketCompleteAction( h, act, n );
            if (!n) return S_FALSE;
        }
        else if (action == WEB_SOCKET_INDICATE_RECEIVE_COMPLETE_ACTION)
        {
            *out_type = type;
            *len = type == WEB_SOCKET_CLOSE_BUFFER_TYPE ? 0 : bufs[0].Data.ulBufferLength;
            if (*len) memcpy( out, bufs[0].Data.pbBuffer, *len );
            WebSocketCompleteAction( h, act, 0 );
            return S_OK;
        }
        else return S_FALSE;
    }
    return S_FALSE;
}

static void reset_wire( void ) { wire_len = wire_pos = 0; }

static WEB_SOCKET_HANDLE pair( WEB_SOCKET_HANDLE *server, WEB_SOCKET_PROPERTY *cprops, ULONG ccount,
                               WEB_SOCKET_PROPERTY *sprops, ULONG scount )
{
    WEB_SOCKET_HANDLE client = NULL;
    WEB_SOCKET_HTTP_HEADER *chdr, *shdr, req[8];
    ULONG n, sn, e1, e2, p, i;
    char host[] = "Host", hostval[] = "localhost";

    *server = NULL;
    if (FAILED(WebSocketCreateClientHandle( cprops, ccount, &client ))) return NULL;
    if (FAILED(WebSocketCreateServerHandle( sprops, scount, server ))) return NULL;
    if (FAILED(WebSocketBeginClientHandshake( client, NULL, 0, NULL, 0, NULL, 0, &chdr, &n ))) return NULL;
    for (i = 0; i < n && i < 7; i++) req[i] = chdr[i];
    req[i].pcName = host; req[i].ulNameLength = 4; req[i].pcValue = hostval; req[i].ulValueLength = 9;
    if (FAILED(WebSocketBeginServerHandshake( *server, NULL, NULL, 0, req, i + 1, &shdr, &sn ))) return NULL;
    if (FAILED(WebSocketEndClientHandshake( client, shdr, sn, &e1, &e2, &p ))) return NULL;
    if (FAILED(WebSocketEndServerHandshake( *server ))) return NULL;
    return client;
}

/* the client sends one buffer (a message or a fragment) */
static void csend( WEB_SOCKET_HANDLE c, WEB_SOCKET_BUFFER_TYPE type, const char *data, ULONG len )
{
    WEB_SOCKET_BUFFER buf;
    buf.Data.pbBuffer = (BYTE *)data;
    buf.Data.ulBufferLength = len;
    WebSocketSend( c, type, &buf, NULL );
    pump_send( c );
}

/* a text message from a new client to a new server: the server's receive result */
static HRESULT text_result( const char *data, ULONG len, WEB_SOCKET_PROPERTY *sprops, ULONG scount, HRESULT *after )
{
    WEB_SOCKET_HANDLE c, s;
    WEB_SOCKET_BUFFER_TYPE type;
    BYTE got[256];
    ULONG glen;
    HRESULT hr;

    reset_wire();
    if (!(c = pair( &s, NULL, 0, sprops, scount ))) return E_FAIL;
    csend( c, WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE, data, len );
    hr = receive( s, got, &glen, &type );
    if (hr == S_OK && (type != WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE || glen != len || memcmp( got, data, len ))) hr = E_UNEXPECTED;
    if (after) *after = receive( s, got, &glen, &type );
    WebSocketDeleteHandle( c );
    WebSocketDeleteHandle( s );
    return hr;
}

int main( void )
{
    static const char good[] = "h\xc3\xa9llo \xe2\x82\xac \xf0\x9d\x84\x9e";
    WEB_SOCKET_HANDLE c, s, ka, none, clamp, ks, ns, cs;
    WEB_SOCKET_PROPERTY prop;
    WEB_SOCKET_BUFFER_TYPE type;
    ULONG v, glen;
    BYTE got[256];
    HRESULT hr, after;

    /* UTF-8 */
    hr = text_result( good, sizeof(good) - 1, NULL, 0, NULL );
    check( hr == S_OK, "a UTF-8 text message (2-, 3- and 4-byte sequences) is received", hr );
    hr = text_result( "a\xc0\xaf" "b", 4, NULL, 0, &after );
    check( hr == E_INVALID_PROTOCOL_FORMAT, "an overlong form: E_INVALID_PROTOCOL_FORMAT", hr );
    check( after == E_INVALID_PROTOCOL_OPERATION, "the handle is aborted after it", after );
    hr = text_result( "\xed\xa0\x80", 3, NULL, 0, NULL );
    check( hr == E_INVALID_PROTOCOL_FORMAT, "a UTF-16 surrogate: E_INVALID_PROTOCOL_FORMAT", hr );
    hr = text_result( "\xf4\x90\x80\x80", 4, NULL, 0, NULL );
    check( hr == E_INVALID_PROTOCOL_FORMAT, "a code point above U+10FFFF: E_INVALID_PROTOCOL_FORMAT", hr );
    hr = text_result( "abc\xe2\x82", 5, NULL, 0, NULL );
    check( hr == E_INVALID_PROTOCOL_FORMAT, "a message ending inside a sequence: E_INVALID_PROTOCOL_FORMAT", hr );
    hr = text_result( "\xff", 1, NULL, 0, NULL );
    check( hr == E_INVALID_PROTOCOL_FORMAT, "a 0xff byte: E_INVALID_PROTOCOL_FORMAT", hr );
    v = 1;
    prop.Type = WEB_SOCKET_DISABLE_UTF8_VERIFICATION_PROPERTY_TYPE;
    prop.pvValue = &v;
    prop.ulValueSize = sizeof(v);
    hr = text_result( "a\xc0\xaf" "b", 4, &prop, 1, NULL );
    check( hr == S_OK, "with DISABLE_UTF8_VERIFICATION the same message is received", hr );

    /* a sequence split between two fragments of one message */
    reset_wire();
    c = pair( &s, NULL, 0, NULL, 0 );
    csend( c, WEB_SOCKET_UTF8_FRAGMENT_BUFFER_TYPE, "x\xe2\x82", 3 );
    csend( c, WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE, "\xac", 1 );
    hr = receive( s, got, &glen, &type );
    check( hr == S_OK && type == WEB_SOCKET_UTF8_FRAGMENT_BUFFER_TYPE, "first fragment (ends inside a sequence)", hr );
    hr = receive( s, got, &glen, &type );
    check( hr == S_OK && type == WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE && glen == 1, "the message's end completes it", hr );
    /* binary data is not checked */
    reset_wire();
    csend( c, WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE, "\xff\xc0", 2 );
    hr = receive( s, got, &glen, &type );
    check( hr == S_OK && type == WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE && glen == 2, "binary messages are not checked", hr );
    WebSocketDeleteHandle( c );
    WebSocketDeleteHandle( s );

    /* keep-alive */
    prop.Type = WEB_SOCKET_KEEPALIVE_INTERVAL_PROPERTY_TYPE;
    v = 15000;
    ka = pair( &ks, &prop, 1, NULL, 0 );
    v = 0;
    none = pair( &ns, &prop, 1, NULL, 0 );
    v = 1000;
    clamp = pair( &cs, &prop, 1, NULL, 0 );
    check( ka && none && clamp, "client handles with keep-alive 15000, 0 and 1000", 0 );
    reset_wire();
    pump_send( ka );
    check( wire_len == 0, "nothing is sent before the interval", wire_len );
    Sleep( 2500 );
    reset_wire();
    pump_send( clamp );
    check( wire_len == 0, "an interval under 15 seconds is 15 seconds (nothing after 2.5 s)", wire_len );
    Sleep( 13000 );
    reset_wire();
    pump_send( ka );
    check( wire_len == 6 && wire[0] == 0x8a && wire[1] == 0x80, "after 15 s of quiet: an unsolicited pong, masked, empty",
           wire_len ? wire[0] : 0 );
    hr = receive( ks, got, &glen, &type );
    check( hr == S_OK && type == WEB_SOCKET_UNSOLICITED_PONG_BUFFER_TYPE && glen == 0, "the server sees an unsolicited pong", hr );
    reset_wire();
    pump_send( ka );
    check( wire_len == 0, "not again at once", wire_len );
    reset_wire();
    pump_send( clamp );
    check( wire_len == 6 && wire[0] == 0x8a, "the 1000 ms handle has sent one too by now", wire_len );
    reset_wire();
    pump_send( none );
    check( wire_len == 0, "keep-alive 0: none", wire_len );

    WebSocketDeleteHandle( ka ); WebSocketDeleteHandle( ks );
    WebSocketDeleteHandle( none ); WebSocketDeleteHandle( ns );
    WebSocketDeleteHandle( clamp ); WebSocketDeleteHandle( cs );

    printf( "RESULT: %s\n", failures ? "FAIL" : "PASS" );
    return failures != 0;
}
