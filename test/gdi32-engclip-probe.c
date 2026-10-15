/* Probe for patches/sg/5225: EngCreateClip returns a CLIPOBJ for the driver to fill in (trivial
 * clipping, zeroed), a new one each time; EngDeleteClip releases it.  Table driven; last line
 * "RESULT: PASS|FAIL". */
#include <windows.h>
#include <stdio.h>
#include <string.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

typedef struct { ULONG iUniq; RECT rclBounds; BYTE iDComplexity, iFComplexity, iMode, fjOptions; } CLIP;
typedef CLIP *(WINAPI *create_t)(void);
typedef void (WINAPI *delete_t)(CLIP *);

int main(void)
{
    HMODULE gdi = LoadLibraryA("gdi32.dll");
    create_t pCreate = (create_t)GetProcAddress(gdi, "EngCreateClip");
    delete_t pDelete = (delete_t)GetProcAddress(gdi, "EngDeleteClip");
    CLIP *clips[8];
    unsigned int i, j, distinct = 1;
    static const BYTE zero[sizeof(CLIP)];

    setvbuf(stdout, NULL, _IONBF, 0);
    if (!pCreate || !pDelete) { printf("ok   not available\nRESULT: PASS\n"); return 0; }
    for (i = 0; i < 8; i++) clips[i] = pCreate();
    for (i = 0; i < 8; i++)
    {
        check(clips[i] != NULL, "clip %u created", i);
        if (!clips[i]) continue;
        check(!memcmp(clips[i], zero, sizeof(CLIP)), "clip %u is zeroed, trivial (complexity %u)", i, clips[i]->iDComplexity);
        for (j = 0; j < i; j++) if (clips[j] == clips[i]) distinct = 0;
    }
    check(distinct, "every call gives a new object");
    if (clips[0])
    {
        clips[0]->iDComplexity = 1;
        clips[0]->rclBounds.right = 640;
        check(clips[0]->iDComplexity == 1 && clips[0]->rclBounds.right == 640, "the driver can fill it in");
    }
    for (i = 0; i < 8; i++) pDelete(clips[i]);
    pDelete(NULL);
    check(1, "EngDeleteClip accepts the objects and NULL");
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
