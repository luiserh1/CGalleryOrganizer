#include <stdlib.h>

#include "test_framework.h"

#include "bmp_parser.h"

void test_bmp_parser_invalid_file(void) {
    ASSERT_FALSE(BmpGetDimensions("nonexistent.bmp", &(int){0}, &(int){0}));

    system("echo 'not a bmp' > temp_not_bmp.bmp");
    ASSERT_FALSE(BmpGetDimensions("temp_not_bmp.bmp", &(int){0}, &(int){0}));
    system("rm temp_not_bmp.bmp");
}

void test_bmp_parser_dimensions(void) {
    int width = 0;
    int height = 0;

    system("touch temp_test.bmp");
    ASSERT_FALSE(BmpGetDimensions("temp_test.bmp", &width, &height));

    system("rm temp_test.bmp");
}

void register_bmp_parser_tests(void) {
    register_test("test_bmp_parser_invalid_file",
                  test_bmp_parser_invalid_file, "bmp");
    register_test("test_bmp_parser_dimensions",
                  test_bmp_parser_dimensions, "bmp");
}
