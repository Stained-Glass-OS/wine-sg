#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <stdio.h>
static int bad, total;
#define EXPECT_HR(e, c) do { HRESULT h_ = (c); total++; if (h_ != (e)) { bad++; printf("MISMATCH line %d: %s = %lx, expected %lx\n", __LINE__, #c, h_, (HRESULT)(e)); } } while (0)
#define EXPECT(c) do { total++; if (!(c)) { bad++; printf("MISMATCH line %d: %s\n", __LINE__, #c); } } while (0)

int main(void)
{
    IDataObject *data = NULL;
    IEnumSTATDATA *enum_advise = (IEnumSTATDATA *)1;
    FORMATETC fmt = { CF_TEXT, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    DWORD conn = 0xdead;

    setvbuf(stdout, NULL, _IONBF, 0);
    OleInitialize(NULL);
    EXPECT_HR(S_OK, OleGetClipboard(&data));
    EXPECT(data != NULL);
    if (data)
    {
        EXPECT_HR(OLE_E_ADVISENOTSUPPORTED, IDataObject_DAdvise(data, &fmt, 0, NULL, &conn));
        EXPECT(conn == 0);
        EXPECT_HR(OLE_E_ADVISENOTSUPPORTED, IDataObject_DUnadvise(data, 1));
        EXPECT_HR(OLE_E_ADVISENOTSUPPORTED, IDataObject_EnumDAdvise(data, &enum_advise));
        EXPECT(enum_advise == NULL);
        IDataObject_Release(data);
    }
    OleUninitialize();
    printf("checks %d, mismatches %d\nRESULT: %s\n", total, bad, bad ? "FAIL" : "PASS");
    return bad != 0;
}
