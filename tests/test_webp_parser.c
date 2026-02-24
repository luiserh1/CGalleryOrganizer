#include <stdlib.h>

#include "test_framework.h"

#include "webp_parser.h"

void test_webp_parser_invalid_file(void) {
    ASSERT_FALSE(WebpGetDimensions("nonexistent.webp", &(int){0}, &(int){0}));

    system("echo 'not a webp' > temp_not_webp.webp");
    ASSERT_FALSE(WebpGetDimensions("temp_not_webp.webp", &(int){0}, &(int){0}));
    system("rm temp_not_webp.webp");
}

void test_webp_parser_dimensions(void) {
    int width = 0;
    int height = 0;

    system("touch temp_test.webp");
    ASSERT_FALSE(WebpGetDimensions("temp_test.webp", &width, &height));

    system("rm temp_test.webp");
}

void register_webp_parser_tests(void) {
    register_test("test_webp_parser_invalid_file",
                  test_webp_parser_invalid_file, "webp");
    register_test("test_webp_parser_dimensions",
                  test_webp_parser_dimensions, "webp");
}
