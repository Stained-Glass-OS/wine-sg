/* Builds a tiny TrueType font in memory for gates: glyphs 0..NUMGLYPHS-1 are boxes of advance 600
 * units (1000 per em); 'A'..'Z' map to glyphs 1..26; an optional GSUB table is added as given.
 * No Microsoft data: the layout follows the public OpenType/TrueType table descriptions. */
#ifndef SGFONT_H
#define SGFONT_H
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define SGF_NUMGLYPHS 40
#define SGF_ADVANCE 600

static void sgf_u16(unsigned char **p, unsigned v) { (*p)[0] = v >> 8; (*p)[1] = v; *p += 2; }
static void sgf_u32(unsigned char **p, unsigned v) { sgf_u16(p, v >> 16); sgf_u16(p, v & 0xffff); }

/* an optional GPOS table, set before sgf_build */
static const unsigned char *sgf_gpos;
static unsigned sgf_gpos_len;

struct sgf_table { char tag[5]; unsigned char *data; unsigned len; };

static unsigned char *sgf_build(const char *family, const unsigned char *gsub, unsigned gsub_len, unsigned *out_len)
{
    struct sgf_table t[12];
    unsigned n = 0, i, off, total;
    unsigned char *p, *out;
    size_t flen = strlen(family);

#define NEWT(tg, size) do { memcpy(t[n].tag, tg, 5); t[n].len = (size); t[n].data = calloc(1, (size) + 4); p = t[n].data; n++; } while (0)
    if (sgf_gpos) { NEWT("GPOS", sgf_gpos_len); memcpy(t[n - 1].data, sgf_gpos, sgf_gpos_len); }
    if (gsub) { NEWT("GSUB", gsub_len); memcpy(t[n - 1].data, gsub, gsub_len); }
    /* OS/2 version 1, 86 bytes */
    NEWT("OS/2", 86);
    sgf_u16(&p, 1); sgf_u16(&p, SGF_ADVANCE); sgf_u16(&p, 400); sgf_u16(&p, 5); sgf_u16(&p, 0);
    for (i = 0; i < 10; i++) sgf_u16(&p, 0);                    /* subscript .. strikeout position */
    sgf_u16(&p, 0);                                             /* sFamilyClass */
    memset(p, 0, 10); p += 10;                                  /* panose */
    sgf_u32(&p, 1); sgf_u32(&p, 0); sgf_u32(&p, 0); sgf_u32(&p, 0); /* unicode ranges: basic latin */
    memcpy(p, "SGTF", 4); p += 4;                               /* vendor */
    sgf_u16(&p, 0x40);                                          /* fsSelection: regular */
    sgf_u16(&p, 0x41); sgf_u16(&p, 0x5a);                       /* first, last char */
    sgf_u16(&p, 800); sgf_u16(&p, (unsigned)-200 & 0xffff); sgf_u16(&p, 0);
    sgf_u16(&p, 900); sgf_u16(&p, 250);                         /* win ascent, descent */
    sgf_u32(&p, 1); sgf_u32(&p, 0);                             /* code page ranges: latin 1 */
    /* cmap: one format 4 subtable for platform 3 encoding 1 */
    NEWT("cmap", 4 + 8 + 32);
    sgf_u16(&p, 0); sgf_u16(&p, 1); sgf_u16(&p, 3); sgf_u16(&p, 1); sgf_u32(&p, 12);
    sgf_u16(&p, 4); sgf_u16(&p, 32); sgf_u16(&p, 0); sgf_u16(&p, 4); sgf_u16(&p, 4); sgf_u16(&p, 1); sgf_u16(&p, 0);
    sgf_u16(&p, 0x5a); sgf_u16(&p, 0xffff); sgf_u16(&p, 0);     /* end codes + pad */
    sgf_u16(&p, 0x41); sgf_u16(&p, 0xffff);                     /* start codes */
    sgf_u16(&p, (unsigned)(1 - 0x41) & 0xffff); sgf_u16(&p, 1); /* deltas */
    sgf_u16(&p, 0); sgf_u16(&p, 0);                             /* range offsets */
    /* glyf: a 500x700 box per glyph, 34 bytes each; loca short format */
    NEWT("glyf", SGF_NUMGLYPHS * 34);
    for (i = 0; i < SGF_NUMGLYPHS; i++)
    {
        sgf_u16(&p, 1); sgf_u16(&p, 0); sgf_u16(&p, 0); sgf_u16(&p, 500); sgf_u16(&p, 700);
        sgf_u16(&p, 3); sgf_u16(&p, 0);
        *p++ = 1; *p++ = 1; *p++ = 1; *p++ = 1;
        sgf_u16(&p, 0); sgf_u16(&p, 500); sgf_u16(&p, 0); sgf_u16(&p, (unsigned)-500 & 0xffff);
        sgf_u16(&p, 0); sgf_u16(&p, 0); sgf_u16(&p, 700); sgf_u16(&p, 0);
    }
    NEWT("head", 54);
    sgf_u32(&p, 0x00010000); sgf_u32(&p, 0x00010000); sgf_u32(&p, 0); sgf_u32(&p, 0x5f0f3cf5);
    sgf_u16(&p, 0x0003); sgf_u16(&p, 1000);
    sgf_u32(&p, 0); sgf_u32(&p, 0); sgf_u32(&p, 0); sgf_u32(&p, 0);   /* created, modified */
    sgf_u16(&p, 0); sgf_u16(&p, 0); sgf_u16(&p, 500); sgf_u16(&p, 700);
    sgf_u16(&p, 0); sgf_u16(&p, 8); sgf_u16(&p, 2); sgf_u16(&p, 0); sgf_u16(&p, 0);
    NEWT("hhea", 36);
    sgf_u32(&p, 0x00010000); sgf_u16(&p, 800); sgf_u16(&p, (unsigned)-200 & 0xffff); sgf_u16(&p, 0);
    sgf_u16(&p, SGF_ADVANCE); sgf_u16(&p, 0); sgf_u16(&p, 0); sgf_u16(&p, 500);
    sgf_u16(&p, 1); sgf_u16(&p, 0); sgf_u16(&p, 0);
    for (i = 0; i < 4; i++) sgf_u16(&p, 0);
    sgf_u16(&p, 0); sgf_u16(&p, SGF_NUMGLYPHS);
    NEWT("hmtx", SGF_NUMGLYPHS * 4);
    for (i = 0; i < SGF_NUMGLYPHS; i++) { sgf_u16(&p, SGF_ADVANCE); sgf_u16(&p, 0); }
    NEWT("loca", (SGF_NUMGLYPHS + 1) * 2);
    for (i = 0; i <= SGF_NUMGLYPHS; i++) sgf_u16(&p, i * 34 / 2);
    NEWT("maxp", 32);
    sgf_u32(&p, 0x00010000); sgf_u16(&p, SGF_NUMGLYPHS); sgf_u16(&p, 4); sgf_u16(&p, 1); sgf_u16(&p, 0);
    sgf_u16(&p, 0); sgf_u16(&p, 1); sgf_u16(&p, 0); sgf_u16(&p, 0); sgf_u16(&p, 0); sgf_u16(&p, 0);
    sgf_u16(&p, 0); sgf_u16(&p, 0); sgf_u16(&p, 0); sgf_u16(&p, 0);
    /* name: family (1), style (2), full name (4), PostScript name (6), UTF-16BE */
    NEWT("name", 6 + 4 * 12 + 4 * 2 * (flen + 7));
    sgf_u16(&p, 0); sgf_u16(&p, 4); sgf_u16(&p, 6 + 4 * 12);
    {
        const char *s[4]; unsigned ids[4] = {1, 2, 4, 6}, o = 0;
        char full[256];
        s[0] = family; s[1] = "Regular"; snprintf(full, sizeof(full), "%s", family); s[2] = full; s[3] = family;
        for (i = 0; i < 4; i++)
        {
            unsigned l = (unsigned)strlen(s[i]) * 2;
            sgf_u16(&p, 3); sgf_u16(&p, 1); sgf_u16(&p, 0x409); sgf_u16(&p, ids[i]); sgf_u16(&p, l); sgf_u16(&p, o);
            o += l;
        }
        for (i = 0; i < 4; i++) { const char *c; for (c = s[i]; *c; c++) { *p++ = 0; *p++ = *c; } }
        t[n - 1].len = 6 + 4 * 12 + o;
    }
    NEWT("post", 32);
    sgf_u32(&p, 0x00030000);

    total = 12 + n * 16;
    for (i = 0; i < n; i++) total += (t[i].len + 3) & ~3;
    out = calloc(1, total);
    p = out;
    {
        unsigned sel = 0, range = 16;
        while ((2u << sel) <= n) sel++, range *= 2;
        sgf_u32(&p, 0x00010000); sgf_u16(&p, n); sgf_u16(&p, range); sgf_u16(&p, sel); sgf_u16(&p, n * 16 - range);
    }
    off = 12 + n * 16;
    for (i = 0; i < n; i++)
    {
        memcpy(p, t[i].tag, 4); p += 4;
        sgf_u32(&p, 0); sgf_u32(&p, off); sgf_u32(&p, t[i].len);
        memcpy(out + off, t[i].data, t[i].len);
        off += (t[i].len + 3) & ~3;
        free(t[i].data);
    }
    *out_len = total;
    return out;
}
#endif
