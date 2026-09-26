/* Transforms made on this system (patches/sg/0402): builds a small
 * registry-only package, a changed copy of it and the transform between
 * them (MsiDatabaseGenerateTransform, MsiCreateTransformSummaryInfo), and
 * applies that to a third copy (MsiDatabaseApplyTransform) to compare.
 *   msitransform-probe DIR   -> DIR\base.msi, DIR\corp.mst and a report */
#include <windows.h>
#include <msi.h>
#include <msiquery.h>
#include <stdio.h>

static WCHAR dir[MAX_PATH];

static UINT run( MSIHANDLE db, const WCHAR *sql )
{
    MSIHANDLE v;
    UINT r = MsiDatabaseOpenViewW( db, sql, &v );
    if (r) { printf( "sql %u: %ls\n", r, sql ); return r; }
    r = MsiViewExecute( v, 0 );
    if (r) printf( "exec %u: %ls\n", r, sql );
    MsiCloseHandle( v );
    return r;
}

static void path( WCHAR *out, const WCHAR *name )
{
    swprintf( out, MAX_PATH, L"%ls\\%ls", dir, name );
}

static const WCHAR *tables[] = {
    L"CREATE TABLE `Property` (`Property` CHAR(72) NOT NULL, `Value` LONGCHAR NOT NULL LOCALIZABLE PRIMARY KEY `Property`)",
    L"CREATE TABLE `Directory` (`Directory` CHAR(72) NOT NULL, `Directory_Parent` CHAR(72), `DefaultDir` CHAR(255) NOT NULL LOCALIZABLE PRIMARY KEY `Directory`)",
    L"CREATE TABLE `Component` (`Component` CHAR(72) NOT NULL, `ComponentId` CHAR(38), `Directory_` CHAR(72) NOT NULL, `Attributes` SHORT NOT NULL, `Condition` CHAR(255), `KeyPath` CHAR(72) PRIMARY KEY `Component`)",
    L"CREATE TABLE `Feature` (`Feature` CHAR(38) NOT NULL, `Feature_Parent` CHAR(38), `Title` CHAR(64) LOCALIZABLE, `Description` CHAR(255) LOCALIZABLE, `Display` SHORT, `Level` SHORT NOT NULL, `Directory_` CHAR(72), `Attributes` SHORT NOT NULL PRIMARY KEY `Feature`)",
    L"CREATE TABLE `FeatureComponents` (`Feature_` CHAR(38) NOT NULL, `Component_` CHAR(72) NOT NULL PRIMARY KEY `Feature_`, `Component_`)",
    L"CREATE TABLE `Registry` (`Registry` CHAR(72) NOT NULL, `Root` SHORT NOT NULL, `Key` CHAR(255) NOT NULL LOCALIZABLE, `Name` CHAR(255) LOCALIZABLE, `Value` CHAR(0) LOCALIZABLE, `Component_` CHAR(72) NOT NULL PRIMARY KEY `Registry`)",
    L"CREATE TABLE `InstallExecuteSequence` (`Action` CHAR(72) NOT NULL, `Condition` CHAR(255), `Sequence` SHORT PRIMARY KEY `Action`)",
};

static const WCHAR *rows[] = {
    L"INSERT INTO `Property` (`Property`, `Value`) VALUES ('ProductCode', '{7C2B3E41-5A0B-4C8E-9E6D-3A1F0B2C4D51}')",
    L"INSERT INTO `Property` (`Property`, `Value`) VALUES ('UpgradeCode', '{0F6A0E73-6B58-4D2A-8C1B-5E9C7A3B2D62}')",
    L"INSERT INTO `Property` (`Property`, `Value`) VALUES ('ProductName', 'SG transform test')",
    L"INSERT INTO `Property` (`Property`, `Value`) VALUES ('ProductVersion', '1.0.0')",
    L"INSERT INTO `Property` (`Property`, `Value`) VALUES ('Manufacturer', 'Stained Glass tests')",
    L"INSERT INTO `Property` (`Property`, `Value`) VALUES ('ProductLanguage', '1033')",
    L"INSERT INTO `Property` (`Property`, `Value`) VALUES ('ALLUSERS', '1')",
    L"INSERT INTO `Directory` (`Directory`, `Directory_Parent`, `DefaultDir`) VALUES ('TARGETDIR', '', 'SourceDir')",
    L"INSERT INTO `Component` (`Component`, `ComponentId`, `Directory_`, `Attributes`, `KeyPath`) VALUES ('Settings', '{3E8D5C2A-1B7F-4A69-B0C3-9D2E4F6A8B71}', 'TARGETDIR', 260, 'regSite')",
    L"INSERT INTO `Feature` (`Feature`, `Title`, `Level`, `Attributes`) VALUES ('Main', 'Main', 1, 0)",
    L"INSERT INTO `FeatureComponents` (`Feature_`, `Component_`) VALUES ('Main', 'Settings')",
    L"INSERT INTO `Registry` (`Registry`, `Root`, `Key`, `Name`, `Value`, `Component_`) VALUES ('regSite', 2, 'Software\\SGTEST\\TransformTest', 'Site', 'default', 'Settings')",
    L"INSERT INTO `Registry` (`Registry`, `Root`, `Key`, `Name`, `Value`, `Component_`) VALUES ('regGone', 2, 'Software\\SGTEST\\TransformTest', 'Removed', 'yes', 'Settings')",
    L"INSERT INTO `InstallExecuteSequence` (`Action`, `Sequence`) VALUES ('CostInitialize', 800)",
    L"INSERT INTO `InstallExecuteSequence` (`Action`, `Sequence`) VALUES ('FileCost', 900)",
    L"INSERT INTO `InstallExecuteSequence` (`Action`, `Sequence`) VALUES ('CostFinalize', 1000)",
    L"INSERT INTO `InstallExecuteSequence` (`Action`, `Sequence`) VALUES ('InstallValidate', 1400)",
    L"INSERT INTO `InstallExecuteSequence` (`Action`, `Sequence`) VALUES ('InstallInitialize', 1500)",
    L"INSERT INTO `InstallExecuteSequence` (`Action`, `Sequence`) VALUES ('ProcessComponents', 1600)",
    L"INSERT INTO `InstallExecuteSequence` (`Action`, `Sequence`) VALUES ('WriteRegistryValues', 5000)",
    L"INSERT INTO `InstallExecuteSequence` (`Action`, `Sequence`) VALUES ('RegisterProduct', 6100)",
    L"INSERT INTO `InstallExecuteSequence` (`Action`, `Sequence`) VALUES ('PublishFeatures', 6300)",
    L"INSERT INTO `InstallExecuteSequence` (`Action`, `Sequence`) VALUES ('PublishProduct', 6400)",
    L"INSERT INTO `InstallExecuteSequence` (`Action`, `Sequence`) VALUES ('InstallFinalize', 6600)",
};

static UINT build_base( const WCHAR *file )
{
    MSIHANDLE db, si;
    UINT r, i;

    DeleteFileW( file );
    if ((r = MsiOpenDatabaseW( file, MSIDBOPEN_CREATE, &db ))) return r;
    for (i = 0; i < ARRAYSIZE(tables); i++) if ((r = run( db, tables[i] ))) return r;
    for (i = 0; i < ARRAYSIZE(rows); i++) if ((r = run( db, rows[i] ))) return r;
    if ((r = MsiGetSummaryInformationW( db, NULL, 10, &si ))) return r;
    MsiSummaryInfoSetPropertyW( si, 7 /* template */, VT_LPSTR, 0, NULL, L"x64;1033" );
    MsiSummaryInfoSetPropertyW( si, 9 /* revision */, VT_LPSTR, 0, NULL, L"{9A4C1E27-3D5B-4F80-A6C2-7B1E0D3F5A93}" );
    MsiSummaryInfoSetPropertyW( si, 14 /* pages */, VT_I4, 200, NULL, NULL );
    MsiSummaryInfoSetPropertyW( si, 15 /* words */, VT_I4, 2, NULL, NULL );
    MsiSummaryInfoPersist( si );
    MsiCloseHandle( si );
    r = MsiDatabaseCommit( db );
    MsiCloseHandle( db );
    return r;
}

static void query( const WCHAR *file, const WCHAR *sql, WCHAR *out, DWORD size )
{
    MSIHANDLE db, v, rec;
    out[0] = 0;
    if (MsiOpenDatabaseW( file, MSIDBOPEN_READONLY, &db )) return;
    if (!MsiDatabaseOpenViewW( db, sql, &v ))
    {
        if (!MsiViewExecute( v, 0 ) && !MsiViewFetch( v, &rec ))
        {
            MsiRecordGetStringW( rec, 1, out, &size );
            MsiCloseHandle( rec );
        }
        else wcscpy( out, L"(no row)" );
        MsiCloseHandle( v );
    }
    MsiCloseHandle( db );
}

int wmain( int argc, WCHAR **argv )
{
    WCHAR base[MAX_PATH], corp[MAX_PATH], mst[MAX_PATH], check[MAX_PATH], val[256];
    MSIHANDLE db, ref, chk;
    UINT r;

    wcscpy( dir, argc > 1 ? argv[1] : L"C:\\" );
    path( base, L"base.msi" ); path( corp, L"corp.msi" ); path( mst, L"corp.mst" ); path( check, L"check.msi" );
    printf( "base=%u\n", build_base( base ) );

    /* the customisation: a changed value, a new row, a removed row, a new table */
    CopyFileW( base, corp, FALSE );
    MsiOpenDatabaseW( corp, MSIDBOPEN_TRANSACT, &db );
    run( db, L"UPDATE `Registry` SET `Value` = 'corp.sgtest.lan' WHERE `Registry` = 'regSite'" );
    run( db, L"INSERT INTO `Registry` (`Registry`, `Root`, `Key`, `Name`, `Value`, `Component_`) VALUES ('regMode', 2, 'Software\\SGTEST\\TransformTest', 'Mode', '#42', 'Settings')" );
    run( db, L"DELETE FROM `Registry` WHERE `Registry` = 'regGone'" );
    run( db, L"CREATE TABLE `SgNotes` (`Id` SHORT NOT NULL, `Text` CHAR(64) PRIMARY KEY `Id`)" );
    run( db, L"INSERT INTO `SgNotes` (`Id`, `Text`) VALUES (7, 'from the transform')" );
    MsiDatabaseCommit( db );
    MsiOpenDatabaseW( base, MSIDBOPEN_READONLY, &ref );

    printf( "same=%u\n", MsiDatabaseGenerateTransformW( ref, ref, NULL, 0, 0 ) );
    printf( "differ=%u\n", MsiDatabaseGenerateTransformW( db, ref, NULL, 0, 0 ) );
    DeleteFileW( mst );
    printf( "generate=%u\n", r = MsiDatabaseGenerateTransformW( db, ref, mst, 0, 0 ) );
    printf( "summary=%u\n", MsiCreateTransformSummaryInfoW( db, ref, mst, 0, MSITRANSFORM_VALIDATE_PRODUCT ) );
    MsiCloseHandle( db );
    MsiCloseHandle( ref );

    /* the transform applied to another copy gives the customised package */
    CopyFileW( base, check, FALSE );
    MsiOpenDatabaseW( check, MSIDBOPEN_TRANSACT, &chk );
    printf( "apply=%u\n", MsiDatabaseApplyTransformW( chk, mst, 0 ) );
    MsiDatabaseCommit( chk );
    MsiCloseHandle( chk );
    query( check, L"SELECT `Value` FROM `Registry` WHERE `Registry` = 'regSite'", val, 256 );
    printf( "site=%ls\n", val );
    query( check, L"SELECT `Value` FROM `Registry` WHERE `Registry` = 'regMode'", val, 256 );
    printf( "mode=%ls\n", val );
    query( check, L"SELECT `Value` FROM `Registry` WHERE `Registry` = 'regGone'", val, 256 );
    printf( "gone=%ls\n", val );
    query( check, L"SELECT `Text` FROM `SgNotes` WHERE `Id` = 7", val, 256 );
    printf( "newtable=%ls\n", val );
    return 0;
}
