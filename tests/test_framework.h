#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <stdio.h>
#include <string.h>

#define MAX_TESTS 1000

typedef void (*test_func)(void);

typedef struct {
    const char *name;
    test_func func;
    const char *category;
} Test;

extern Test g_tests[MAX_TESTS];
extern int g_test_count;
extern int g_fail_count;

void register_test(const char *name, test_func func, const char *category);

#define ASSERT_TRUE(condition) \
    if (!(condition)) { \
        printf("  FAIL: %s:%d: Expected TRUE, got FALSE\n", __FILE__, __LINE__); \
        g_fail_count++; \
        return; \
    }

#define ASSERT_FALSE(condition) \
    if (condition) { \
        printf("  FAIL: %s:%d: Expected FALSE, got TRUE\n", __FILE__, __LINE__); \
        g_fail_count++; \
        return; \
    }

#define ASSERT_EQ(expected, actual) \
    if ((expected) != (actual)) { \
        printf("  FAIL: %s:%d: Expected %d, got %d\n", __FILE__, __LINE__, (int)(expected), (int)(actual)); \
        g_fail_count++; \
        return; \
    }

#define ASSERT_STR_EQ(expected, actual) \
    if (strcmp((expected), (actual)) != 0) { \
        printf("  FAIL: %s:%d: Expected '%s', got '%s'\n", __FILE__, __LINE__, (expected), (actual)); \
        g_fail_count++; \
        return; \
    }

#endif // TEST_FRAMEWORK_H
