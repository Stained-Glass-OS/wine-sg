/* relsd-gate.sh's probe (0507): RtlValidRelativeSecurityDescriptor. */
#include <windows.h>
#include <sddl.h>
#include <stdio.h>

typedef BOOLEAN (WINAPI *valid_fn)(PSECURITY_DESCRIPTOR, ULONG, SECURITY_INFORMATION);

int main(void)
{
    valid_fn valid = (valid_fn)GetProcAddress(GetModuleHandleA("ntdll.dll"), "RtlValidRelativeSecurityDescriptor");
    PSECURITY_DESCRIPTOR sd = NULL, nodacl = NULL;
    ULONG len = 0, len2 = 0;
    SECURITY_DESCRIPTOR_RELATIVE *rel;
    BYTE copy[1024];
    SECURITY_DESCRIPTOR abs;

    /* the kind registry hives hold: owner, group, a DACL */
    ConvertStringSecurityDescriptorToSecurityDescriptorA("O:BAG:SYD:(A;OICI;KA;;;SY)(A;OICI;KA;;;BA)(A;OICI;KR;;;BU)",
                                                         SDDL_REVISION_1, &sd, &len);
    ConvertStringSecurityDescriptorToSecurityDescriptorA("O:BAG:SY", SDDL_REVISION_1, &nodacl, &len2);
    printf("valid %d\n", valid(sd, len, OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION));
    printf("truncated %d\n", valid(sd, len - 8, 0));
    printf("tiny %d\n", valid(sd, 8, 0));
    memcpy(copy, sd, len);
    rel = (SECURITY_DESCRIPTOR_RELATIVE *)copy;
    rel->Owner = len + 64;
    printf("badowner %d\n", valid(copy, len, 0));
    memcpy(copy, sd, len);
    rel->Control &= ~SE_SELF_RELATIVE;
    printf("absolute %d\n", valid(copy, len, 0));
    printf("nodacl-asked %d nodacl-notasked %d\n", valid(nodacl, len2, DACL_SECURITY_INFORMATION), valid(nodacl, len2, OWNER_SECURITY_INFORMATION));
    InitializeSecurityDescriptor(&abs, SECURITY_DESCRIPTOR_REVISION);
    printf("null %d\n", valid(NULL, 100, 0));
    return 0;
}
