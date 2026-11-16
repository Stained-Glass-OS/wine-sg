/* Files copied in Wine as text/uri-list (wine-sg 7789), for test/urilist-gate.sh:
 * a native X program converting CLIPBOARD to text/uri-list, as a file manager
 * pasting does; the property's bytes go to stdout. */
#include <X11/Xlib.h>
#include <stdio.h>
#include <unistd.h>

int main( void )
{
    Display *dpy = XOpenDisplay( NULL );
    Window win;
    Atom type, sel, target, prop;
    XEvent ev;
    int format, i;
    unsigned long count, after;
    unsigned char *data = NULL;

    if (!dpy) return 2;
    win = XCreateSimpleWindow( dpy, DefaultRootWindow( dpy ), 0, 0, 1, 1, 0, 0, 0 );
    sel = XInternAtom( dpy, "CLIPBOARD", False );
    target = XInternAtom( dpy, "text/uri-list", False );
    prop = XInternAtom( dpy, "SG_PASTE", False );
    XConvertSelection( dpy, sel, target, prop, win, CurrentTime );
    XFlush( dpy );
    for (i = 0; i < 1000; i++)
    {
        if (XCheckTypedWindowEvent( dpy, win, SelectionNotify, &ev )) break;
        usleep( 10000 );
    }
    if (i == 1000 || ev.xselection.property == None) { fprintf( stderr, "no text/uri-list\n" ); return 1; }
    if (XGetWindowProperty( dpy, win, prop, 0, 1 << 20, True, AnyPropertyType, &type, &format, &count,
                            &after, &data ) != Success || !data) return 1;
    fwrite( data, 1, count, stdout );
    return 0;
}
