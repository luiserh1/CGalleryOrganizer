#include <stdlib.h>

#include "test_framework.h"

#include "heic_parser.h"

void test_heic_parser_invalid_file(void) {
    ASSERT_FALSE(HeicGetDimensions("nonexistent.heic", &(int){0}, &(int){0}));

    system("echo 'not a heic' > temp_not_heic.heic");
    ASSERT_FALSE(HeicGetDimensions("temp_not_heic.heic", &(int){0}, &(int){0}));
    system("rm temp_not_heic.heic");
}

void test_heic_parser_dimensions(void) {
    int width = 0;
    int height = 0;

    system("touch temp_test.heic");
    ASSERT_FALSE(HeicGetDimensions("temp_test.heic", &width, &height));

    system("rm temp_test.heic");
}

void register_heic_parser_tests(void) {
    register_test("test_heic_parser_invalid_file",
                  test_heic_parser_invalid_file, "heic");
    register_test("test_heic_parser_dimensions",
                  test_heic_parser_dimensions, "heic");
}
