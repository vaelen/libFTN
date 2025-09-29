/*
 * test_plz_zlib.c - Tests for PLZ compression with real zlib
 * Copyright (c) 2025 Andrew C. Young <andrew@vaelen.org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "ftn/binkp.h"
#include "ftn/binkp/plz.h"

static int test_plz_context_lifecycle(void) {
    ftn_plz_context_t ctx;
    ftn_binkp_error_t result;

    printf("Testing PLZ context lifecycle... ");

    /* Test initialization */
    result = ftn_plz_init(&ctx);
    if (result != BINKP_OK) {
        printf("FAIL: init failed\n");
        return 0;
    }

    /* Test that context is properly initialized */
    if (ctx.plz_enabled != 0 ||
        ctx.plz_negotiated != 0 ||
        ctx.local_mode != PLZ_MODE_NONE ||
        ctx.remote_mode != PLZ_MODE_NONE ||
        ctx.compress_buffer == NULL ||
        ctx.decompress_buffer == NULL) {
        printf("FAIL: invalid initial state\n");
        ftn_plz_free(&ctx);
        return 0;
    }

    /* Test cleanup */
    ftn_plz_free(&ctx);
    printf("PASS\n");
    return 1;
}

static int test_plz_compression_roundtrip(void) {
    ftn_plz_context_t ctx;
    ftn_binkp_error_t result;
    const char* test_data = "This is a test string that should compress well because it has repetitive patterns and common words that zlib can compress effectively.";
    size_t test_len = strlen(test_data);
    uint8_t* compressed = NULL;
    size_t compressed_len = 0;
    uint8_t* decompressed = NULL;
    size_t decompressed_len = 0;

    printf("Testing PLZ compression roundtrip... ");

    /* Initialize context */
    result = ftn_plz_init(&ctx);
    if (result != BINKP_OK) {
        printf("FAIL: init failed\n");
        return 0;
    }

    /* Enable PLZ */
    result = ftn_plz_set_mode(&ctx, PLZ_MODE_SUPPORTED);
    if (result != BINKP_OK) {
        printf("FAIL: set_mode failed\n");
        ftn_plz_free(&ctx);
        return 0;
    }

    /* Simulate negotiation */
    result = ftn_plz_negotiate(&ctx, "PLZ");
    if (result != BINKP_OK) {
        printf("FAIL: negotiate failed\n");
        ftn_plz_free(&ctx);
        return 0;
    }

    /* Test compression */
    result = ftn_plz_compress_data(&ctx, (const uint8_t*)test_data, test_len, &compressed, &compressed_len);
    if (result != BINKP_OK) {
        printf("FAIL: compression failed\n");
        ftn_plz_free(&ctx);
        return 0;
    }

    /* Check that compression actually reduced size */
    if (compressed_len >= test_len) {
        printf("FAIL: compression didn't reduce size (%lu >= %lu)\n",
               (unsigned long)compressed_len, (unsigned long)test_len);
        free(compressed);
        ftn_plz_free(&ctx);
        return 0;
    }

    /* Test decompression */
    result = ftn_plz_decompress_data(&ctx, compressed, compressed_len, &decompressed, &decompressed_len);
    if (result != BINKP_OK) {
        printf("FAIL: decompression failed\n");
        free(compressed);
        ftn_plz_free(&ctx);
        return 0;
    }

    /* Verify roundtrip integrity */
    if (decompressed_len != test_len) {
        printf("FAIL: decompressed length mismatch (%lu != %lu)\n",
               (unsigned long)decompressed_len, (unsigned long)test_len);
        free(compressed);
        free(decompressed);
        ftn_plz_free(&ctx);
        return 0;
    }

    if (memcmp(test_data, decompressed, test_len) != 0) {
        printf("FAIL: decompressed data mismatch\n");
        free(compressed);
        free(decompressed);
        ftn_plz_free(&ctx);
        return 0;
    }

    printf("PASS (compressed %lu -> %lu bytes, ratio: %.1f%%)\n",
           (unsigned long)test_len, (unsigned long)compressed_len, 100.0 * compressed_len / test_len);

    free(compressed);
    free(decompressed);
    ftn_plz_free(&ctx);
    return 1;
}

static int test_plz_compression_levels(void) {
    ftn_plz_context_t ctx;
    ftn_binkp_error_t result;
    const char* test_data = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"; /* Very compressible data */
    size_t test_len = strlen(test_data);
    uint8_t* compressed = NULL;
    size_t compressed_len = 0;
    ftn_plz_level_t levels[] = {PLZ_LEVEL_FAST, PLZ_LEVEL_NORMAL, PLZ_LEVEL_BEST};
    const char* level_names[] = {"FAST", "NORMAL", "BEST"};
    int i;

    printf("Testing PLZ compression levels... ");

    for (i = 0; i < 3; i++) {
        /* Initialize context */
        result = ftn_plz_init(&ctx);
        if (result != BINKP_OK) {
            printf("FAIL: init failed for level %s\n", level_names[i]);
            return 0;
        }

        /* Set mode and level */
        result = ftn_plz_set_mode(&ctx, PLZ_MODE_SUPPORTED);
        if (result != BINKP_OK) {
            printf("FAIL: set_mode failed for level %s\n", level_names[i]);
            ftn_plz_free(&ctx);
            return 0;
        }

        result = ftn_plz_set_level(&ctx, levels[i]);
        if (result != BINKP_OK) {
            printf("FAIL: set_level failed for level %s\n", level_names[i]);
            ftn_plz_free(&ctx);
            return 0;
        }

        /* Simulate negotiation */
        result = ftn_plz_negotiate(&ctx, "PLZ");
        if (result != BINKP_OK) {
            printf("FAIL: negotiate failed for level %s\n", level_names[i]);
            ftn_plz_free(&ctx);
            return 0;
        }

        /* Test compression */
        result = ftn_plz_compress_data(&ctx, (const uint8_t*)test_data, test_len, &compressed, &compressed_len);
        if (result != BINKP_OK) {
            printf("FAIL: compression failed for level %s\n", level_names[i]);
            ftn_plz_free(&ctx);
            return 0;
        }

        /* Should compress very well */
        if (compressed_len >= test_len / 2) {
            printf("FAIL: compression not effective for level %s (%lu >= %lu)\n",
                   level_names[i], (unsigned long)compressed_len, (unsigned long)(test_len / 2));
            free(compressed);
            ftn_plz_free(&ctx);
            return 0;
        }

        free(compressed);
        compressed = NULL;
        ftn_plz_free(&ctx);
    }

    printf("PASS\n");
    return 1;
}

static int test_plz_no_compression_mode(void) {
    ftn_plz_context_t ctx;
    ftn_binkp_error_t result;
    const char* test_data = "Test data";
    size_t test_len = strlen(test_data);
    uint8_t* output = NULL;
    size_t output_len = 0;

    printf("Testing PLZ no compression mode... ");

    /* Initialize context */
    result = ftn_plz_init(&ctx);
    if (result != BINKP_OK) {
        printf("FAIL: init failed\n");
        return 0;
    }

    /* Keep in NONE mode - no negotiation */

    /* Test "compression" (should just copy) */
    result = ftn_plz_compress_data(&ctx, (const uint8_t*)test_data, test_len, &output, &output_len);
    if (result != BINKP_OK) {
        printf("FAIL: compress_data failed\n");
        ftn_plz_free(&ctx);
        return 0;
    }

    /* Should be identical */
    if (output_len != test_len || memcmp(test_data, output, test_len) != 0) {
        printf("FAIL: data not copied correctly\n");
        free(output);
        ftn_plz_free(&ctx);
        return 0;
    }

    free(output);
    ftn_plz_free(&ctx);
    printf("PASS\n");
    return 1;
}

int main(void) {
    int passed = 0;
    int total = 0;

    printf("PLZ Zlib Integration Tests\n");
    printf("==========================\n\n");

    total++; if (test_plz_context_lifecycle()) passed++;
    total++; if (test_plz_compression_roundtrip()) passed++;
    total++; if (test_plz_compression_levels()) passed++;
    total++; if (test_plz_no_compression_mode()) passed++;

    printf("\nTest Results: %d/%d tests passed\n", passed, total);

    if (passed == total) {
        printf("All tests PASSED!\n");
        return 0;
    } else {
        printf("Some tests FAILED!\n");
        return 1;
    }
}