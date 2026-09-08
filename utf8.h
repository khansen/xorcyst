#ifndef XASM_UTF8_H
#define XASM_UTF8_H

#include <stddef.h>

static int xasm_utf8_valid(const char *text, size_t length)
{
    const unsigned char *bytes = (const unsigned char *)text;
    size_t i = 0;
    while (i < length) {
        unsigned int codepoint, minimum;
        unsigned char first = bytes[i++];
        size_t remaining;
        if (first < 0x80) continue;
        if (first >= 0xc2 && first <= 0xdf) {
            remaining = 1; codepoint = first & 0x1f; minimum = 0x80;
        } else if (first >= 0xe0 && first <= 0xef) {
            remaining = 2; codepoint = first & 0x0f; minimum = 0x800;
        } else if (first >= 0xf0 && first <= 0xf4) {
            remaining = 3; codepoint = first & 0x07; minimum = 0x10000;
        } else {
            return 0;
        }
        if (remaining > length - i) return 0;
        while (remaining-- > 0) {
            unsigned char continuation = bytes[i++];
            if ((continuation & 0xc0) != 0x80) return 0;
            codepoint = (codepoint << 6) | (continuation & 0x3f);
        }
        if (codepoint < minimum || codepoint > 0x10ffff
            || (codepoint >= 0xd800 && codepoint <= 0xdfff)) return 0;
    }
    return 1;
}

#endif
