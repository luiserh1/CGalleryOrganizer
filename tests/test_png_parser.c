#include <stdlib.h>

#include "test_framework.h"

#include "png_parser.h"

void test_png_parser_invalid_file(void) {
    ASSERT_FALSE(PngGetDimensions("nonexistent.png", &(int){0}, &(int){0}));

    system("echo 'not a png' > temp_not_png.png");
    ASSERT_FALSE(PngGetDimensions("temp_not_png.png", &(int){0}, &(int){0}));
    system("rm temp_not_png.png");
}

void test_png_parser_dimensions(void) {
    int width = 0;
    int height = 0;

    unsigned char minimal_png[] = {
        0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A,
        0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
        0x08, 0x02, 0x00, 0x00, 0x00, 0x90, 0x77, 0x53,
        0xDE,
        0x00, 0x00, 0x00, 0x0C, 0x49, 0x44, 0x41, 0x54,
        0x08, 0xD7, 0x63, 0xF8, 0xFF, 0xFF, 0xFF, 0x00,
        0x05, 0xFE, 0x02, 0xFE, 0xDC, 0xCC, 0x59, 0xE7,
        0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4E, 0x44,
        0xAE, 0x42, 0x60, 0x82
    };

    FILE *f = fopen("temp_test.png", "wb");
    if (f) {
        fwrite(minimal_png, 1, sizeof(minimal_png), f);
        fclose(f);
    }

    ASSERT_TRUE(PngGetDimensions("temp_test.png", &width, &height));
    ASSERT_EQ(1, width);
    ASSERT_EQ(1, height);

    system("rm temp_test.png");
}

void register_png_parser_tests(void) {
    register_test("test_png_parser_invalid_file",
                  test_png_parser_invalid_file, "png");
    register_test("test_png_parser_dimensions",
                  test_png_parser_dimensions, "png");
}
