/* embedmaxgrab-grabcheck: can another client take the pointer? Prints
 * "pointer=free" or "pointer=grabbed (CODE)" -- a grab left behind by
 * another client makes every click go to that client's window.
 * For embedmaxgrab-gate.sh. SPDX-License-Identifier: LGPL-2.1-or-later */
#include <stdio.h>
#include <X11/Xlib.h>

int main( void )
{
    Display *d = XOpenDisplay( NULL );
    int st;

    if (!d) return 2;
    st = XGrabPointer( d, DefaultRootWindow( d ), False, ButtonPressMask, GrabModeAsync, GrabModeAsync,
                       None, None, CurrentTime );
    if (st == GrabSuccess)
    {
        XUngrabPointer( d, CurrentTime );
        printf( "pointer=free\n" );
    }
    else printf( "pointer=grabbed (%d)\n", st );
    XSync( d, False );
    XCloseDisplay( d );
    return 0;
}
