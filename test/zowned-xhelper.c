/* zowned-xhelper: what a compositor does to the gate's X windows, and an expose.
 *   redirect WINDOW  -- redirect WINDOW's children (as sg-deskcomp does the Wine
 *                       desktop's) and keep running; their exposures are no
 *                       longer cut by the windows over them
 *   expose WINDOW    -- XClearArea with exposures: the whole window is exposed */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xcomposite.h>

int main( int argc, char **argv )
{
    Display *dpy;
    Window win;
    int ev, err;

    if (argc < 3 || !(dpy = XOpenDisplay( NULL ))) return 2;
    win = strtoul( argv[2], NULL, 0 );
    if (!strcmp( argv[1], "redirect" ))
    {
        if (!XCompositeQueryExtension( dpy, &ev, &err )) return 3;
        XCompositeRedirectSubwindows( dpy, win, CompositeRedirectAutomatic );
        XSync( dpy, False );
        printf( "redirected\n" );
        fflush( stdout );
        for (;;) pause();
    }
    if (!strcmp( argv[1], "expose" ))
    {
        XClearArea( dpy, win, 0, 0, 0, 0, True );
        XSync( dpy, False );
        return 0;
    }
    return 2;
}
