/* mscms, fast mode (patch 3544): profile from a logical color space, element size/reference, named colors, gamut checks,
   device links, standard color space and CMM registration */
#include <windows.h>
#include <icm.h>
#include <stdio.h>
#include <string.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static unsigned be32(const BYTE *p) { return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3]; }
static void put32(BYTE *p, unsigned v) { p[0] = v >> 24; p[1] = v >> 16; p[2] = v >> 8; p[3] = v; }

int main(void)
{
    LOGCOLORSPACEA space;
    PBYTE data = NULL;
    HPROFILE profile;
    PROFILE desc;
    DWORD size, count, i;
    BOOL ret, present;

    memset(&space, 0, sizeof(space));
    space.lcsSignature = 0x50534f43;
    space.lcsVersion = 0x400;
    space.lcsSize = sizeof(space);
    space.lcsCSType = LCS_sRGB;
    ret = CreateProfileFromLogColorSpaceA(&space, &data);
    check(ret && data && be32(data + 36) == 0x61637370, "sRGB profile from a logical color space");
    size = data ? be32(data) : 0;
    check(size > 128 && size < 100000, "profile size in the header");

    /* calibrated RGB: Rec.709 primaries and gamma 2.2 as 2.30 fixed point endpoints */
    {
        LOGCOLORSPACEA cal = space;
        PBYTE data2 = NULL;
        cal.lcsCSType = LCS_CALIBRATED_RGB;
        cal.lcsEndpoints.ciexyzRed.ciexyzX = (FXPT2DOT30)(0.4124 * 1073741824.0);
        cal.lcsEndpoints.ciexyzRed.ciexyzY = (FXPT2DOT30)(0.2126 * 1073741824.0);
        cal.lcsEndpoints.ciexyzRed.ciexyzZ = (FXPT2DOT30)(0.0193 * 1073741824.0);
        cal.lcsEndpoints.ciexyzGreen.ciexyzX = (FXPT2DOT30)(0.3576 * 1073741824.0);
        cal.lcsEndpoints.ciexyzGreen.ciexyzY = (FXPT2DOT30)(0.7152 * 1073741824.0);
        cal.lcsEndpoints.ciexyzGreen.ciexyzZ = (FXPT2DOT30)(0.1192 * 1073741824.0);
        cal.lcsEndpoints.ciexyzBlue.ciexyzX = (FXPT2DOT30)(0.1805 * 1073741824.0);
        cal.lcsEndpoints.ciexyzBlue.ciexyzY = (FXPT2DOT30)(0.0722 * 1073741824.0);
        cal.lcsEndpoints.ciexyzBlue.ciexyzZ = (FXPT2DOT30)(0.9505 * 1073741824.0);
        cal.lcsGammaRed = cal.lcsGammaGreen = cal.lcsGammaBlue = (DWORD)(2.2 * 65536.0);
        ret = CreateProfileFromLogColorSpaceA(&cal, &data2);
        check(ret && data2 && be32(data2 + 36) == 0x61637370, "calibrated RGB profile");
        if (data2) LocalFree(data2);
        cal.lcsSignature = 0;
        check(!CreateProfileFromLogColorSpaceA(&cal, &data2), "bad signature");
    }

    desc.dwType = PROFILE_MEMBUFFER;
    desc.pProfileData = data;
    desc.cbDataSize = size;
    profile = OpenColorProfileA(&desc, PROFILE_READWRITE, 0, OPEN_EXISTING);
    check(profile != NULL, "open the profile");

    /* element size and reference */
    GetCountColorProfileElements(profile, &count);
    present = TRUE;
    IsColorProfileTagPresent(profile, 0x74657374 /* 'test' */, &present);
    check(!present, "no test tag yet");
    check(SetColorProfileElementSize(profile, 0x74657374, 16), "add a tag");
    IsColorProfileTagPresent(profile, 0x74657374, &present);
    {
        DWORD count2 = 0;
        GetCountColorProfileElements(profile, &count2);
        check(present && count2 == count + 1, "the tag is there");
    }
    {
        BYTE buf[16] = "0123456789abcde", got[16];
        DWORD len = 16;
        check(SetColorProfileElement(profile, 0x74657374, 0, &len, buf), "write the tag");
        len = 16;
        memset(got, 0, sizeof(got));
        check(GetColorProfileElement(profile, 0x74657374, 0, &len, got, &present) && !memcmp(got, buf, 16), "read the tag back");
        check(SetColorProfileElementSize(profile, 0x74657374, 32), "grow the tag");
        len = 0;
        GetColorProfileElement(profile, 0x74657374, 0, &len, NULL, &present);
        check(len == 32, "new size");
        len = 32;
        { BYTE big[32]; memset(big, 0xaa, 32);
          check(GetColorProfileElement(profile, 0x74657374, 0, &len, big, &present) && !memcmp(big, buf, 16) && big[20] == 0, "old data kept, new bytes zero"); }
        check(SetColorProfileElementSize(profile, 0x74657374, 8), "shrink the tag");
        len = 0;
        GetColorProfileElement(profile, 0x74657374, 0, &len, NULL, &present);
        check(len == 8, "shrunk size");
    }
    check(SetColorProfileElementReference(profile, 0x74657432 /* 'tet2' */, 0x74657374), "reference a tag");
    {
        DWORD l1 = 0, l2 = 0;
        BOOL linked = FALSE;
        GetColorProfileElement(profile, 0x74657432, 0, &l1, NULL, &linked);
        l2 = l1;
        { BYTE tmp[8]; GetColorProfileElement(profile, 0x74657432, 0, &l2, tmp, &linked); }
        check(l1 == 8 && linked, "referencing tag has the same data");
    }
    check(!SetColorProfileElementReference(profile, 0x74657433, 0x7a7a7a7a), "reference to a missing tag fails");
    {
        BYTE out[16384];
        DWORD len = sizeof(out);
        check(GetColorProfileFromHandle(profile, out, &len) && be32(out) == len && be32(out + 128) == count + 2, "profile stays consistent");
        for (i = 0; i < count + 2; i++)
        {
            unsigned off = be32(out + 132 + i * 12 + 4), sz = be32(out + 132 + i * 12 + 8);
            if (off + sz > len) { check(0, "tag inside the profile"); break; }
        }
        check(i == count + 2, "all tags inside the profile");
    }
    check(SetColorProfileElementSize(profile, 0x74657374, 0) == FALSE, "zero size");
    CloseColorProfile(profile);

    /* read-only profile cannot change */
    profile = OpenColorProfileA(&desc, PROFILE_READ, 0, OPEN_EXISTING);
    check(!SetColorProfileElementSize(profile, 0x74657374, 4) && GetLastError() == ERROR_ACCESS_DENIED, "read-only profile");
    {
        BYTE intent[64];
        DWORD len = sizeof(intent);
        check(GetPS2ColorRenderingIntent(profile, 0, intent, &len) && len == strlen((char *)intent) + 1, "PostScript rendering intent");
        len = 2;
        check(!GetPS2ColorRenderingIntent(profile, 0, intent, &len) && GetLastError() == ERROR_INSUFFICIENT_BUFFER && len > 2, "intent buffer too small");
    }

    /* transform and gamut checks */
    {
        LOGCOLORSPACEA ls = space;
        HTRANSFORM xf;
        BYTE px[4 * 2] = { 10, 200, 40, 0, 250, 3, 3, 0 }, res[2] = { 7, 7 };
        COLOR colors[1];
        BYTE cres = 7;

        xf = CreateColorTransformA(&ls, profile, NULL, 0);
        check(xf != NULL, "color transform to the profile");
        ret = CheckBitmapBits(xf, px, BM_xRGBQUADS, 2, 1, 8, res, NULL, 0);
        check(ret && res[0] == 0xff && res[1] == 0xff, "sRGB colors are inside the sRGB gamut");
        colors[0].rgb.red = 30000; colors[0].rgb.green = 12000; colors[0].rgb.blue = 50000;
        ret = CheckColors(xf, colors, 1, COLOR_RGB, &cres);
        check(ret && cres == 0xff, "CheckColors inside");
        check(!CheckColors(xf, colors, 1, (COLORTYPE)99, &cres), "CheckColors bad type");
        check(GetCMMInfo(xf, CMM_IDENT) != 0, "GetCMMInfo");
        check(DeleteColorTransform(xf), "delete transform");
    }


    /* a narrow gamut: pure sRGB red is outside of it */
    {
        LOGCOLORSPACEA nar = space, ls = space;
        PBYTE ndata = NULL;
        PROFILE nd;
        HPROFILE nprof;
        HTRANSFORM xf;
        BYTE px[8] = { 0, 0, 255, 0, 128, 128, 128, 0 }, res[2] = { 9, 9 };
        double v[3][3] = { { 0.40, 0.30, 0.10 }, { 0.31, 0.37, 0.18 }, { 0.22, 0.20, 0.45 } };
        FXPT2DOT30 *e = (FXPT2DOT30 *)&nar.lcsEndpoints;
        int k;

        nar.lcsCSType = LCS_CALIBRATED_RGB;
        for (k = 0; k < 9; k++) e[k] = (FXPT2DOT30)(v[k / 3][k % 3] * 1073741824.0);
        nar.lcsGammaRed = nar.lcsGammaGreen = nar.lcsGammaBlue = 2 * 65536;
        if (CreateProfileFromLogColorSpaceA(&nar, &ndata))
        {
            nd.dwType = PROFILE_MEMBUFFER; nd.pProfileData = ndata; nd.cbDataSize = be32(ndata);
            nprof = OpenColorProfileA(&nd, PROFILE_READ, 0, OPEN_EXISTING);
            xf = CreateColorTransformA(&ls, nprof, NULL, 0);
            check(xf != NULL, "transform to a narrow gamut");
            ret = CheckBitmapBits(xf, px, BM_xRGBQUADS, 2, 1, 8, res, NULL, 0);
            check(ret && res[0] == 0 && res[1] == 0xff, "red is out of gamut, gray is inside");
            DeleteColorTransform(xf);
            CloseColorProfile(nprof);
            LocalFree(ndata);
        }
        else check(0, "narrow profile");
    }

    /* device link */
    {
        HPROFILE two[2] = { profile, profile };
        DWORD intents[1] = { INTENT_PERCEPTUAL };
        PBYTE link = NULL;
        ret = CreateDeviceLinkProfile(two, 2, intents, 1, 0, &link, 0);
        check(ret && link && be32(link + 36) == 0x61637370 && be32(link + 12) == 0x6c696e6b /* 'link' */, "device link profile");
        if (link) LocalFree(link);
        check(!CreateDeviceLinkProfile(two, 1, intents, 1, 0, &link, 0), "device link needs two profiles");
    }
    CloseColorProfile(profile);

    /* named colors: a hand made 'ncl2' profile */
    {
        BYTE np[512];
        BYTE *tag;
        DWORD npsize;
        HPROFILE nprof;
        NAMED_PROFILE_INFO info;
        COLOR_NAME names[2];
        DWORD idx[2], names_idx[2] = { 2, 1 };
        PROFILE nd;

        memcpy(np, data, 128);
        put32(np + 12, 0x6e6d636c); /* 'nmcl' */
        put32(np + 128, 1);
        put32(np + 132, 0x6e636c32);
        put32(np + 136, 144);
        tag = np + 144;
        put32(tag + 8, 0x1234);
        put32(tag + 0, 0x6e636c32);
        put32(tag + 4, 0);
        put32(tag + 12, 2);   /* count */
        put32(tag + 16, 0);   /* device coordinates */
        memset(tag + 20, 0, 64);
        strcpy((char *)tag + 20, "PRE-");
        strcpy((char *)tag + 52, "-SUF");
        memset(tag + 84, 0, 2 * 38);
        strcpy((char *)tag + 84, "Alpha");
        strcpy((char *)tag + 84 + 38, "Beta");
        put32(np + 140, 84 + 76);
        npsize = 144 + 84 + 76;
        put32(np, npsize);
        nd.dwType = PROFILE_MEMBUFFER;
        nd.pProfileData = np;
        nd.cbDataSize = npsize;
        nprof = OpenColorProfileA(&nd, PROFILE_READ, 0, OPEN_EXISTING);
        if (!nprof) check(0, "open named profile (lcms may refuse it)");
        else
        {
            memset(&info, 0xcc, sizeof(info));
            check(GetNamedProfileInfo(nprof, &info) && info.dwFlags == 0x1234 && info.dwCount == 2 && info.dwCountDevCoordinates == 0 &&
                    !strcmp(info.szPrefix, "PRE-") && !strcmp(info.szSuffix, "-SUF"), "GetNamedProfileInfo");
            memset(names, 0, sizeof(names));
            strcpy(names[0], "PRE-Beta-SUF");
            strcpy(names[1], "Alpha");
            check(ConvertColorNameToIndex(nprof, names, idx, 2) && idx[0] == 2 && idx[1] == 1, "ConvertColorNameToIndex");
            strcpy(names[0], "Nope");
            check(!ConvertColorNameToIndex(nprof, names, idx, 1), "unknown name");
            check(ConvertIndexToColorName(nprof, names_idx, names, 2) && !strcmp(names[0], "PRE-Beta-SUF") && !strcmp(names[1], "PRE-Alpha-SUF"),
                    "ConvertIndexToColorName");
            names_idx[0] = 3;
            check(!ConvertIndexToColorName(nprof, names_idx, names, 1), "index out of range");
            CloseColorProfile(nprof);
        }
        check(!GetNamedProfileInfo(NULL, &info), "named info without a profile");
    }

    /* standard color space registration */
    {
        char path[MAX_PATH], buf[MAX_PATH];
        DWORD len = sizeof(buf);
        wsprintfA(path, "C:\\fastb-test.icm");
        check(SetStandardColorSpaceProfileA(NULL, LCS_sRGB, path), "SetStandardColorSpaceProfile");
        len = sizeof(buf);
        check(GetStandardColorSpaceProfileA(NULL, LCS_sRGB, buf, &len) && !strcmp(buf, path), "GetStandardColorSpaceProfile returns it");
        check(!SetStandardColorSpaceProfileA("remote", LCS_sRGB, path) && GetLastError() == ERROR_NOT_SUPPORTED, "remote machine");
    }

    /* CMM registration */
    {
        check(RegisterCMMA(NULL, 0x74657374, "testcmm.dll"), "RegisterCMM");
        check(SelectCMM(0x74657374), "SelectCMM of a registered module");
        check(SelectCMM(0x57696e20), "SelectCMM of the default module");
        check(UnregisterCMMA(NULL, 0x74657374), "UnregisterCMM");
        check(!SelectCMM(0x74657374) && GetLastError() == ERROR_INVALID_PARAMETER, "SelectCMM of an unregistered module");
        check(!UnregisterCMMA(NULL, 0x74657374), "UnregisterCMM twice");
        check(!RegisterCMMA("remote", 0x74657374, "x.dll"), "RegisterCMM remote");
    }

    LocalFree(data);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
