/* opcservices, fast mode (patch 3545): package round trip through ReadPackageFromStream, part and relationship deletion,
   relationship enumeration by type, relationships content stream, URI comparison, stream methods */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <ole2.h>
#include <stdio.h>
#include <msopc.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

int main(void)
{
    IOpcFactory *factory;
    IOpcPackage *package, *package2;
    IOpcPartSet *parts, *parts2;
    IOpcRelationshipSet *rels, *rels2;
    IOpcPartUri *uri, *uri2, *uri3;
    IOpcPart *part, *part2;
    IOpcRelationship *rel;
    IStream *content, *mem;
    IUri *target;
    HRESULT hr;
    BOOL exists;
    ULONG written, read;
    char buf[64];
    LARGE_INTEGER zero = { 0 };

    CoInitialize(NULL);
    hr = CoCreateInstance(&CLSID_OpcFactory, NULL, CLSCTX_INPROC_SERVER, &IID_IOpcFactory, (void **)&factory);
    if (FAILED(hr)) { printf("FAIL  factory %#lx\nRESULT: FAIL\n", hr); return 1; }

    hr = IOpcFactory_CreatePackage(factory, &package);
    check(hr == S_OK, "CreatePackage");
    IOpcPackage_GetPartSet(package, &parts);

    IOpcFactory_CreatePartUri(factory, L"/docs/readme.txt", &uri);
    hr = IOpcPartSet_CreatePart(parts, uri, L"text/plain", OPC_COMPRESSION_NORMAL, &part);
    check(hr == S_OK, "create a part");
    IOpcPart_GetContentStream(part, &content);
    IStream_Write(content, "hello opc", 9, &written);
    IStream_Release(content);

    /* stream semantics of part content */
    IOpcPart_GetContentStream(part, &content);
    {
        ULARGE_INTEGER size;
        STATSTG st;
        IStream *clone, *dst;
        ULARGE_INTEGER n, w;

        IStream_Stat(content, &st, STATFLAG_NONAME);
        check(st.cbSize.QuadPart == 9 && st.type == STGTY_STREAM, "Stat of a content stream");
        size.QuadPart = 4;
        check(IStream_SetSize(content, size) == S_OK, "SetSize shrinks");
        IStream_Stat(content, &st, STATFLAG_NONAME);
        check(st.cbSize.QuadPart == 4, "size 4");
        size.QuadPart = 12;
        IStream_SetSize(content, size);
        IStream_Seek(content, zero, STREAM_SEEK_SET, NULL);
        memset(buf, 1, sizeof(buf));
        IStream_Read(content, buf, 12, &read);
        check(read == 12 && !memcmp(buf, "hell", 4) && buf[4] == 0 && buf[11] == 0, "growing zero fills");
        IStream_Seek(content, zero, STREAM_SEEK_SET, NULL);
        IStream_Write(content, "hello opc", 9, &written);
        check(IStream_Clone(content, &clone) == S_OK, "Clone");
        IStream_Seek(clone, zero, STREAM_SEEK_SET, NULL);
        IStream_Read(clone, buf, 5, &read);
        check(read == 5 && !memcmp(buf, "hello", 5), "clone shares the data");
        IStream_Release(clone);
        CreateStreamOnHGlobal(NULL, TRUE, &dst);
        IStream_Seek(content, zero, STREAM_SEEK_SET, NULL);
        n.QuadPart = 5;
        hr = IStream_CopyTo(content, dst, n, &n, &w);
        check(hr == S_OK && n.QuadPart == 5 && w.QuadPart == 5, "CopyTo");
        IStream_Release(dst);
        check(IStream_Commit(content, 0) == S_OK, "Commit");
        check(IStream_LockRegion(content, n, n, LOCK_WRITE) == STG_E_INVALIDFUNCTION, "LockRegion");
        size.QuadPart = 9;
        IStream_SetSize(content, size);
        IStream_Seek(content, zero, STREAM_SEEK_END, NULL);
        IStream_Write(content, "!", 1, &written);
        IStream_Seek(content, zero, STREAM_SEEK_SET, NULL);
        IStream_Read(content, buf, 20, &read);
        check(read == 10 && !memcmp(buf, "hello opc!", 10), "append");
        {
            LARGE_INTEGER pos = { { 3, 0 } };
            IStream_Seek(content, pos, STREAM_SEEK_SET, NULL);
            IStream_Write(content, "LO", 2, &written);
            IStream_Seek(content, zero, STREAM_SEEK_SET, NULL);
            IStream_Read(content, buf, 20, &read);
            check(read == 10 && !memcmp(buf, "helLO opc!", 10), "overwrite in the middle keeps the size");
        }
        IStream_Seek(content, zero, STREAM_SEEK_SET, NULL);
        IStream_Write(content, "hello opc", 9, &written);
        size.QuadPart = 9;
        IStream_SetSize(content, size);
    }
    IStream_Release(content);

    /* relationships */
    IOpcPart_GetRelationshipSet(part, &rels);
    IOpcFactory_CreatePartUri(factory, L"/docs/other.txt", &uri2);
    CreateUri(L"other.txt", Uri_CREATE_ALLOW_RELATIVE, 0, &target);
    hr = IOpcRelationshipSet_CreateRelationship(rels, L"rId1", L"http://example.com/rel/a", target, OPC_URI_TARGET_MODE_INTERNAL, &rel);
    check(hr == S_OK, "relationship rId1");
    IOpcRelationship_Release(rel);
    IUri_Release(target);
    CreateUri(L"http://example.com/x", 0, 0, &target);
    hr = IOpcRelationshipSet_CreateRelationship(rels, L"rId2", L"http://example.com/rel/b", target, OPC_URI_TARGET_MODE_EXTERNAL, &rel);
    IOpcRelationship_Release(rel);
    hr = IOpcRelationshipSet_CreateRelationship(rels, L"rId3", L"http://example.com/rel/a", target, OPC_URI_TARGET_MODE_EXTERNAL, &rel);
    IOpcRelationship_Release(rel);
    IUri_Release(target);
    {
        IOpcRelationshipEnumerator *en;
        BOOL has;
        int n = 0;
        WCHAR *id;
        WCHAR ids[4][8] = { { 0 } };

        hr = IOpcRelationshipSet_GetEnumeratorForType(rels, L"http://example.com/rel/a", &en);
        check(hr == S_OK, "GetEnumeratorForType");
        IOpcRelationshipEnumerator_MoveNext(en, &has);
        while (has && n < 4)
        {
            IOpcRelationshipEnumerator_GetCurrent(en, &rel);
            IOpcRelationship_GetId(rel, &id);
            lstrcpyW(ids[n++], id);
            CoTaskMemFree(id);
            IOpcRelationship_Release(rel);
            IOpcRelationshipEnumerator_MoveNext(en, &has);
        }
        check(n == 2 && !wcscmp(ids[0], L"rId1") && !wcscmp(ids[1], L"rId3"), "the enumerator only returns the matching relationships");
        IOpcRelationshipEnumerator_Release(en);
        hr = IOpcRelationshipSet_GetEnumeratorForType(rels, L"http://example.com/none", &en);
        IOpcRelationshipEnumerator_MoveNext(en, &has);
        check(!has, "no relationship of a type");
        IOpcRelationshipEnumerator_Release(en);
    }
    {
        IStream *rs;
        STATSTG st;

        hr = IOpcRelationshipSet_GetRelationshipsContentStream(rels, &rs);
        check(hr == S_OK && rs, "GetRelationshipsContentStream");
        IStream_Stat(rs, &st, STATFLAG_NONAME);
        memset(buf, 0, sizeof(buf));
        IStream_Read(rs, buf, 40, &read);
        check(st.cbSize.QuadPart > 100 && strstr(buf, "<?xml"), "XML document");
        IStream_Release(rs);
    }
    hr = IOpcRelationshipSet_DeleteRelationship(rels, L"rId3");
    check(hr == S_OK, "DeleteRelationship");
    IOpcRelationshipSet_RelationshipExists(rels, L"rId3", &exists);
    check(!exists, "rId3 is gone");
    check(IOpcRelationshipSet_DeleteRelationship(rels, L"rId3") == OPC_E_NO_SUCH_RELATIONSHIP, "delete twice");
    IOpcRelationshipSet_RelationshipExists(rels, L"rId1", &exists);
    check(exists, "rId1 stays");

    /* a second part and a package level relationship */
    hr = IOpcPartSet_CreatePart(parts, uri2, L"application/xml", OPC_COMPRESSION_NONE, &part2);
    IOpcPart_GetContentStream(part2, &content);
    IStream_Write(content, "<a/>", 4, &written);
    IStream_Release(content);
    IOpcPackage_GetRelationshipSet(package, &rels2);
    CreateUri(L"docs/readme.txt", Uri_CREATE_ALLOW_RELATIVE, 0, &target);
    IOpcRelationshipSet_CreateRelationship(rels2, L"rIdRoot", L"http://example.com/rel/root", target, OPC_URI_TARGET_MODE_INTERNAL, &rel);
    IOpcRelationship_Release(rel);
    IUri_Release(target);

    /* write, read back */
    CreateStreamOnHGlobal(NULL, TRUE, &mem);
    hr = IOpcFactory_WritePackageToStream(factory, package, OPC_WRITE_DEFAULT, mem);
    check(hr == S_OK, "WritePackageToStream");
    IStream_Seek(mem, zero, STREAM_SEEK_SET, NULL);
    hr = IOpcFactory_ReadPackageFromStream(factory, mem, OPC_READ_DEFAULT, &package2);
    check(hr == S_OK && package2, "ReadPackageFromStream");
    if (package2)
    {
        WCHAR *type;
        OPC_COMPRESSION_OPTIONS opt;

        IOpcPackage_GetPartSet(package2, &parts2);
        hr = IOpcPartSet_GetPart(parts2, uri, &part);
        check(hr == S_OK, "readme.txt was read");
        IOpcPart_GetContentType(part, &type);
        check(!wcscmp(type, L"text/plain"), "content type of the part");
        CoTaskMemFree(type);
        IOpcPart_GetCompressionOptions(part, &opt);
        check(opt == OPC_COMPRESSION_NORMAL, "deflated part");
        IOpcPart_GetContentStream(part, &content);
        memset(buf, 0, sizeof(buf));
        IStream_Read(content, buf, 20, &read);
        check(read == 9 && !memcmp(buf, "hello opc", 9), "content of the part");
        IStream_Release(content);
        IOpcPart_GetRelationshipSet(part, &rels);
        IOpcRelationshipSet_RelationshipExists(rels, L"rId1", &exists);
        check(exists, "part relationship rId1");
        IOpcRelationshipSet_GetRelationship(rels, L"rId2", &rel);
        {
            OPC_URI_TARGET_MODE mode = OPC_URI_TARGET_MODE_INTERNAL;
            WCHAR *tp;
            IOpcRelationship_GetTargetMode(rel, &mode);
            check(mode == OPC_URI_TARGET_MODE_EXTERNAL, "target mode External survives");
            IOpcRelationship_GetRelationshipType(rel, &tp);
            check(!wcscmp(tp, L"http://example.com/rel/b"), "relationship type");
            CoTaskMemFree(tp);
        }
        IOpcRelationship_Release(rel);
        IOpcRelationshipSet_RelationshipExists(rels, L"rId3", &exists);
        check(!exists, "deleted relationship is not written");
        IOpcRelationshipSet_Release(rels);
        IOpcPart_Release(part);
        hr = IOpcPartSet_GetPart(parts2, uri2, &part);
        check(hr == S_OK, "other.txt was read");
        IOpcPart_GetCompressionOptions(part, &opt);
        IOpcPart_Release(part);
        IOpcPackage_GetRelationshipSet(package2, &rels);
        IOpcRelationshipSet_RelationshipExists(rels, L"rIdRoot", &exists);
        check(exists, "package relationship");
        IOpcRelationshipSet_Release(rels);

        /* delete a part */
        hr = IOpcPartSet_DeletePart(parts2, uri2);
        check(hr == S_OK, "DeletePart");
        IOpcPartSet_PartExists(parts2, uri2, &exists);
        check(!exists, "part is gone");
        check(IOpcPartSet_DeletePart(parts2, uri2) == OPC_E_NO_SUCH_PART, "delete part twice");
        IOpcPartSet_PartExists(parts2, uri, &exists);
        check(exists, "other part stays");
        IOpcPartSet_Release(parts2);
        IOpcPackage_Release(package2);
    }

    /* garbage is refused */
    {
        IStream *junk;
        IOpcPackage *bad = NULL;
        CreateStreamOnHGlobal(NULL, TRUE, &junk);
        IStream_Write(junk, "this is not a zip file at all", 29, &written);
        IStream_Seek(junk, zero, STREAM_SEEK_SET, NULL);
        hr = IOpcFactory_ReadPackageFromStream(factory, junk, OPC_READ_DEFAULT, &bad);
        check(FAILED(hr) && !bad, "not a zip file");
        IStream_Release(junk);
        check(IOpcFactory_ReadPackageFromStream(factory, NULL, OPC_READ_DEFAULT, &bad) == E_POINTER, "no stream");
    }
    IStream_Release(mem);

    /* part URIs */
    {
        INT32 cmp = 5;
        IUri *rel_uri;
        BSTR raw;

        IOpcFactory_CreatePartUri(factory, L"/Docs/README.TXT", &uri3);
        hr = IOpcPartUri_ComparePartUri(uri, uri3, &cmp);
        check(hr == S_OK && cmp == 0, "case insensitive part name compare");
        hr = IOpcPartUri_ComparePartUri(uri, uri2, &cmp);
        check(hr == S_OK && cmp > 0 && cmp <= 1, "readme.txt sorts after other.txt");
        hr = IOpcPartUri_ComparePartUri(uri2, uri, &cmp);
        check(hr == S_OK && cmp < 0 && cmp >= -1, "other.txt sorts before readme.txt");
        IOpcPartUri_Release(uri3);
        hr = IOpcPartUri_GetRelativeUri(uri, uri2, &rel_uri);
        check(hr == S_OK, "GetRelativeUri");
        IUri_GetRawUri(rel_uri, &raw);
        check(!wcscmp(raw, L"other.txt"), "relative reference to a sibling");
        SysFreeString(raw);
        IUri_Release(rel_uri);
    }

    /* file stream */
    {
        IStream *fs, *fs2;
        WCHAR path[MAX_PATH];
        ULARGE_INTEGER sz;
        STATSTG st;

        GetTempPathW(MAX_PATH, path);
        lstrcatW(path, L"fastb-opc.bin");
        hr = IOpcFactory_CreateStreamOnFile(factory, path, OPC_STREAM_IO_WRITE, NULL, 0, &fs);
        check(hr == S_OK, "file stream");
        IStream_Write(fs, "0123456789", 10, &written);
        sz.QuadPart = 4;
        check(IStream_SetSize(fs, sz) == S_OK, "file SetSize");
        IStream_Stat(fs, &st, STATFLAG_NONAME);
        check(st.cbSize.QuadPart == 4, "file size shrunk");
        sz.QuadPart = 3;
        check(IStream_LockRegion(fs, sz, sz, LOCK_WRITE) == S_OK, "file LockRegion");
        check(IStream_UnlockRegion(fs, sz, sz, LOCK_WRITE) == S_OK, "file UnlockRegion");
        check(IStream_Commit(fs, 0) == S_OK, "file Commit");
        check(IStream_Revert(fs) == E_NOTIMPL, "file Revert");
        fs2 = (IStream *)1;
        check(IStream_Clone(fs, &fs2) == E_NOTIMPL && !fs2, "file Clone");
        IStream_Release(fs);
        DeleteFileW(path);
    }

    /* the class factory */
    {
        HRESULT (WINAPI *get)(REFCLSID, REFIID, void **) = (void *)GetProcAddress(LoadLibraryA("opcservices.dll"), "DllGetClassObject");
        void *obj = (void *)1;
        check(get(&IID_IOpcFactory, &IID_IClassFactory, &obj) == CLASS_E_CLASSNOTAVAILABLE && !obj, "unknown class");
    }

    IOpcPartUri_Release(uri2);
    IOpcPartUri_Release(uri);
    IOpcRelationshipSet_Release(rels2);
    IOpcPart_Release(part2);
    IOpcPartSet_Release(parts);
    IOpcPackage_Release(package);
    IOpcFactory_Release(factory);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
