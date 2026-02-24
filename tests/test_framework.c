#include "test_framework.h"

int g_test_count = 0;
int g_fail_count = 0;
Test g_tests[MAX_TESTS];

void register_test(const char *name, test_func func, const char *category) {
  if (g_test_count < MAX_TESTS) {
    g_tests[g_test_count].name = name;
    g_tests[g_test_count].func = func;
    g_tests[g_test_count].category = category;
    g_test_count++;
  } else {
    printf("Error: Max tests reached.\n");
  }
}
