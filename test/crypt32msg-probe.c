/* crypt32 message behaviours that Wine's own conformance tests record from
 * real Windows (patches/sg/2417), run by test/crypt32msg-gate.sh: a hash
 * message without content verifies through CryptVerifyMessageHash and, given
 * nothing to hash, through CryptVerifyDetachedMessageHash; the hash or the
 * signature having been checked, the data can no longer be added;
 * CryptHashMessage sizes a one-piece message from several pieces;
 * CryptSignMessage without a signing certificate makes a message. */
#include <windows.h>
#include <wincrypt.h>
#include <stdio.h>
#include <string.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

/* a hashed message (MD5 of {1,2,3,4}) with no content */
static BYTE detachedHashContent[] = {
0x30,0x3f,0x06,0x09,0x2a,0x86,0x48,0x86,0xf7,0x0d,0x01,0x07,0x05,0xa0,0x32,
0x30,0x30,0x02,0x01,0x00,0x30,0x0c,0x06,0x08,0x2a,0x86,0x48,0x86,0xf7,0x0d,
0x02,0x05,0x05,0x00,0x30,0x0b,0x06,0x09,0x2a,0x86,0x48,0x86,0xf7,0x0d,0x01,
0x07,0x01,0x04,0x10,0x08,0xd6,0xc0,0x5a,0x21,0x51,0x2a,0x79,0xa1,0xdf,0xeb,
0x9d,0x2a,0x8f,0x26,0x2f };
static const BYTE msgData[] = { 1, 2, 3, 4 };

int main(void)
{
    CRYPT_HASH_MESSAGE_PARA hp;
    CRYPT_SIGN_MESSAGE_PARA sp;
    HCRYPTMSG msg;
    DWORD size, hashSize = 0, blobSize;
    const BYTE *pData = msgData;
    const BYTE *pieces[2] = { msgData, msgData };
    DWORD sizes[2] = { 4, 4 };
    BOOL ret;

    memset(&hp, 0, sizeof(hp));
    hp.cbSize = sizeof(hp);
    hp.dwMsgEncodingType = PKCS_7_ASN_ENCODING;

    /* nothing to hash and nothing asked back: nothing to check */
    ret = CryptVerifyDetachedMessageHash(&hp, detachedHashContent, sizeof(detachedHashContent), 0, NULL, NULL, NULL, NULL);
    check(ret, "CryptVerifyDetachedMessageHash with no data to hash succeeds");
    SetLastError(0xdeadbeef);
    ret = CryptVerifyDetachedMessageHash(&hp, detachedHashContent, sizeof(detachedHashContent), 0, NULL, NULL, NULL, &hashSize);
    check(!ret && GetLastError() == CRYPT_E_HASH_VALUE, "but asking for the computed hash checks it, and it fails");
    size = sizeof(msgData);
    ret = CryptVerifyDetachedMessageHash(&hp, detachedHashContent, sizeof(detachedHashContent), 1, &pData, &size, NULL, &hashSize);
    check(ret && hashSize == 16, "given the right data it verifies and sizes the hash");
    size = 3;
    SetLastError(0xdeadbeef);
    ret = CryptVerifyDetachedMessageHash(&hp, detachedHashContent, sizeof(detachedHashContent), 1, &pData, &size, NULL, NULL);
    check(!ret && GetLastError() == CRYPT_E_HASH_VALUE, "given other data it fails with CRYPT_E_HASH_VALUE");

    /* a message without content verifies there, not through CryptMsgControl */
    ret = CryptVerifyMessageHash(&hp, detachedHashContent, sizeof(detachedHashContent), NULL, NULL, NULL, NULL);
    check(ret, "CryptVerifyMessageHash of a message without content succeeds");
    msg = CryptMsgOpenToDecode(PKCS_7_ASN_ENCODING, 0, 0, 0, NULL, NULL);
    CryptMsgUpdate(msg, detachedHashContent, sizeof(detachedHashContent), TRUE);
    ret = CryptMsgControl(msg, 0, CMSG_CTRL_VERIFY_HASH, NULL);
    check(!ret, "while CryptMsgControl(VERIFY_HASH) on it fails");
    CryptMsgClose(msg);

    /* after the hash was checked, the data cannot be added */
    msg = CryptMsgOpenToDecode(PKCS_7_ASN_ENCODING, CMSG_DETACHED_FLAG, 0, 0, NULL, NULL);
    ret = CryptMsgUpdate(msg, detachedHashContent, sizeof(detachedHashContent), TRUE);
    check(ret, "a detached hash message takes its header");
    SetLastError(0xdeadbeef);
    ret = CryptMsgControl(msg, 0, CMSG_CTRL_VERIFY_HASH, NULL);
    check(!ret && GetLastError() == CRYPT_E_HASH_VALUE, "its hash cannot be checked before it has its data");
    SetLastError(0xdeadbeef);
    ret = CryptMsgUpdate(msg, msgData, sizeof(msgData), TRUE);
    check(!ret && (GetLastError() == NTE_BAD_HASH_STATE || GetLastError() == CRYPT_E_MSG_ERROR),
          "and the data can then no longer be added");
    CryptMsgClose(msg);
    msg = CryptMsgOpenToDecode(PKCS_7_ASN_ENCODING, CMSG_DETACHED_FLAG, 0, 0, NULL, NULL);
    CryptMsgUpdate(msg, detachedHashContent, sizeof(detachedHashContent), TRUE);
    ret = CryptMsgUpdate(msg, msgData, sizeof(msgData), TRUE);
    check(ret, "given in the right order the data are taken");
    ret = CryptMsgControl(msg, 0, CMSG_CTRL_VERIFY_HASH, NULL);
    check(ret, "and the hash verifies");
    CryptMsgClose(msg);

    /* CryptHashMessage */
    hp.HashAlgorithm.pszObjId = (char *)szOID_RSA_MD5;
    blobSize = 0;
    SetLastError(0xdeadbeef);
    ret = CryptHashMessage(&hp, FALSE, 2, pieces, sizes, NULL, &blobSize, NULL, NULL);
    check(ret && blobSize > 0, "CryptHashMessage sizes a one-piece message from two pieces");
    if (ret)
    {
        BYTE *blob = HeapAlloc(GetProcessHeap(), 0, blobSize);
        SetLastError(0xdeadbeef);
        ret = CryptHashMessage(&hp, FALSE, 2, pieces, sizes, blob, &blobSize, NULL, NULL);
        check(!ret && GetLastError() == CRYPT_E_MSG_ERROR, "but making it fails with CRYPT_E_MSG_ERROR");
        HeapFree(GetProcessHeap(), 0, blob);
    }
    blobSize = 0;
    ret = CryptHashMessage(&hp, TRUE, 2, pieces, sizes, NULL, &blobSize, NULL, NULL);
    check(ret && blobSize > 0, "a detached hash of two pieces is sized");

    /* CryptSignMessage without a signing certificate */
    memset(&sp, 0, sizeof(sp));
    sp.cbSize = sizeof(sp);
    sp.dwMsgEncodingType = PKCS_7_ASN_ENCODING;
    blobSize = 0;
    ret = CryptSignMessage(&sp, FALSE, 0, NULL, NULL, NULL, &blobSize);
    check(ret && blobSize > 0, "CryptSignMessage with no signer sizes a message");
    if (ret && blobSize)
    {
        BYTE *blob = HeapAlloc(GetProcessHeap(), 0, 4096);
        DWORD type = 0, n = sizeof(type);

        blobSize = 4096;
        ret = CryptSignMessage(&sp, FALSE, 1, pieces, sizes, blob, &blobSize);
        check(ret, "and makes it");
        msg = CryptMsgOpenToDecode(PKCS_7_ASN_ENCODING, 0, 0, 0, NULL, NULL);
        ret = ret && CryptMsgUpdate(msg, blob, blobSize, TRUE);
        check(ret && CryptMsgGetParam(msg, CMSG_TYPE_PARAM, 0, &type, &n) && type == CMSG_SIGNED, "a signed message");
        n = sizeof(type);
        type = 99;
        check(CryptMsgGetParam(msg, CMSG_SIGNER_COUNT_PARAM, 0, &type, &n) && type == 0, "with no signers");
        CryptMsgClose(msg);
        HeapFree(GetProcessHeap(), 0, blob);
    }
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
