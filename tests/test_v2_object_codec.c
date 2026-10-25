#include "v2/object.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
/* Include the production codec to exercise its non-native conversion helper
 * directly, without a production fault hook or pretending this is a BE host. */
#include "../v2/object.c"

static void check_word_conversion(void)
{
    static const unsigned char wire[] = {0x12,0x34,0x56,0x78, 0xef,0xcd,0xab,0x90,
                                         0xff,0xff,0xff,0xff, 0,0,0,0};
    static const uint32_t expected[] = {0x78563412U,0x90abcdefU,UINT32_MAX,0};
    uint32_t words[4];
    memset(words, 0xa5, sizeof(words));
    decode_words(words, wire, 4);
    assert(!memcmp(words, expected, sizeof(words)));
    memcpy(words, wire, sizeof(words));
    decode_words(words, (const unsigned char *)words, 4);
    assert(!memcmp(words, expected, sizeof(words)));
}

/* A transport-only driver: the Python oracle independently constructs the
 * tables and verifies the exact bytes returned by the production codec. */
int main(int argc, char **argv)
{
    FILE *f;
    long n;
    unsigned char *input, *output = NULL;
    size_t size = 0;
    v2_object *o, *owned;
    v2_object_error error, owned_error;
    check_word_conversion();
    if ((argc != 3 && argc != 4) || !(f = fopen(argv[1], "rb")))
        return 2;
    if (fseek(f, 0, SEEK_END) || (n = ftell(f)) < 0 || fseek(f, 0, SEEK_SET))
        return 2;
    input = malloc(n ? (size_t)n : 1);
    if (!input || fread(input, 1, (size_t)n, f) != (size_t)n || fclose(f))
        return 2;
    o = v2_object_decode(input, (size_t)n, &error);
    owned = v2_object_decode_owned(input, (size_t)n, &owned_error);
    if ((o == NULL) != (owned == NULL) || error != owned_error)
    {
        v2_object_destroy(o);
        v2_object_destroy(owned);
        return 2;
    }
    if (o)
    {
        unsigned char *copy_bytes = NULL, *owned_bytes = NULL;
        size_t copy_size = 0, owned_size = 0;
        int same = v2_object_encode(o, &copy_bytes, &copy_size) &&
                   v2_object_encode(owned, &owned_bytes, &owned_size) && copy_size == owned_size &&
                   !memcmp(copy_bytes, owned_bytes, copy_size);
        free(copy_bytes);
        free(owned_bytes);
        v2_object_destroy(o);
        o = owned;
        if (!same)
        {
            v2_object_destroy(o);
            return 2;
        }
    }
    if (!o)
    {
        fprintf(stderr, "%s\n", v2_object_error_message(error));
        return 1;
    }
    if (argc == 4)
    {
        uint32_t location[5];
        memcpy(location, v2_object_record(o, V2_OBJ_LOCATIONS, 0), sizeof(location));
        if (!strcmp(argv[3], "corrupt"))
        {
            location[4] = 0;
            if (!v2_object_set(o, V2_OBJ_LOCATIONS, 0, location) || v2_object_validate(o))
                return 2;
            v2_object_destroy(o);
            return 1;
        }
        if (!strcmp(argv[3], "add-blob"))
        {
            size_t length;
            const unsigned char *borrowed = v2_object_bytes(o, 0, &length);
            if (v2_object_blob(o, borrowed, length) == V2_NONE)
                return 2;
        }
        else
        {
            if (strcmp(argv[3], "mutate"))
                return 2;
            location[4]++;
            if (v2_object_add(o, V2_OBJ_FILES, v2_object_record(o, V2_OBJ_FILES, 0)) == V2_NONE ||
                !v2_object_set(o, V2_OBJ_LOCATIONS, 0, location) ||
                !v2_object_append_bytes(o, 2, v2_object_bytes(o, 2, NULL), 1))
                return 2;
        }
    }
    if (!v2_object_encode(o, &output, &size))
    {
        v2_object_destroy(o);
        return 2;
    }
    v2_object_destroy(o);
    f = fopen(argv[2], "wb");
    if (!f || fwrite(output, 1, size, f) != size || fclose(f))
    {
        free(output);
        return 2;
    }
    free(output);
    return 0;
}
