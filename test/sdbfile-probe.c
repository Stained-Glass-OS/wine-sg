/* apphelp's compatibility database reader and writer (patches/sg/6059): writes C:\sg.sdb (through its
 * NT path) with a DATABASE of a NAME, a DATABASE_ID and two EXEs, the first holding a tag of every type,
 * and reads it back: versions, the tree (each list's exact children), the values, the string table
 * (one item per distinct string), the ID, tag names, GUID strings, SdbInitDatabase. Prints KEY=VALUE. */
#include <windows.h>
#include <stdio.h>

typedef void *PDB;
typedef DWORD TAGID;
typedef WORD TAG;

#define F(ret, name, args) static ret (WINAPI *p##name) args
F(PDB, SdbCreateDatabase, (LPCWSTR, int));
F(PDB, SdbOpenDatabase, (LPCWSTR, int));
F(void, SdbCloseDatabase, (PDB));
F(void, SdbCloseDatabaseWrite, (PDB));
F(TAGID, SdbBeginWriteListTag, (PDB, TAG));
F(BOOL, SdbEndWriteListTag, (PDB, TAGID));
F(BOOL, SdbWriteNULLTag, (PDB, TAG));
F(BOOL, SdbWriteBYTETag, (PDB, TAG, BYTE));
F(BOOL, SdbWriteWORDTag, (PDB, TAG, WORD));
F(BOOL, SdbWriteDWORDTag, (PDB, TAG, DWORD));
F(BOOL, SdbWriteQWORDTag, (PDB, TAG, ULONGLONG));
F(BOOL, SdbWriteStringTag, (PDB, TAG, LPCWSTR));
F(BOOL, SdbWriteBinaryTag, (PDB, TAG, const BYTE *, DWORD));
F(TAGID, SdbGetFirstChild, (PDB, TAGID));
F(TAGID, SdbGetNextChild, (PDB, TAGID, TAGID));
F(TAG, SdbGetTagFromTagID, (PDB, TAGID));
F(TAGID, SdbFindFirstTag, (PDB, TAGID, TAG));
F(TAGID, SdbFindNextTag, (PDB, TAGID, TAGID));
F(BYTE, SdbReadBYTETag, (PDB, TAGID, BYTE));
F(WORD, SdbReadWORDTag, (PDB, TAGID, WORD));
F(DWORD, SdbReadDWORDTag, (PDB, TAGID, DWORD));
F(ULONGLONG, SdbReadQWORDTag, (PDB, TAGID, ULONGLONG));
F(BOOL, SdbReadStringTag, (PDB, TAGID, LPWSTR, DWORD));
F(WCHAR *, SdbGetStringTagPtr, (PDB, TAGID));
F(BOOL, SdbReadBinaryTag, (PDB, TAGID, BYTE *, DWORD));
F(DWORD, SdbGetTagDataSize, (PDB, TAGID));
F(BOOL, SdbGetDatabaseVersion, (LPCWSTR, DWORD *, DWORD *));
F(BOOL, SdbGetDatabaseID, (PDB, GUID *));
F(LPCWSTR, SdbTagToString, (TAG));
F(BOOL, SdbGUIDToString, (const GUID *, LPWSTR, DWORD));
F(BOOL, SdbGUIDFromString, (LPCWSTR, GUID *));
F(BOOL, SdbIsNullGUID, (const GUID *));
F(void *, SdbInitDatabase, (DWORD, LPCWSTR));
F(void, SdbReleaseDatabase, (void *));

static const GUID db_guid = { 0x5a4e3d2c, 0x1b0a, 0x4f9e, { 0x8d, 0x7c, 0x6b, 0x5a, 0x49, 0x38, 0x27, 0x16 } };
static const GUID null_guid;
static const BYTE blob[5] = { 1, 2, 3, 4, 5 };

static int count_children( PDB db, TAGID parent )
{
    TAGID id;
    int n = 0;
    for (id = pSdbGetFirstChild( db, parent ); id; id = pSdbGetNextChild( db, parent, id )) n++;
    return n;
}

int main( int argc, char **argv )
{
    HMODULE m = LoadLibraryA( "apphelp.dll" );
    WCHAR buf[64];
    GUID guid;
    DWORD major = 0, minor = 0;
    TAGID database, exe, exe2, id, table;
    PDB db;
    int n;

#define L(name) if (!(p##name = (void *)GetProcAddress( m, #name ))) { printf( "missing=%s\n", #name ); return 1; }
    L(SdbCreateDatabase) L(SdbOpenDatabase) L(SdbCloseDatabase) L(SdbCloseDatabaseWrite) L(SdbBeginWriteListTag)
    L(SdbEndWriteListTag) L(SdbWriteNULLTag) L(SdbWriteBYTETag) L(SdbWriteWORDTag) L(SdbWriteDWORDTag)
    L(SdbWriteQWORDTag) L(SdbWriteStringTag) L(SdbWriteBinaryTag) L(SdbGetFirstChild) L(SdbGetNextChild)
    L(SdbGetTagFromTagID) L(SdbFindFirstTag) L(SdbFindNextTag) L(SdbReadBYTETag) L(SdbReadWORDTag)
    L(SdbReadDWORDTag) L(SdbReadQWORDTag) L(SdbReadStringTag) L(SdbGetStringTagPtr) L(SdbReadBinaryTag)
    L(SdbGetTagDataSize) L(SdbGetDatabaseVersion) L(SdbGetDatabaseID) L(SdbTagToString) L(SdbGUIDToString)
    L(SdbGUIDFromString) L(SdbIsNullGUID) L(SdbInitDatabase) L(SdbReleaseDatabase)

    /* writing */
    db = pSdbCreateDatabase( L"\\??\\C:\\sg.sdb", 1 /* NT_PATH */ );
    printf( "create=%d\n", db != NULL );
    if (!db) return 1;
    database = pSdbBeginWriteListTag( db, 0x7001 /* DATABASE */ );
    pSdbWriteStringTag( db, 0x6001 /* NAME */, L"SG Test Fixes" );
    pSdbWriteBinaryTag( db, 0x9007 /* DATABASE_ID */, (const BYTE *)&db_guid, sizeof(db_guid) );
    exe = pSdbBeginWriteListTag( db, 0x7007 /* EXE */ );
    pSdbWriteStringTag( db, 0x6001, L"app.exe" );
    pSdbWriteStringTag( db, 0x6005 /* VENDOR */, L"SG Test Fixes" );     /* the same string again */
    pSdbWriteNULLTag( db, 0x1002 /* GENERAL */ );
    pSdbWriteBYTETag( db, 0x2001, 0xab );
    pSdbWriteWORDTag( db, 0x3001 /* MATCH_MODE */, 0x1234 );
    pSdbWriteDWORDTag( db, 0x4017 /* FLAGS */, 0xdeadbeef );
    pSdbWriteQWORDTag( db, 0x5001 /* TIME */, 0x0123456789abcdefull );
    pSdbWriteStringTag( db, 0x8801, L"direct" );                          /* a STRING tag: in place */
    pSdbWriteBinaryTag( db, 0x9004 /* EXE_ID */, blob, sizeof(blob) );   /* odd size: padded */
    printf( "wrongtype=%d\n", pSdbWriteDWORDTag( db, 0x6001, 1 ) );
    pSdbEndWriteListTag( db, exe );
    exe2 = pSdbBeginWriteListTag( db, 0x7007 );
    pSdbWriteStringTag( db, 0x6001, L"other.exe" );
    pSdbEndWriteListTag( db, exe2 );
    pSdbEndWriteListTag( db, database );
    pSdbCloseDatabaseWrite( db );

    n = pSdbGetDatabaseVersion( L"C:\\sg.sdb", &major, &minor );
    printf( "version=%d %lu.%lu\n", n, major, minor );

    /* reading */
    db = pSdbOpenDatabase( L"C:\\sg.sdb", 0 /* DOS_PATH */ );
    printf( "open=%d\n", db != NULL );
    if (!db) return 1;
    printf( "toplevel=%d\n", count_children( db, 0 ) );
    database = pSdbFindFirstTag( db, 0, 0x7001 );
    table = pSdbFindFirstTag( db, 0, 0x7801 );
    printf( "database=%d %d\n", database != 0, count_children( db, database ) );
    exe = pSdbFindFirstTag( db, database, 0x7007 );
    exe2 = pSdbFindNextTag( db, database, exe );
    printf( "exes=%d %d %d %d\n", exe != 0, exe2 != 0, pSdbFindNextTag( db, database, exe2 ) == 0,
            exe ? count_children( db, exe ) : -1 );
    printf( "exe2children=%d\n", exe2 ? count_children( db, exe2 ) : -1 );
    id = pSdbFindFirstTag( db, exe, 0x6001 );
    printf( "name=%ls\n", id ? pSdbGetStringTagPtr( db, id ) : L"(none)" );
    id = pSdbFindFirstTag( db, exe, 0x6005 );
    printf( "vendor=%ls\n", id ? pSdbGetStringTagPtr( db, id ) : L"(none)" );
    id = pSdbFindFirstTag( db, exe2, 0x6001 );
    printf( "name2=%ls\n", id ? pSdbGetStringTagPtr( db, id ) : L"(none)" );
    n = pSdbReadStringTag( db, id, buf, 4 );
    printf( "readstring=%d %d %ls\n", n, pSdbReadStringTag( db, id, buf, ARRAYSIZE(buf) ), buf );
    printf( "null=%d\n", pSdbFindFirstTag( db, exe, 0x1002 ) != 0 && !pSdbGetTagDataSize( db, pSdbFindFirstTag( db, exe, 0x1002 ) ) );
    printf( "byte=%#x\n", pSdbReadBYTETag( db, pSdbFindFirstTag( db, exe, 0x2001 ), 0 ) );
    printf( "word=%#x\n", pSdbReadWORDTag( db, pSdbFindFirstTag( db, exe, 0x3001 ), 0 ) );
    printf( "dword=%#lx\n", pSdbReadDWORDTag( db, pSdbFindFirstTag( db, exe, 0x4017 ), 0 ) );
    printf( "qword=%#llx\n", pSdbReadQWORDTag( db, pSdbFindFirstTag( db, exe, 0x5001 ), 0 ) );
    printf( "default=%#lx\n", pSdbReadDWORDTag( db, pSdbFindFirstTag( db, exe, 0x3001 ), 7 ) );
    id = pSdbFindFirstTag( db, exe, 0x8801 );
    printf( "direct=%ls\n", id ? pSdbGetStringTagPtr( db, id ) : L"(none)" );
    id = pSdbFindFirstTag( db, exe, 0x9004 );
    {
        BYTE out[8] = {0};
        DWORD size = pSdbGetTagDataSize( db, id );
        BOOL small = pSdbReadBinaryTag( db, id, out, 4 ), big = pSdbReadBinaryTag( db, id, out, sizeof(out) );
        printf( "binary=%lu %d %d %d\n", size, small, big, !memcmp( out, blob, sizeof(blob) ) );
    }
    printf( "stringtable=%d %d\n", table != 0, table ? count_children( db, table ) : -1 );
    printf( "tagname=%ls %ls %ls\n", pSdbTagToString( 0x7007 ), pSdbTagToString( 0x9007 ), pSdbTagToString( 0x7fff ) );
    memset( &guid, 0, sizeof(guid) );
    n = pSdbGetDatabaseID( db, &guid );
    printf( "dbid=%d %d\n", n, IsEqualGUID( &guid, &db_guid ) );
    pSdbCloseDatabase( db );

    pSdbGUIDToString( &db_guid, buf, ARRAYSIZE(buf) );
    printf( "guidstring=%ls\n", buf );
    memset( &guid, 0, sizeof(guid) );
    n = pSdbGUIDFromString( buf, &guid );
    printf( "guidparse=%d %d %d %d\n", n, IsEqualGUID( &guid, &db_guid ),
            pSdbIsNullGUID( &guid ), pSdbIsNullGUID( &null_guid ) );

    /* not a database */
    {
        HANDLE f = CreateFileW( L"C:\\bad.sdb", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL );
        DWORD w;
        WriteFile( f, "this is not a database", 22, &w, NULL );
        CloseHandle( f );
        printf( "bad=%d\n", pSdbOpenDatabase( L"C:\\bad.sdb", 0 ) == NULL );
    }

    {
        void *h1 = pSdbInitDatabase( 3 /* HID_DOS_PATHS | HID_DATABASE_FULLPATH */, L"C:\\sg.sdb" );
        void *h2 = pSdbInitDatabase( 4 /* HID_NO_DATABASE */, NULL );
        void *h3 = pSdbInitDatabase( 3, L"C:\\bad.sdb" );
        printf( "init=%d %d %d\n", h1 != NULL, h2 != NULL, h3 == NULL );
        pSdbReleaseDatabase( h1 );
        pSdbReleaseDatabase( h2 );
    }
    printf( "done=1\n" );
    return 0;
}
