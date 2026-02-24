#include <stdlib.h>

#include "test_framework.h"

#include "gif_parser.h"

void test_gif_parser_invalid_file(void) {
    ASSERT_FALSE(GifGetDimensions("nonexistent.gif", &(int){0}, &(int){0}));

    system("echo 'not a gif' > temp_not_gif.gif");
    ASSERT_FALSE(GifGetDimensions("temp_not_gif.gif", &(int){0}, &(int){0}));
    system("rm temp_not_gif.gif");
}

void test_gif_parser_dimensions(void) {
    int width = 0;
    int height = 0;

    unsigned char minimal_gif[] = {
        0x47, 0x49, 0x46, 0x38, 0x39, 0x61,
        0x01, 0x00, 0x01, 0x00,
        0x80, 0x00, 0x00,
        0x00, 0x00, 0x00,
        0x21, 0xF9, 0x04, 0x01, 0x00, 0x00, 0x00,
        0x2C, 0x00, 0x00, 0x00, 0x00,
        0x01, 0x00, 0x01, 0x00,
        0x00, 0x02, 0x01, 0x00,
        0x00
    };

    FILE *f = fopen("temp_test.gif", "wb");
    if (f) {
        fwrite(minimal_gif, 1, sizeof(minimal_gif), f);
        fclose(f);
    }

    ASSERT_TRUE(GifGetDimensions("temp_test.gif", &width, &height));
    ASSERT_EQ(1, width);
    ASSERT_EQ(1, height);

    system("rm temp_test.gif");
}

void register_gif_parser_tests(void) {
    register_test("test_gif_parser_invalid_file",
                  test_gif_parser_invalid_file, "gif");
    register_test("test_gif_parser_dimensions",
                  test_gif_parser_dimensions, "gif");
}
