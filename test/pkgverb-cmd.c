/* pkgverb-cmd: a packaged File Explorer command (IExplorerCommand) for test/pkgverb-gate.sh
 * (Stained Glass OS wine-sg 6029). Three classes:
 *   {5a3c1d10-...-0001}  "SG Package Verb": enabled; Invoke writes the items' paths to C:\pkgverb-log.txt
 *   {5a3c1d10-...-0002}  "SG Package Menu": sub-commands "Sub A" (invokable), "Sub B" (hidden),
 *                        "Sub C" (disabled)
 *   {5a3c1d10-...-0003}  "SG Hidden Verb": hidden
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include <windows.h>
#include <shobjidl.h>
#include <stdio.h>

static const GUID clsid_verb = {0x5a3c1d10,0x6c1e,0x4d57,{0x9b,0x8a,0x2f,0x4e,0x7c,0x9d,0x00,0x01}};
static const GUID clsid_menu = {0x5a3c1d10,0x6c1e,0x4d57,{0x9b,0x8a,0x2f,0x4e,0x7c,0x9d,0x00,0x02}};
static const GUID clsid_hidden = {0x5a3c1d10,0x6c1e,0x4d57,{0x9b,0x8a,0x2f,0x4e,0x7c,0x9d,0x00,0x03}};

struct command
{
    IExplorerCommand IExplorerCommand_iface;
    LONG ref;
    const WCHAR *title;
    EXPCMDSTATE state;
    EXPCMDFLAGS flags;
};

static HRESULT command_create( const WCHAR *title, EXPCMDSTATE state, EXPCMDFLAGS flags, IExplorerCommand **out );

static struct command *impl( IExplorerCommand *iface ) { return CONTAINING_RECORD( iface, struct command, IExplorerCommand_iface ); }

static HRESULT WINAPI cmd_QueryInterface( IExplorerCommand *iface, REFIID iid, void **out )
{
    if (IsEqualIID( iid, &IID_IUnknown ) || IsEqualIID( iid, &IID_IExplorerCommand ))
    {
        *out = iface;
        IExplorerCommand_AddRef( iface );
        return S_OK;
    }
    *out = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI cmd_AddRef( IExplorerCommand *iface ) { return InterlockedIncrement( &impl( iface )->ref ); }
static ULONG WINAPI cmd_Release( IExplorerCommand *iface )
{
    ULONG ref = InterlockedDecrement( &impl( iface )->ref );
    if (!ref) HeapFree( GetProcessHeap(), 0, impl( iface ) );
    return ref;
}
static HRESULT dup( const WCHAR *s, WCHAR **out )
{
    size_t n = (lstrlenW( s ) + 1) * sizeof(WCHAR);
    if (!(*out = CoTaskMemAlloc( n ))) return E_OUTOFMEMORY;
    memcpy( *out, s, n );
    return S_OK;
}
static HRESULT WINAPI cmd_GetTitle( IExplorerCommand *iface, IShellItemArray *items, WCHAR **name ) { return dup( impl( iface )->title, name ); }
static HRESULT WINAPI cmd_GetIcon( IExplorerCommand *iface, IShellItemArray *items, WCHAR **icon ) { *icon = NULL; return E_NOTIMPL; }
static HRESULT WINAPI cmd_GetToolTip( IExplorerCommand *iface, IShellItemArray *items, WCHAR **tip ) { return dup( L"SG tool tip", tip ); }
static HRESULT WINAPI cmd_GetCanonicalName( IExplorerCommand *iface, GUID *guid ) { *guid = GUID_NULL; return S_OK; }
static HRESULT WINAPI cmd_GetState( IExplorerCommand *iface, IShellItemArray *items, BOOL slow, EXPCMDSTATE *state )
{
    *state = impl( iface )->state;
    return S_OK;
}
static HRESULT WINAPI cmd_Invoke( IExplorerCommand *iface, IShellItemArray *items, IBindCtx *bc )
{
    FILE *f = _wfopen( L"C:\\pkgverb-log.txt", L"a" );
    DWORD count = 0, i;

    if (!f) return E_FAIL;
    fprintf( f, "invoked %ls\n", impl( iface )->title );
    if (items && SUCCEEDED(IShellItemArray_GetCount( items, &count )))
        for (i = 0; i < count; i++)
        {
            IShellItem *item;
            WCHAR *path;
            if (FAILED(IShellItemArray_GetItemAt( items, i, &item ))) continue;
            if (SUCCEEDED(IShellItem_GetDisplayName( item, SIGDN_FILESYSPATH, &path )))
            {
                fprintf( f, "item %ls\n", path );
                CoTaskMemFree( path );
            }
            IShellItem_Release( item );
        }
    fclose( f );
    return S_OK;
}
static HRESULT WINAPI cmd_GetFlags( IExplorerCommand *iface, EXPCMDFLAGS *flags ) { *flags = impl( iface )->flags; return S_OK; }

/* ---- sub-commands ---- */
struct subs { IEnumExplorerCommand IEnumExplorerCommand_iface; LONG ref; ULONG pos; };
static struct subs *subs_impl( IEnumExplorerCommand *iface ) { return CONTAINING_RECORD( iface, struct subs, IEnumExplorerCommand_iface ); }
static HRESULT WINAPI subs_QueryInterface( IEnumExplorerCommand *iface, REFIID iid, void **out )
{
    if (IsEqualIID( iid, &IID_IUnknown ) || IsEqualIID( iid, &IID_IEnumExplorerCommand ))
    {
        *out = iface;
        IEnumExplorerCommand_AddRef( iface );
        return S_OK;
    }
    *out = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI subs_AddRef( IEnumExplorerCommand *iface ) { return InterlockedIncrement( &subs_impl( iface )->ref ); }
static ULONG WINAPI subs_Release( IEnumExplorerCommand *iface )
{
    ULONG ref = InterlockedDecrement( &subs_impl( iface )->ref );
    if (!ref) HeapFree( GetProcessHeap(), 0, subs_impl( iface ) );
    return ref;
}
static HRESULT WINAPI subs_Next( IEnumExplorerCommand *iface, ULONG count, IExplorerCommand **out, ULONG *fetched )
{
    static const WCHAR *const titles[] = { L"Sub A", L"Sub B", L"Sub C" };
    static const EXPCMDSTATE states[] = { ECS_ENABLED, ECS_HIDDEN, ECS_DISABLED };
    struct subs *s = subs_impl( iface );
    ULONG got = 0;

    while (got < count && s->pos < 3)
    {
        command_create( titles[s->pos], states[s->pos], ECF_DEFAULT, &out[got] );
        s->pos++;
        got++;
    }
    if (fetched) *fetched = got;
    return got == count ? S_OK : S_FALSE;
}
static HRESULT WINAPI subs_Skip( IEnumExplorerCommand *iface, ULONG count ) { subs_impl( iface )->pos += count; return S_OK; }
static HRESULT WINAPI subs_Reset( IEnumExplorerCommand *iface ) { subs_impl( iface )->pos = 0; return S_OK; }
static HRESULT WINAPI subs_Clone( IEnumExplorerCommand *iface, IEnumExplorerCommand **out ) { *out = NULL; return E_NOTIMPL; }
static const IEnumExplorerCommandVtbl subs_vtbl = { subs_QueryInterface, subs_AddRef, subs_Release, subs_Next, subs_Skip, subs_Reset, subs_Clone };

static HRESULT WINAPI cmd_EnumSubCommands( IExplorerCommand *iface, IEnumExplorerCommand **out )
{
    struct subs *s;
    if (!(impl( iface )->flags & ECF_HASSUBCOMMANDS)) { *out = NULL; return E_NOTIMPL; }
    s = HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*s) );
    s->IEnumExplorerCommand_iface.lpVtbl = &subs_vtbl;
    s->ref = 1;
    *out = &s->IEnumExplorerCommand_iface;
    return S_OK;
}

static const IExplorerCommandVtbl cmd_vtbl =
{
    cmd_QueryInterface, cmd_AddRef, cmd_Release, cmd_GetTitle, cmd_GetIcon, cmd_GetToolTip, cmd_GetCanonicalName,
    cmd_GetState, cmd_Invoke, cmd_GetFlags, cmd_EnumSubCommands,
};

static HRESULT command_create( const WCHAR *title, EXPCMDSTATE state, EXPCMDFLAGS flags, IExplorerCommand **out )
{
    struct command *c = HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*c) );
    if (!c) return E_OUTOFMEMORY;
    c->IExplorerCommand_iface.lpVtbl = &cmd_vtbl;
    c->ref = 1;
    c->title = title;
    c->state = state;
    c->flags = flags;
    *out = &c->IExplorerCommand_iface;
    return S_OK;
}

/* ---- class factory ---- */
struct factory { IClassFactory IClassFactory_iface; const GUID *clsid; };
static HRESULT WINAPI cf_QueryInterface( IClassFactory *iface, REFIID iid, void **out )
{
    if (IsEqualIID( iid, &IID_IUnknown ) || IsEqualIID( iid, &IID_IClassFactory )) { *out = iface; return S_OK; }
    *out = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI cf_AddRef( IClassFactory *iface ) { return 2; }
static ULONG WINAPI cf_Release( IClassFactory *iface ) { return 1; }
static HRESULT WINAPI cf_CreateInstance( IClassFactory *iface, IUnknown *outer, REFIID iid, void **out )
{
    struct factory *f = CONTAINING_RECORD( iface, struct factory, IClassFactory_iface );
    IExplorerCommand *cmd;
    HRESULT hr;

    if (IsEqualCLSID( f->clsid, &clsid_verb )) hr = command_create( L"SG Package Verb", ECS_ENABLED, ECF_DEFAULT, &cmd );
    else if (IsEqualCLSID( f->clsid, &clsid_menu )) hr = command_create( L"SG Package Menu", ECS_ENABLED, ECF_HASSUBCOMMANDS, &cmd );
    else hr = command_create( L"SG Hidden Verb", ECS_HIDDEN, ECF_DEFAULT, &cmd );
    if (FAILED(hr)) return hr;
    hr = IExplorerCommand_QueryInterface( cmd, iid, out );
    IExplorerCommand_Release( cmd );
    return hr;
}
static HRESULT WINAPI cf_LockServer( IClassFactory *iface, BOOL lock ) { return S_OK; }
static const IClassFactoryVtbl cf_vtbl = { cf_QueryInterface, cf_AddRef, cf_Release, cf_CreateInstance, cf_LockServer };
static struct factory factories[3] = { { {&cf_vtbl}, &clsid_verb }, { {&cf_vtbl}, &clsid_menu }, { {&cf_vtbl}, &clsid_hidden } };

HRESULT WINAPI DllGetClassObject( REFCLSID clsid, REFIID iid, void **out )
{
    int i;
    for (i = 0; i < 3; i++)
        if (IsEqualCLSID( clsid, factories[i].clsid ))
            return IClassFactory_QueryInterface( &factories[i].IClassFactory_iface, iid, out );
    *out = NULL;
    return CLASS_E_CLASSNOTAVAILABLE;
}

HRESULT WINAPI DllCanUnloadNow(void) { return S_FALSE; }
