/* msvcp140's mutex and condition variable in the layout Visual Studio 2022
 * 17.10's STL constructs inline (its xthreads.h: _Mtx_internal_imp_t is
 * { int _Type; { void *_Unused; SRWLOCK }; ... }, _Cnd_internal_imp_t is
 * { void *_Unused; CONDITION_VARIABLE }) -- wine-sg 0428.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <stdio.h>

typedef void (__cdecl *mtx_init_fn)(void *, int);
typedef int (__cdecl *mtx_fn)(void *);
typedef void (__cdecl *cnd_init_fn)(void *);
typedef int (__cdecl *cnd_wait_fn)(void *, void *);
typedef int (__cdecl *cnd_fn)(void *);

static mtx_init_fn mtx_init; static mtx_fn mtx_lock, mtx_unlock;
static cnd_init_fn cnd_init; static cnd_wait_fn cnd_wait; static cnd_fn cnd_signal;
static BYTE mtx[128] __attribute__((aligned(16))), cnd[64] __attribute__((aligned(16)));
static volatile LONG ready, woke;

static DWORD WINAPI waiter( void *arg )
{
    mtx_lock( mtx );
    InterlockedExchange( &ready, 1 );
    while (!woke) { cnd_wait( cnd, mtx ); InterlockedExchange( &woke, woke | 2 ); }
    mtx_unlock( mtx );
    return 0;
}

int main( void )
{
    HMODULE m = LoadLibraryA( "msvcp140.dll" );
    void **srw = (void **)(mtx + 2 * sizeof(void *));   /* after _Type and _Unused */
    void **cv = (void **)(cnd + sizeof(void *));         /* after _Unused */
    HANDLE t;
    void *before;
    int i;

    mtx_init = (mtx_init_fn)GetProcAddress( m, "_Mtx_init_in_situ" );
    mtx_lock = (mtx_fn)GetProcAddress( m, "_Mtx_lock" );
    mtx_unlock = (mtx_fn)GetProcAddress( m, "_Mtx_unlock" );
    cnd_init = (cnd_init_fn)GetProcAddress( m, "_Cnd_init_in_situ" );
    cnd_wait = (cnd_wait_fn)GetProcAddress( m, "_Cnd_wait" );
    cnd_signal = (cnd_fn)GetProcAddress( m, "_Cnd_signal" );
    if (!mtx_init || !mtx_lock || !mtx_unlock || !cnd_init || !cnd_wait || !cnd_signal)
    { printf( "MISSING\n" ); return 1; }

    mtx_init( mtx, 1 /* plain */ );
    printf( "SRW_FREE %d\n", *srw == NULL );
    mtx_lock( mtx );
    printf( "SRW_HELD %d\n", *srw != NULL );
    mtx_unlock( mtx );
    printf( "SRW_RELEASED %d\n", *srw == NULL );

    cnd_init( cnd );
    before = *cv;
    t = CreateThread( NULL, 0, waiter, NULL, 0, NULL );
    for (i = 0; i < 200 && !ready; i++) Sleep( 10 );
    Sleep( 200 );                                          /* the waiter is in _Cnd_wait */
    mtx_lock( mtx );
    InterlockedExchange( &woke, 1 );
    cnd_signal( cnd );
    mtx_unlock( mtx );
    printf( "WAITER_DONE %d\n", WaitForSingleObject( t, 5000 ) == WAIT_OBJECT_0 );
    /* the condition variable's word is used (a waiter on Windows, a wake
     * count on Wine): it is the one at the STL's offset */
    printf( "CV_USED %d\n", *cv != before );
    return 0;
}
