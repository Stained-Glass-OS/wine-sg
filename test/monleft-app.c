/* monleft-app.c: monleft-gate.sh's Linux program -- one plain X window, 600x400, titled NAME
 * (a terminal's, FileZilla's: decorated, no place asked), mapped; its id goes to LOG.
 *   monleft-app NAME LOG SECONDS
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

int main( int argc, char **argv )
{
    Display *d;
    Window w;
    FILE *f;

    if (argc < 4 || !(d = XOpenDisplay( NULL ))) return 1;
    w = XCreateSimpleWindow( d, DefaultRootWindow( d ), 0, 0, 600, 400, 0, 0, 0x208040 );
    XStoreName( d, w, argv[1] );
    XMapWindow( d, w );
    XSync( d, False );
    if ((f = fopen( argv[2], "a" ))) { fprintf( f, "%s %lu\n", argv[1], (unsigned long)w ); fclose( f ); }
    sleep( atoi( argv[3] ) );
    return 0;
}
