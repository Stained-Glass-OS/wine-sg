/* linuxdialog-gate.sh's Linux program: a window "Gate Main" and its dialog "Gate Dialog"
 * (WM_TRANSIENT_FOR the main one), as a GTK or Qt program's Open dialog. Runs until killed.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
int main( void )
{
    Display *d = XOpenDisplay( NULL );
    Window main, dialog;
    XClassHint hint = { (char *)"gatedialog", (char *)"GateDialogApp" };
    XEvent ev;
    if (!d) return 2;
    main = XCreateSimpleWindow( d, DefaultRootWindow( d ), 100, 100, 500, 350, 0, 0, 0x3060a0 );
    XStoreName( d, main, "Gate Main" );
    XSetClassHint( d, main, &hint );
    XMapWindow( d, main );
    XSync( d, False );
    sleep( 4 );
    dialog = XCreateSimpleWindow( d, DefaultRootWindow( d ), 200, 160, 260, 160, 0, 0, 0xa03060 );
    XStoreName( d, dialog, "Gate Dialog" );
    XSetClassHint( d, dialog, &hint );
    XSetTransientForHint( d, dialog, main );
    {
        /* as Qt's message box: a dialog, its size fixed, no minimize or maximize (_MOTIF_WM_HINTS
         * functions resize | move | close, decorations border | title | menu) */
        XSizeHints size = { 0 };
        long motif[5] = { 3, 0x26, 0x1a, 0, 0 };
        Atom type = XInternAtom( d, "_NET_WM_WINDOW_TYPE_DIALOG", False );
        size.flags = PMinSize | PMaxSize;
        size.min_width = size.max_width = 260;
        size.min_height = size.max_height = 160;
        XSetWMNormalHints( d, dialog, &size );
        XChangeProperty( d, dialog, XInternAtom( d, "_MOTIF_WM_HINTS", False ), XInternAtom( d, "_MOTIF_WM_HINTS", False ),
                         32, PropModeReplace, (unsigned char *)motif, 5 );
        XChangeProperty( d, dialog, XInternAtom( d, "_NET_WM_WINDOW_TYPE", False ), XA_ATOM, 32, PropModeReplace,
                         (unsigned char *)&type, 1 );
    }
    XMapWindow( d, dialog );
    XSync( d, False );
    for (;;) XNextEvent( d, &ev );
    return 0;
}
