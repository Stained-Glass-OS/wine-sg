/* The smart card database (wine-sg 7753): SCardIntroduceCardType registers a card type under
 * HKLM\Software\Microsoft\Cryptography\Calais\SmartCards; SCardListCards finds it by its ATR (under
 * its mask) and its interfaces; SCardGetProviderId, SCardListInterfaces and
 * SCardGet/SetCardTypeProviderName read and write its providers; SCardForgetCardType removes it.
 * They were stubs. Prints ok/FAIL lines and RESULT: PASS|FAIL. */
#include <windows.h>
#include <winscard.h>
#include <stdio.h>
#include <wchar.h>

#ifndef SCARD_PROVIDER_PRIMARY
#define SCARD_PROVIDER_PRIMARY     1
#endif
#ifndef SCARD_PROVIDER_CSP
#define SCARD_PROVIDER_CSP         2
#endif
#ifndef SCARD_PROVIDER_KSP
#define SCARD_PROVIDER_KSP         3
#endif
#ifndef SCARD_PROVIDER_CARD_MODULE
#define SCARD_PROVIDER_CARD_MODULE 0x80000001
#endif

static int failures;
#define CHECK(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

static const GUID primary = {0x7a3f1c2e,0x41d0,0x4b6a,{0x9e,0x12,0x35,0x6c,0x80,0xa1,0x0f,0x5d}};
static const GUID iface[2] = {{0x11111111,0x2222,0x3333,{0x44,0x44,0x55,0x55,0x55,0x55,0x55,0x55}},
                              {0x66666666,0x7777,0x8888,{0x99,0x99,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa}}};
static const GUID other = {0xdeadbeef,0x1234,0x5678,{1,2,3,4,5,6,7,8}};
/* T=0 card, historical bytes only: TS T0 (4 historical bytes) */
static const BYTE atr[]  = {0x3b, 0x04, 0x41, 0x11, 0x77, 0x81};
static const BYTE mask[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0x00};   /* the last byte is free */
static const BYTE atr_other_last[] = {0x3b, 0x04, 0x41, 0x11, 0x77, 0x42};
static const BYTE atr_different[]  = {0x3b, 0x04, 0x41, 0x12, 0x77, 0x81};

static BOOL in_list(const WCHAR *list, const WCHAR *name)
{
    for (; list && *list; list += wcslen(list) + 1) if (!wcscmp(list, name)) return TRUE;
    return FALSE;
}

static BOOL listed(const BYTE *a, const GUID *ifs, DWORD n, const WCHAR *name)
{
    WCHAR buf[4096];
    DWORD len = ARRAYSIZE(buf);
    LONG r = SCardListCardsW(0, a, ifs, n, buf, &len);
    return r == SCARD_S_SUCCESS && in_list(buf, name);
}

int main(void)
{
    const WCHAR *card = L"SG Test Card";
    WCHAR buf[512], *alloc = NULL;
    char bufA[512];
    DWORD len, count;
    GUID guid, guids[4], *galloc = NULL;
    LONG r;
    HKEY key = NULL;

    SCardForgetCardTypeW(0, card);
    r = SCardIntroduceCardTypeW(0, card, &primary, iface, 2, atr, mask, sizeof(atr));
    CHECK(r == SCARD_S_SUCCESS, "SCardIntroduceCardType (%#lx)", r);
    CHECK(!RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Cryptography\\Calais\\SmartCards\\SG Test Card",
                         0, KEY_READ, &key), "the card's key is in the machine's smart card database");
    if (key) RegCloseKey(key);

    CHECK(listed(NULL, NULL, 0, card), "SCardListCards with no ATR lists it");
    CHECK(listed(atr, NULL, 0, card), "its ATR finds it");
    CHECK(listed(atr_other_last, NULL, 0, card), "an ATR that differs only under the mask's 0 bits finds it");
    CHECK(!listed(atr_different, NULL, 0, card), "an ATR that differs under the mask does not");
    CHECK(listed(atr, &iface[1], 1, card), "an interface it supports finds it");
    CHECK(!listed(atr, &other, 1, card), "an interface it does not support does not");

    len = SCARD_AUTOALLOCATE;
    r = SCardListCardsW(0, atr, NULL, 0, (WCHAR *)&alloc, &len);
    CHECK(r == SCARD_S_SUCCESS && alloc && in_list(alloc, card), "SCARD_AUTOALLOCATE list (%#lx)", r);
    if (alloc) SCardFreeMemory(0, alloc);
    len = sizeof(bufA);
    r = SCardListCardsA(0, atr, NULL, 0, bufA, &len);
    CHECK(r == SCARD_S_SUCCESS && !strcmp(bufA, "SG Test Card"), "SCardListCardsA (%#lx) %s", r, bufA);

    r = SCardGetProviderIdW(0, card, &guid);
    CHECK(r == SCARD_S_SUCCESS && IsEqualGUID(&guid, &primary), "SCardGetProviderId (%#lx)", r);
    count = ARRAYSIZE(guids);
    r = SCardListInterfacesW(0, card, guids, &count);
    CHECK(r == SCARD_S_SUCCESS && count == 2 && IsEqualGUID(&guids[1], &iface[1]), "SCardListInterfaces (%#lx, %lu)", r, count);
    count = SCARD_AUTOALLOCATE;
    r = SCardListInterfacesW(0, card, (GUID *)&galloc, &count);
    CHECK(r == SCARD_S_SUCCESS && count == 2 && galloc && IsEqualGUID(&galloc[0], &iface[0]), "SCardListInterfaces autoallocate (%#lx)", r);
    if (galloc) SCardFreeMemory(0, galloc);

    r = SCardSetCardTypeProviderNameW(0, card, SCARD_PROVIDER_CSP, L"Microsoft Base Smart Card Crypto Provider");
    CHECK(r == SCARD_S_SUCCESS, "set the CSP (%#lx)", r);
    r = SCardSetCardTypeProviderNameW(0, card, SCARD_PROVIDER_KSP, L"Microsoft Smart Card Key Storage Provider");
    CHECK(r == SCARD_S_SUCCESS, "set the KSP (%#lx)", r);
    r = SCardSetCardTypeProviderNameA(0, "SG Test Card", SCARD_PROVIDER_CARD_MODULE, "sgtestmd.dll");
    CHECK(r == SCARD_S_SUCCESS, "set the card module (A) (%#lx)", r);
    len = ARRAYSIZE(buf);
    r = SCardGetCardTypeProviderNameW(0, card, SCARD_PROVIDER_CSP, buf, &len);
    CHECK(r == SCARD_S_SUCCESS && !wcscmp(buf, L"Microsoft Base Smart Card Crypto Provider"), "get the CSP (%#lx)", r);
    len = sizeof(bufA);
    r = SCardGetCardTypeProviderNameA(0, "SG Test Card", SCARD_PROVIDER_CARD_MODULE, bufA, &len);
    CHECK(r == SCARD_S_SUCCESS && !strcmp(bufA, "sgtestmd.dll") && len == 13, "get the card module (A) (%#lx, %lu)", r, len);
    len = 3;
    r = SCardGetCardTypeProviderNameW(0, card, SCARD_PROVIDER_KSP, buf, &len);
    CHECK(r == SCARD_E_INSUFFICIENT_BUFFER && len == 42, "a short buffer gets the length (%#lx, %lu)", r, len);
    len = ARRAYSIZE(buf);
    r = SCardGetCardTypeProviderNameW(0, card, SCARD_PROVIDER_PRIMARY, buf, &len);
    CHECK(r == SCARD_S_SUCCESS && !_wcsicmp(buf, L"{7A3F1C2E-41D0-4B6A-9E12-356C80A10F5D}"), "the primary provider as a GUID string (%ls)", buf);
    r = SCardSetCardTypeProviderNameW(0, card, 4, L"x");
    CHECK(r == SCARD_E_INVALID_PARAMETER, "an unknown provider id is refused (%#lx)", r);

    len = ARRAYSIZE(buf);
    r = SCardGetCardTypeProviderNameW(0, L"SG No Such Card", SCARD_PROVIDER_CSP, buf, &len);
    CHECK(r == SCARD_E_UNKNOWN_CARD, "an unknown card gives SCARD_E_UNKNOWN_CARD (%#lx)", r);

    r = SCardForgetCardTypeA(0, "SG Test Card");
    CHECK(r == SCARD_S_SUCCESS, "SCardForgetCardType (%#lx)", r);
    CHECK(!listed(NULL, NULL, 0, card), "it is no longer listed");
    r = SCardGetProviderIdW(0, card, &guid);
    CHECK(r == SCARD_E_UNKNOWN_CARD, "and is unknown (%#lx)", r);

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
