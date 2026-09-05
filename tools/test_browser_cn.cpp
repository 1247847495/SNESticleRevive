/* test_browser_cn.cpp - PC-side reproduction of the browser name pipeline.
 * Replicates _FontDecode / FontGetStrWidth / BrowserCopyEllipsis /
 * BrowserCopyMarquee exactly as font.cpp + uiBrowser.cpp implement them,
 * against the REAL font_cjk.cpp tables, so we can see what a Chinese ROM
 * filename turns into on a desktop.
 *
 * Build: g++ -O2 -o test_browser_cn tools/test_browser_cn.cpp src/common/base/font_cjk.cpp -Isrc/common/base -Isrc/platform/ps2
 */
#include <cstdio>
#include <cstring>
#include <cstdint>

typedef char Char;
typedef int32_t Int32;
typedef uint32_t Uint32;
typedef uint16_t Uint16;
typedef uint8_t Uint8;
typedef float Float32;
#define TRUE 1
#define FALSE 0

extern "C" {
extern const int    _FontCjkCount;
extern const Uint16 _FontCjkCpSorted[];
extern const Uint16 _FontCjkIdxOfCp[];
extern const Uint16 _FontCjkGbkSorted[];
extern const Uint16 _FontCjkIdxOfGbk[];
}

#define FONT_CP_GBK   0x80000000u
#define FONT_CJK_ADV  12

/* --- exact copy of font.cpp helpers ------------------------------- */
static float g_scaleX = 1.0f, g_scaleY = 1.0f;

static Uint32 _FontDecode(const Char *p, Int32 *pLen)
{
    const unsigned char *s = (const unsigned char *)p;
    unsigned char c = s[0];
    *pLen = 1;
    if (c < 0x80) return c;
    if (c >= 0xC2 && c <= 0xDF)
    {
        if ((s[1] & 0xC0) == 0x80)
        {
            *pLen = 2;
            return ((Uint32)(c & 0x1F) << 6) | (Uint32)(s[1] & 0x3F);
        }
    }
    else if (c >= 0xE0 && c <= 0xEF)
    {
        if ((s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80)
        {
            Uint32 cp = ((Uint32)(c & 0x0F) << 12) |
                        ((Uint32)(s[1] & 0x3F) << 6) |
                        (Uint32)(s[2] & 0x3F);
            if (cp >= 0x800) { *pLen = 3; return cp; }
        }
    }
    else if (c >= 0xF0 && c <= 0xF4)
    {
        if ((s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80 &&
            (s[3] & 0xC0) == 0x80)
        {
            *pLen = 4;
            return ((Uint32)(c & 0x07) << 18) |
                   ((Uint32)(s[1] & 0x3F) << 12) |
                   ((Uint32)(s[2] & 0x3F) << 6) |
                   (Uint32)(s[3] & 0x3F);
        }
    }
    if (c >= 0x81 && c <= 0xFE &&
        s[1] >= 0x40 && s[1] <= 0xFE && s[1] != 0x7F)
    {
        *pLen = 2;
        return FONT_CP_GBK | ((Uint32)c << 8) | (Uint32)s[1];
    }
    return c;
}

static Int32 FontCharLen(const Char *pStr)
{
    Int32 len;
    if (!pStr || !*pStr) return 0;
    _FontDecode(pStr, &len);
    return len;
}

static Int32 _FontCjkLookupCp(Uint32 cp)
{
    Int32 lo = 0, hi = _FontCjkCount - 1;
    while (lo <= hi)
    {
        Int32 mid = (lo + hi) >> 1;
        if (_FontCjkCpSorted[mid] == (Uint16)cp)
            return (Int32)_FontCjkIdxOfCp[mid];
        if (_FontCjkCpSorted[mid] < cp) lo = mid + 1;
        else                            hi = mid - 1;
    }
    return -1;
}

static Int32 _FontCjkLookupGbk(Uint32 gb)
{
    Int32 lo = 0, hi = _FontCjkCount - 1;
    while (lo <= hi)
    {
        Int32 mid = (lo + hi) >> 1;
        if (_FontCjkGbkSorted[mid] == (Uint16)gb)
            return (Int32)_FontCjkIdxOfGbk[mid];
        if (_FontCjkGbkSorted[mid] < gb) lo = mid + 1;
        else                             hi = mid - 1;
    }
    return -1;
}

static Int32 _FontCjkResolve(Uint32 cp)
{
    if (cp & FONT_CP_GBK)
        return _FontCjkLookupGbk(cp & 0xFFFF);
    return _FontCjkLookupCp(cp);
}

static Int32 _FontAdvLogical(Int32 physW)
{
    float sx = g_scaleX;
    if (sx <= 0.0f) sx = 1.0f;
    return (Int32)(((float)physW) / sx + 0.5f);
}

/* ASCII advance approximation: m5x7 glyphs are ~5-6px wide + 2 gap.
 * Exact per-char values do not matter for this repro; CJK does. */
static Int32 asciiWidth(unsigned char c)
{
    if (c == ' ') return _FontAdvLogical(4);
    return _FontAdvLogical(5 + 2);
}

static Int32 FontGetStrWidth(const Char *pStr)
{
    Int32 iWidth = 0;
    while (*pStr)
    {
        Int32 len;
        Uint32 cp = _FontDecode(pStr, &len);
        if (cp == ' ')
            iWidth += _FontAdvLogical(4);
        else if (cp < 0x80)
            iWidth += asciiWidth((unsigned char)cp);
        else
        {
            Int32 gi = _FontCjkResolve(cp);
            if (gi >= 0)
                iWidth += _FontAdvLogical(FONT_CJK_ADV);
            else
                iWidth += _FontAdvLogical(6);
        }
        pStr += len;
    }
    return iWidth;
}

/* --- exact copy of uiBrowser.cpp BrowserCopyEllipsis --------------- */
static void BrowserCopyEllipsis(Char *out, size_t out_size, const Char *src, Int32 max_px)
{
    if (!out || out_size == 0) return;
    if (!src) { out[0] = '\0'; return; }

    snprintf(out, out_size, "%s", src);
    if (FontGetStrWidth(out) <= max_px)
        return;

    Char dots[] = "\xE2\x80\xA6";
    Int32 dotsW = FontGetStrWidth(dots);

    if (dotsW > max_px)
    {
        size_t i = 0;
        while (src[i] && i + 1 < out_size)
        {
            Int32 clen = FontCharLen(src + i);
            if (clen <= 0 || i + (size_t)clen >= out_size) break;
            memcpy(out + i, src + i, (size_t)clen);
            out[i + clen] = '\0';
            if (FontGetStrWidth(out) > max_px)
            {
                if (i > 0) out[i] = '\0';
                break;
            }
            i += (size_t)clen;
        }
        return;
    }

    {
        size_t i = 0;
        out[0] = '\0';
        while (src[i] && i + 4 < out_size)
        {
            Int32 clen = FontCharLen(src + i);
            if (clen <= 0 || i + (size_t)clen + 1 >= out_size) break;
            memcpy(out + i, src + i, (size_t)clen);
            out[i + clen] = '\0';
            if (FontGetStrWidth(out) > max_px - dotsW)
            {
                if (i > 0) out[i] = '\0';
                break;
            }
            i += (size_t)clen;
        }
        {
            size_t n = strlen(out);
            size_t dl = strlen(dots);
            if (n + dl < out_size)
                memcpy(out + n, dots, dl + 1);
        }
    }
}

/* --- exact copy of BrowserCopyMarquee ------------------------------- */
static void BrowserCopyMarquee(Char *out, size_t out_size, const Char *src,
                               Int32 max_px, Uint32 tick)
{
    if (!out || out_size == 0) return;
    if (!src) { out[0] = '\0'; return; }

    snprintf(out, out_size, "%s", src);
    if (FontGetStrWidth(out) <= max_px)
        return;

    Int32 fullW = FontGetStrWidth(src);
    Int32 maxOffset = fullW - max_px;
    if (maxOffset < 0) maxOffset = 0;

    Int32 pxOffset = (Int32)tick;
    if (pxOffset > maxOffset) pxOffset = maxOffset;

    {
        size_t startChar = 0;
        size_t i = 0, idx;

        if (pxOffset >= maxOffset)
        {
            while (src[startChar] && FontGetStrWidth(src + startChar) > max_px)
            {
                Int32 clen = FontCharLen(src + startChar);
                startChar += (clen > 0) ? (size_t)clen : 1;
            }
        }
        else
        {
            Int32 cumW = 0;
            Char  tmp[8] = {0};
            while (src[startChar])
            {
                Int32 clen = FontCharLen(src + startChar);
                if (clen <= 0 || clen > 6) clen = 1;
                memcpy(tmp, src + startChar, (size_t)clen);
                tmp[clen] = '\0';
                Int32 cw = FontGetStrWidth(tmp);
                if (cumW + cw > pxOffset) break;
                cumW += cw;
                startChar += (size_t)clen;
            }
        }

        out[0] = '\0';
        idx = startChar;
        while (src[idx] && i + 1 < out_size)
        {
            Int32 clen = FontCharLen(src + idx);
            if (clen <= 0 || i + (size_t)clen >= out_size) break;
            memcpy(out + i, src + idx, (size_t)clen);
            out[i + clen] = '\0';
            if (FontGetStrWidth(out) > max_px)
            {
                if (i > 0) out[i] = '\0';
                break;
            }
            i += (size_t)clen;
            idx += (size_t)clen;
        }
    }
}

/* --- driver ---------------------------------------------------------- */
static void dump(const char *label, const char *s)
{
    printf("  %-22s [w=%3d] \"", label, FontGetStrWidth(s));
    for (const unsigned char *p = (const unsigned char *)s; *p; p++)
        if (*p >= 0x20 && *p < 0x7F) putchar(*p);
        else printf("\\x%02X", *p);
    printf("\"\n");
}

static void run(const char *name, Int32 max_px, Uint32 tick)
{
    Char out[512];
    char label[128];
    printf("\n=== max_px=%d  (scaleX=%.2f)\n", max_px, g_scaleX);
    dump("input:", name);

    BrowserCopyEllipsis(out, sizeof(out), name, max_px);
    dump("ellipsis:", out);

    BrowserCopyMarquee(out, sizeof(out), name, max_px, tick);
    snprintf(label, sizeof(label), "marquee(tick=%u):", tick);
    dump(label, out);
}

int main()
{
    /* UTF-8 long Chinese ROM name */
    const char *utf8 = "马里奥世界超级大冒险合集完美中文版.sfc";
    /* GBK long Chinese ROM name */
    const char *gbk = "\xC2\xED\xC0\xEF\xB0\xC2\xCA\xC0\xBD\xE7\xB3\xAC\xBC\xB6\xB4\xF3\xC3\xB0\xCF\xD5.sfc";

    printf("################ 240p (sx=1.0) ################");
    g_scaleX = g_scaleY = 1.0f;
    run(utf8, 240, 0);
    run(utf8, 240, 60);
    run(utf8, 148, 0);
    run(gbk, 240, 0);

    printf("\n################ 480i (sx=2.5) ################");
    g_scaleX = 2.5f; g_scaleY = 2.0f;
    run(utf8, 240, 0);
    run(gbk, 240, 0);

    return 0;
}
