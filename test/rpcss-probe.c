/* rpcss-probe: RPC stub memory management, RpcSs* / RpcSm* and the NDR helpers (Stained Glass OS wine-sg 8226).
 * They were spec stubs: a program calling RpcSsAllocate crashed (then no RESULT line). Prints ok/FAIL lines and RESULT.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <rpc.h>
#include <rpcndr.h>
#include <stdio.h>

static int failures;
static void check( int ok, const char *what, long got )
{
    if (ok) printf( "ok    %s\n", what );
    else { printf( "FAIL  %s (got %#lx)\n", what, got ); failures++; }
}

static int allocs, frees;
static void * __RPC_USER my_alloc( size_t size ) { allocs++; return HeapAlloc( GetProcessHeap(), 0, size ); }
static void __RPC_USER my_free( void *p ) { frees++; HeapFree( GetProcessHeap(), 0, p ); }

static RPC_SS_THREAD_HANDLE shared;
static void *from_other;

static DWORD WINAPI other_thread( void *arg )
{
    RpcSsSetThreadHandle( shared );
    from_other = RpcSsAllocate( 32 );
    RpcSsSetThreadHandle( NULL );
    return 0;
}

int main( void )
{
    MIDL_STUB_MESSAGE msg;
    RPC_CLIENT_ALLOC *old_alloc = NULL;
    RPC_CLIENT_FREE *old_free = NULL;
    RPC_STATUS status = 1;
    void *a, *b, *c;
    HANDLE thread;

    {
        RpcSsEnableAllocate();
        a = RpcSsAllocate( 100 );
        b = RpcSsAllocate( 0 );
        c = RpcSsAllocate( 4096 );
        memset( a, 0xaa, 100 );
        memset( c, 0xcc, 4096 );
        check( a && b && c && a != b && b != c, "RpcSsAllocate gives distinct blocks", 0 );
        RpcSsFree( b );
        shared = RpcSsGetThreadHandle();
        check( shared != NULL, "RpcSsGetThreadHandle in an enabled environment", 0 );
        thread = CreateThread( NULL, 0, other_thread, NULL, 0, NULL );
        WaitForSingleObject( thread, 5000 );
        check( from_other != NULL, "another thread allocates in a shared environment (RpcSsSetThreadHandle)", 0 );
        RpcSsDisableAllocate();
        check( RpcSsGetThreadHandle() == NULL, "RpcSsDisableAllocate ends the environment", 0 );

        status = RpcSmEnableAllocate();
        check( status == RPC_S_OK, "RpcSmEnableAllocate", status );
        a = RpcSmAllocate( 64, &status );
        check( a && status == RPC_S_OK, "RpcSmAllocate", status );
        status = RpcSmFree( a );
        check( status == RPC_S_OK, "RpcSmFree", status );
        status = RpcSmFree( (void *)&status );
        check( status != RPC_S_OK, "RpcSmFree of memory that is not the environment's fails", status );
        (void)RpcSmGetThreadHandle( &status );
        check( status == RPC_S_OK, "RpcSmGetThreadHandle", status );
        status = RpcSmDisableAllocate();
        check( status == RPC_S_OK, "RpcSmDisableAllocate", status );

        RpcSsSetClientAllocFree( my_alloc, my_free );
        a = NdrRpcSmClientAllocate( 10 );
        NdrRpcSmClientFree( a );
        check( allocs == 1 && frees == 1, "the client allocator set with RpcSsSetClientAllocFree is used", allocs * 10 + frees );
        RpcSsSwapClientAllocFree( NULL, NULL, &old_alloc, &old_free );
        check( old_alloc == my_alloc && old_free == my_free, "RpcSsSwapClientAllocFree gives the old pair back", 0 );
        a = NdrRpcSmClientAllocate( 10 );
        check( a != NULL && allocs == 1, "without one the default client allocator", allocs );
        RpcSmClientFree( a );

        memset( &msg, 0, sizeof(msg) );
        NdrRpcSsEnableAllocate( &msg );
        check( msg.pfnAllocate == NdrRpcSsDefaultAllocate && msg.pfnFree == NdrRpcSsDefaultFree,
               "NdrRpcSsEnableAllocate sets the stub's allocators", 0 );
        a = msg.pfnAllocate( 20 );
        check( a != NULL && RpcSsGetThreadHandle() != NULL, "and the environment they allocate in", 0 );
        NdrRpcSsDisableAllocate( &msg );
        check( RpcSsGetThreadHandle() == NULL, "NdrRpcSsDisableAllocate frees it", 0 );
        memset( &msg, 0, sizeof(msg) );
        NdrRpcSmSetClientToOsf( &msg );
        check( msg.pfnAllocate == NdrRpcSmClientAllocate && msg.pfnFree == NdrRpcSmClientFree,
               "NdrRpcSmSetClientToOsf sets the client allocators", 0 );
    }
    printf( "RESULT: %s\n", failures ? "FAIL" : "PASS" );
    return failures != 0;
}
