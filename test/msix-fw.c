/* msix-fw: sgframework.dll, the payload of test/msix-gate.sh's framework package.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
__declspec(dllexport) int sg_framework_answer(void)
{
    return 7;
}
