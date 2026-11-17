/* startverb-probe: what a document's program is given -- appends its command line to C:\startverb-out.txt
 * (test/startverb-gate.sh) */
#include <windows.h>
#include <stdio.h>
int main(void)
{
    FILE *f = fopen("C:\\startverb-out.txt", "a");
    if (!f) return 1;
    fprintf(f, "%s\n", GetCommandLineA());
    fclose(f);
    return 0;
}
