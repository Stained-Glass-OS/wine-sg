/* OpenMP schedule(dynamic) loops as MSVC compiles them (patches/sg/6547).
 * MSVC passes 0x20 besides the schedule kind (digiKam's libraw: 0x62 =
 * increment | 0x20 | chunked, chunks of 1) and, knowing the chunk size, runs
 * one iteration per _vcomp_for_dynamic_next. Wine took 0x22 as an unknown kind
 * and handed out guided chunks: only each chunk's first row was processed and
 * digiKam showed RAW photos as a few lines. The probe forks a team on
 * vcomp140.dll, runs such a loop over 2844 "rows" processing only *begin, and
 * checks that every row was done, once. */
#include <windows.h>
#include <stdio.h>

static void (CDECL *p_fork)( BOOL ifval, int nargs, void *wrapper, ... );
static void (CDECL *p_dyn_init)( unsigned int flags, unsigned int first, unsigned int last, int step, unsigned int chunk );
static int (CDECL *p_dyn_next)( unsigned int *begin, unsigned int *end );
static void (CDECL *p_set_threads)( int n );

#define ROWS 2844
static LONG done[ROWS];

static void CDECL loop( void )
{
    unsigned int begin, end;

    p_dyn_init( 0x62, 0, ROWS - 1, 1, 1 );
    while (p_dyn_next( &begin, &end ))
        InterlockedIncrement( &done[begin] );      /* one row: the chunk is one iteration */
}

int main( void )
{
    HMODULE vcomp = LoadLibraryA( "vcomp140.dll" );
    int threads, i, missing, twice;

    if (!vcomp) { printf( "no vcomp140\n" ); return 1; }
    p_fork = (void *)GetProcAddress( vcomp, "_vcomp_fork" );
    p_dyn_init = (void *)GetProcAddress( vcomp, "_vcomp_for_dynamic_init" );
    p_dyn_next = (void *)GetProcAddress( vcomp, "_vcomp_for_dynamic_next" );
    p_set_threads = (void *)GetProcAddress( vcomp, "omp_set_num_threads" );
    if (!p_fork || !p_dyn_init || !p_dyn_next || !p_set_threads) { printf( "missing exports\n" ); return 1; }

    for (threads = 1; threads <= 8; threads *= 2)
    {
        memset( done, 0, sizeof(done) );
        p_set_threads( threads );
        p_fork( TRUE, 0, loop );
        for (i = missing = twice = 0; i < ROWS; i++)
        {
            if (!done[i]) missing++;
            if (done[i] > 1) twice++;
        }
        printf( "threads=%d missing=%d twice=%d\n", threads, missing, twice );
        printf( "rows_%d=%d\n", threads, !missing && !twice );
    }
    printf( "done=1\n" );
    return 0;
}
