/* fontrecord gate: start GDI's fonts (win32u's font init, which keeps the Fonts key's
 * external entries) and say how many families GDI has. */
#include <windows.h>
#include <stdio.h>

static int CALLBACK count(const LOGFONTW *lf, const TEXTMETRICW *tm, DWORD type, LPARAM p)
{
    (void)lf; (void)tm; (void)type;
    (*(int *)p)++;
    return 1;
}

int main(void)
{
    LOGFONTW lf = { 0 };
    HDC dc = CreateCompatibleDC(NULL);
    int n = 0;
    lf.lfCharSet = DEFAULT_CHARSET;
    EnumFontFamiliesExW(dc, &lf, count, (LPARAM)&n, 0);
    printf("families=%d\n", n);
    DeleteDC(dc);
    return 0;
}
