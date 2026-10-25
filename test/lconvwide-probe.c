/* lconvwide-probe: msvcrt.dll's localeconv() has the wide members (_W_decimal_point and on)
 * after the narrow ones, as Python's _locale module (built for msvcrt.dll, as OpenShot ships it)
 * reads them. Prints name=value lines. */
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <locale.h>

struct lconv_full {
    char *decimal_point, *thousands_sep, *grouping, *int_curr_symbol, *currency_symbol,
         *mon_decimal_point, *mon_thousands_sep, *mon_grouping, *positive_sign, *negative_sign;
    char int_frac_digits, frac_digits, p_cs_precedes, p_sep_by_space, n_cs_precedes,
         n_sep_by_space, p_sign_posn, n_sign_posn;
    wchar_t *W_decimal_point, *W_thousands_sep, *W_int_curr_symbol, *W_currency_symbol,
            *W_mon_decimal_point, *W_mon_thousands_sep, *W_positive_sign, *W_negative_sign;
};

static int wis(const wchar_t *p, const wchar_t *want)
{
    /* the pointer must be a real string (not a byte of narrow data read as a pointer) */
    if ((unsigned long long)(size_t)p < 0x10000) return 0;
    return !wcscmp(p, want);
}

int main(void)
{
    struct lconv_full *lc;

    setlocale(LC_ALL, "C");
    lc = (struct lconv_full *)localeconv();
    printf("c_wide=%d\n", wis(lc->W_decimal_point, L".") && wis(lc->W_thousands_sep, L"") &&
           wis(lc->W_int_curr_symbol, L"") && wis(lc->W_currency_symbol, L"") &&
           wis(lc->W_mon_decimal_point, L"") && wis(lc->W_mon_thousands_sep, L"") &&
           wis(lc->W_positive_sign, L"") && wis(lc->W_negative_sign, L""));
    if (!setlocale(LC_ALL, "German_Germany.1252")) { printf("german=0\n"); return 0; }
    lc = (struct lconv_full *)localeconv();
    printf("german_narrow=%d\n", !strcmp(lc->decimal_point, ",") && !strcmp(lc->thousands_sep, "."));
    printf("german_wide=%d\n", wis(lc->W_decimal_point, L",") && wis(lc->W_thousands_sep, L".") &&
           wis(lc->W_currency_symbol, L"\x20ac") && wis(lc->W_mon_decimal_point, L","));
    /* a numeric-only change keeps the monetary wide members and swaps the numeric ones */
    setlocale(LC_NUMERIC, "C");
    lc = (struct lconv_full *)localeconv();
    printf("mixed_wide=%d\n", wis(lc->W_decimal_point, L".") && wis(lc->W_currency_symbol, L"\x20ac"));
    printf("done=1\n");
    return 0;
}
