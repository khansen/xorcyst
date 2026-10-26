/* Shared comparison of two not-yet-created names in the same directory.
 * Existing-file identity is checked by each caller before reaching this code. */
#ifndef XORCYST_IR_PATH_H
#define XORCYST_IR_PATH_H
#include <errno.h>
#include <string.h>
#include <unistd.h>
#ifdef __APPLE__
#include <dlfcn.h>
#include <CoreFoundation/CoreFoundation.h>
#endif
static int ir_same_future_name(const char *parent, const char *a, const char *b,
                               const char **error)
{
    *error = NULL;
    if (!strcmp(a, b)) return 1;
#ifdef __APPLE__
    long sensitive;
    CFStringRef left, right;
    CFStringCompareFlags flags = kCFCompareNonliteral;
    const unsigned char *p;
    int ascii = 1, same;
    static void *library;
    static CFStringRef (*make_string)(CFAllocatorRef, const char *, CFStringEncoding);
    static CFComparisonResult (*compare)(CFStringRef, CFStringRef, CFStringCompareFlags);
    static void (*release)(CFTypeRef);
    errno = 0;
    sensitive = pathconf(parent, _PC_CASE_SENSITIVE);
    if (sensitive < 0 && errno) {
        *error = "cannot determine output filesystem case sensitivity";
        return 1;
    }
    if (sensitive <= 0) flags |= kCFCompareCaseInsensitive;
    for (p = (const unsigned char *)a; *p; ++p) if (*p >= 128) ascii = 0;
    for (p = (const unsigned char *)b; *p; ++p) if (*p >= 128) ascii = 0;
    /* ASCII needs no canonical decomposition. Any non-ASCII name, including
     * a Unicode alias of an ASCII letter, uses the system comparison below. */
    if (ascii) {
        const unsigned char *x = (const unsigned char *)a, *y = (const unsigned char *)b;
        while (*x && *y) {
            unsigned l = *x++, r = *y++;
            if (sensitive <= 0) {
                if (l >= 'A' && l <= 'Z') l += 'a' - 'A';
                if (r >= 'A' && r <= 'Z') r += 'a' - 'A';
            }
            if (l != r) return 0;
        }
        return *x == *y;
    }
    if (!library) {
        library = dlopen("/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation",
                         RTLD_LOCAL | RTLD_LAZY);
        if (library) {
            make_string = (CFStringRef (*)(CFAllocatorRef, const char *, CFStringEncoding))
                dlsym(library, "CFStringCreateWithCString");
            compare = (CFComparisonResult (*)(CFStringRef, CFStringRef, CFStringCompareFlags))
                dlsym(library, "CFStringCompare");
            release = (void (*)(CFTypeRef))dlsym(library, "CFRelease");
        }
    }
    if (!make_string || !compare || !release) {
        *error = "cannot initialize Unicode output comparison";
        return 1;
    }
    left = make_string(NULL, a, kCFStringEncodingUTF8);
    right = make_string(NULL, b, kCFStringEncodingUTF8);
    same = !left || !right || compare(left, right, flags) == kCFCompareEqualTo;
    if (!left || !right) *error = "out of memory comparing output names";
    if (left) release(left);
    if (right) release(right);
    return same;
#else
    (void)parent;
    return 0;
#endif
}
#endif
