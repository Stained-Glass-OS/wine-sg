/* Wine as an XDND source (wine-sg 7787): a small native X program that takes
 * XDND drops, for test/xdndsrc-gate.sh.
 *
 *   xdndsrc-target LOG X Y W H [INTO_NAME]
 *
 * A window at X,Y (W x H) speaking XDND 5. With INTO_NAME it goes into the X
 * window of that name (a Linux program in a Wine window's frame). What it does
 * with a drop comes from the file LOG.mode, read at each XdndEnter:
 *   copy     accept the proposed action, finish with it
 *   move     accept with XdndActionMove, finish with move (the files untouched)
 *   movedel  as move, and convert DELETE first (asks the source to delete)
 *   refuse   do not accept
 * LOG gets one line per event: ENTER, TYPE name, ACTION name, LEAVE, DROP,
 * DATA type, DELETE, FINISHED action; LOG.data the dropped data. */
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/cursorfont.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static Display *dpy;
static FILE *logf;
static const char *logname;
static char mode[16] = "copy";

static Atom A( const char *name ) { return XInternAtom( dpy, name, False ); }

static void say( const char *fmt, const char *arg )
{
    fprintf( logf, fmt, arg );
    fputc( '\n', logf );
    fflush( logf );
}

static Window find_named( Window w, const char *name )
{
    Window root, parent, *children = NULL, found = 0;
    unsigned int n, i;
    char *wm_name = NULL;

    if (XFetchName( dpy, w, &wm_name ) && wm_name)
    {
        int same = !strcmp( wm_name, name );
        XFree( wm_name );
        if (same) return w;
    }
    if (!XQueryTree( dpy, w, &root, &parent, &children, &n )) return 0;
    for (i = 0; i < n && !found; i++) found = find_named( children[i], name );
    if (children) XFree( children );
    return found;
}

static void read_mode(void)
{
    char path[4096];
    FILE *f;

    snprintf( path, sizeof(path), "%s.mode", logname );
    if ((f = fopen( path, "r" )))
    {
        if (fscanf( f, "%15s", mode ) != 1) strcpy( mode, "copy" );
        fclose( f );
    }
}

static int wait_notify( Window win, XEvent *ev )
{
    int i;
    for (i = 0; i < 500; i++)
    {
        if (XCheckTypedWindowEvent( dpy, win, SelectionNotify, ev )) return 1;
        usleep( 10000 );
    }
    return 0;
}

int main( int argc, char **argv )
{
    Window win, src = 0;
    Atom types[64], proposed = 0, last_action = 0;
    int ntypes = 0, version = 5;
    XSetWindowAttributes attr;
    XEvent ev;

    if (argc < 6) return 2;
    logname = argv[1];
    if (!(logf = fopen( logname, "a" ))) return 2;
    if (!(dpy = XOpenDisplay( NULL ))) return 2;
    attr.override_redirect = True;
    attr.background_pixel = 0x3366cc;
    attr.event_mask = StructureNotifyMask | PropertyChangeMask;
    win = XCreateWindow( dpy, DefaultRootWindow( dpy ), atoi( argv[2] ), atoi( argv[3] ), atoi( argv[4] ),
                         atoi( argv[5] ), 0, CopyFromParent, InputOutput, CopyFromParent,
                         CWOverrideRedirect | CWBackPixel | CWEventMask, &attr );
    XStoreName( dpy, win, "sg-xdnd-native" );
    XDefineCursor( dpy, win, XCreateFontCursor( dpy, XC_pirate ) );  /* its own cursor, unlike any drag's */
    XChangeProperty( dpy, win, A("XdndAware"), XA_ATOM, 32, PropModeReplace, (unsigned char *)&(long){5}, 1 );
    if (argc > 6)
    {
        Window into = 0;
        int i;
        for (i = 0; i < 100 && !(into = find_named( DefaultRootWindow( dpy ), argv[6] )); i++) usleep( 100000 );
        if (!into) { say( "NOTFOUND %s", argv[6] ); return 1; }
        XReparentWindow( dpy, win, into, atoi( argv[2] ), atoi( argv[3] ) );
    }
    XMapRaised( dpy, win );
    XSync( dpy, False );
    say( "READY %s", argc > 6 ? argv[6] : "root" );

    for (;;)
    {
        XNextEvent( dpy, &ev );
        if (ev.type != ClientMessage) continue;
        if (ev.xclient.message_type == A("XdndEnter"))
        {
            int i;
            read_mode();
            src = ev.xclient.data.l[0];
            version = (unsigned long)ev.xclient.data.l[1] >> 24;
            ntypes = 0;
            proposed = last_action = 0;
            if (ev.xclient.data.l[1] & 1)
            {
                Atom type, *list = NULL;
                int format;
                unsigned long count, after;
                if (XGetWindowProperty( dpy, src, A("XdndTypeList"), 0, 64, False, XA_ATOM, &type, &format,
                                        &count, &after, (unsigned char **)&list ) == Success && list)
                {
                    for (i = 0; i < (int)count && ntypes < 64; i++) types[ntypes++] = list[i];
                    XFree( list );
                }
            }
            else
                for (i = 2; i < 5; i++) if (ev.xclient.data.l[i]) types[ntypes++] = ev.xclient.data.l[i];
            fprintf( logf, "ENTER v=%d mode=%s\n", version, mode );
            for (i = 0; i < ntypes; i++) say( "TYPE %s", XGetAtomName( dpy, types[i] ) );
        }
        else if (ev.xclient.message_type == A("XdndPosition"))
        {
            XEvent r;
            Atom action = ev.xclient.data.l[4];
            int accept = strcmp( mode, "refuse" ) != 0;

            if (action != proposed)
            {
                proposed = action;
                say( "ACTION %s", action ? XGetAtomName( dpy, action ) : "none" );
            }
            if (!strncmp( mode, "move", 4 )) action = A("XdndActionMove");
            last_action = accept ? action : 0;
            memset( &r, 0, sizeof(r) );
            r.xclient.type = ClientMessage;
            r.xclient.window = src;
            r.xclient.message_type = A("XdndStatus");
            r.xclient.format = 32;
            r.xclient.data.l[0] = win;
            r.xclient.data.l[1] = accept ? 1 : 0;
            r.xclient.data.l[4] = last_action;
            XSendEvent( dpy, src, False, NoEventMask, &r );
            XFlush( dpy );
        }
        else if (ev.xclient.message_type == A("XdndLeave"))
        {
            say( "%s", "LEAVE" );
        }
        else if (ev.xclient.message_type == A("XdndDrop"))
        {
            Atom want = 0, type;
            Time t = ev.xclient.data.l[2];
            XEvent n, r;
            int i, format;
            unsigned long count, after;
            unsigned char *data = NULL;
            const char *pref[] = { "text/uri-list", "text/plain;charset=utf-8", "UTF8_STRING" };
            int j;

            say( "%s", "DROP" );
            for (j = 0; j < 3 && !want; j++)
                for (i = 0; i < ntypes; i++) if (types[i] == A(pref[j])) { want = types[i]; break; }
            if (want)
            {
                XConvertSelection( dpy, A("XdndSelection"), want, A("SGX_DROP"), win, t );
                XFlush( dpy );
                if (wait_notify( win, &n ) && n.xselection.property &&
                    XGetWindowProperty( dpy, win, A("SGX_DROP"), 0, 1 << 20, True, AnyPropertyType, &type,
                                        &format, &count, &after, &data ) == Success && data)
                {
                    char path[4096];
                    FILE *f;
                    snprintf( path, sizeof(path), "%s.data", logname );
                    if ((f = fopen( path, "w" )))
                    {
                        fwrite( data, 1, count, f );
                        fclose( f );
                    }
                    say( "DATA %s", XGetAtomName( dpy, want ) );
                    XFree( data );
                }
                else say( "NODATA %s", XGetAtomName( dpy, want ) );
            }
            if (!strcmp( mode, "movedel" ))
            {
                XConvertSelection( dpy, A("XdndSelection"), A("DELETE"), A("SGX_DEL"), win, t );
                XFlush( dpy );
                if (wait_notify( win, &n ) && n.xselection.property) say( "%s", "DELETE" );
            }
            memset( &r, 0, sizeof(r) );
            r.xclient.type = ClientMessage;
            r.xclient.window = src;
            r.xclient.message_type = A("XdndFinished");
            r.xclient.format = 32;
            r.xclient.data.l[0] = win;
            r.xclient.data.l[1] = last_action ? 1 : 0;
            r.xclient.data.l[2] = last_action;
            XSendEvent( dpy, src, False, NoEventMask, &r );
            XFlush( dpy );
            say( "FINISHED %s", last_action ? XGetAtomName( dpy, last_action ) : "none" );
        }
    }
    return 0;
}
