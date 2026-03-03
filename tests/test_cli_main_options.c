#include <stdbool.h>
#include <string.h>

#include "cli/cli_main_options.h"
#include "cli/cli_parse_utils.h"
#include "test_framework.h"

static bool ParseArgs(const char *const *args, int argc, CliMainOptions *options,
                      CliRenameHistoryFilter *filter, int *exit_code) {
  char *argv_local[64] = {0};
  if (!args || argc <= 0 || argc > (int)(sizeof(argv_local) / sizeof(argv_local[0]))) {
    return false;
  }

  for (int i = 0; i < argc; i++) {
    argv_local[i] = (char *)args[i];
  }

  return CliParseMainOptions(options, filter, argc, argv_local, exit_code);
}

void test_cli_main_options_defaults_and_positionals(void) {
  const char *args[] = {"gallery_organizer", "src_dir", "env_dir"};
  CliMainOptions options;
  CliRenameHistoryFilter filter;
  int exit_code = -1;

  bool ok = ParseArgs(args, 3, &options, &filter, &exit_code);
  ASSERT_TRUE(ok);
  ASSERT_EQ(0, exit_code);
  ASSERT_TRUE(options.exhaustive == false);
  ASSERT_TRUE(options.sim_incremental == true);
  ASSERT_TRUE(options.sim_threshold == 0.92f);
  ASSERT_EQ(5, options.sim_topk);
  ASSERT_STR_EQ("src_dir", options.target_dir);
  ASSERT_STR_EQ("env_dir", options.env_dir);
  ASSERT_EQ(0, options.rename_action_count);
  ASSERT_TRUE(CliMainHasRenameAction(&options) == false);
  ASSERT_EQ(CLI_RENAME_ROLLBACK_FILTER_ANY, filter.rollback_filter);
}

void test_cli_main_options_help_and_unknown_option(void) {
  {
    const char *args[] = {"gallery_organizer", "--help"};
    CliMainOptions options;
    CliRenameHistoryFilter filter;
    int exit_code = -1;
    bool ok = ParseArgs(args, 2, &options, &filter, &exit_code);
    ASSERT_FALSE(ok);
    ASSERT_EQ(0, exit_code);
  }

  {
    const char *args[] = {"gallery_organizer", "--nope"};
    CliMainOptions options;
    CliRenameHistoryFilter filter;
    int exit_code = -1;
    bool ok = ParseArgs(args, 2, &options, &filter, &exit_code);
    ASSERT_FALSE(ok);
    ASSERT_EQ(1, exit_code);
  }
}

void test_cli_main_options_similarity_and_cache_modes(void) {
  const char *args[] = {"gallery_organizer",
                        "src_dir",
                        "env_dir",
                        "--sim-incremental",
                        "off",
                        "--sim-threshold",
                        "0.5",
                        "--sim-topk",
                        "9",
                        "--sim-memory-mode",
                        "eager",
                        "--cache-compress",
                        "auto",
                        "--cache-compress-level",
                        "7"};
  CliMainOptions options;
  CliRenameHistoryFilter filter;
  int exit_code = -1;

  bool ok = ParseArgs(args, (int)(sizeof(args) / sizeof(args[0])), &options,
                      &filter, &exit_code);
  ASSERT_TRUE(ok);
  ASSERT_EQ(0, exit_code);
  ASSERT_FALSE(options.sim_incremental);
  ASSERT_TRUE(options.sim_threshold == 0.5f);
  ASSERT_EQ(9, options.sim_topk);
  ASSERT_EQ(APP_SIM_MEMORY_EAGER, options.sim_memory_mode);
  ASSERT_EQ(APP_CACHE_COMPRESSION_AUTO, options.cache_compression_mode);
  ASSERT_EQ(7, options.cache_compression_level);
  ASSERT_TRUE(options.cache_compression_level_set);
}

void test_cli_main_options_invalid_numeric_and_mode_values(void) {
  {
    const char *args[] = {"gallery_organizer", "src", "--sim-threshold", "1.2"};
    CliMainOptions options;
    CliRenameHistoryFilter filter;
    int exit_code = -1;
    bool ok = ParseArgs(args, 4, &options, &filter, &exit_code);
    ASSERT_FALSE(ok);
    ASSERT_EQ(1, exit_code);
  }

  {
    const char *args[] = {"gallery_organizer", "src", "--sim-topk", "0"};
    CliMainOptions options;
    CliRenameHistoryFilter filter;
    int exit_code = -1;
    bool ok = ParseArgs(args, 4, &options, &filter, &exit_code);
    ASSERT_FALSE(ok);
    ASSERT_EQ(1, exit_code);
  }

  {
    const char *args[] = {"gallery_organizer", "src", "--sim-memory-mode", "bad"};
    CliMainOptions options;
    CliRenameHistoryFilter filter;
    int exit_code = -1;
    bool ok = ParseArgs(args, 4, &options, &filter, &exit_code);
    ASSERT_FALSE(ok);
    ASSERT_EQ(1, exit_code);
  }

  {
    const char *args[] = {"gallery_organizer", "src", "--cache-compress", "zip"};
    CliMainOptions options;
    CliRenameHistoryFilter filter;
    int exit_code = -1;
    bool ok = ParseArgs(args, 4, &options, &filter, &exit_code);
    ASSERT_FALSE(ok);
    ASSERT_EQ(1, exit_code);
  }

  {
    const char *args[] = {"gallery_organizer", "src", "--cache-compress-level",
                          "20"};
    CliMainOptions options;
    CliRenameHistoryFilter filter;
    int exit_code = -1;
    bool ok = ParseArgs(args, 4, &options, &filter, &exit_code);
    ASSERT_FALSE(ok);
    ASSERT_EQ(1, exit_code);
  }
}

void test_cli_main_options_non_rename_conflicts(void) {
  {
    const char *args[] = {"gallery_organizer", "src", "--rollback", "--organize"};
    CliMainOptions options;
    CliRenameHistoryFilter filter;
    int exit_code = -1;
    bool ok = ParseArgs(args, 4, &options, &filter, &exit_code);
    ASSERT_FALSE(ok);
    ASSERT_EQ(1, exit_code);
  }

  {
    const char *args[] = {"gallery_organizer", "src", "--organize", "--preview"};
    CliMainOptions options;
    CliRenameHistoryFilter filter;
    int exit_code = -1;
    bool ok = ParseArgs(args, 4, &options, &filter, &exit_code);
    ASSERT_FALSE(ok);
    ASSERT_EQ(1, exit_code);
  }

  {
    const char *args[] = {"gallery_organizer", "src", "--duplicates-report",
                          "--duplicates-move"};
    CliMainOptions options;
    CliRenameHistoryFilter filter;
    int exit_code = -1;
    bool ok = ParseArgs(args, 4, &options, &filter, &exit_code);
    ASSERT_FALSE(ok);
    ASSERT_EQ(1, exit_code);
  }

  {
    const char *args[] = {"gallery_organizer", "src", "--similarity-report",
                          "--preview"};
    CliMainOptions options;
    CliRenameHistoryFilter filter;
    int exit_code = -1;
    bool ok = ParseArgs(args, 4, &options, &filter, &exit_code);
    ASSERT_FALSE(ok);
    ASSERT_EQ(1, exit_code);
  }
}

void test_cli_main_options_rename_rules_and_history_filters(void) {
  {
    const char *args[] = {"gallery_organizer", "env", "--rename-apply"};
    CliMainOptions options;
    CliRenameHistoryFilter filter;
    int exit_code = -1;
    bool ok = ParseArgs(args, 3, &options, &filter, &exit_code);
    ASSERT_FALSE(ok);
    ASSERT_EQ(1, exit_code);
  }

  {
    const char *args[] = {"gallery_organizer", "env", "--rename-from-preview",
                          "rp-1"};
    CliMainOptions options;
    CliRenameHistoryFilter filter;
    int exit_code = -1;
    bool ok = ParseArgs(args, 4, &options, &filter, &exit_code);
    ASSERT_FALSE(ok);
    ASSERT_EQ(1, exit_code);
  }

  {
    const char *args[] = {"gallery_organizer", "env", "--rename-accept-auto-suffix"};
    CliMainOptions options;
    CliRenameHistoryFilter filter;
    int exit_code = -1;
    bool ok = ParseArgs(args, 3, &options, &filter, &exit_code);
    ASSERT_FALSE(ok);
    ASSERT_EQ(1, exit_code);
  }

  {
    const char *args[] = {"gallery_organizer", "env", "--rename-preview-json"};
    CliMainOptions options;
    CliRenameHistoryFilter filter;
    int exit_code = -1;
    bool ok = ParseArgs(args, 3, &options, &filter, &exit_code);
    ASSERT_FALSE(ok);
    ASSERT_EQ(1, exit_code);
  }

  {
    const char *args[] = {"gallery_organizer", "env", "--rename-pattern",
                          "x-[index].[format]"};
    CliMainOptions options;
    CliRenameHistoryFilter filter;
    int exit_code = -1;
    bool ok = ParseArgs(args, 4, &options, &filter, &exit_code);
    ASSERT_FALSE(ok);
    ASSERT_EQ(1, exit_code);
  }

  {
    const char *args[] = {"gallery_organizer", "env", "--rename-history-id-prefix",
                          "rop-2026"};
    CliMainOptions options;
    CliRenameHistoryFilter filter;
    int exit_code = -1;
    bool ok = ParseArgs(args, 4, &options, &filter, &exit_code);
    ASSERT_FALSE(ok);
    ASSERT_EQ(1, exit_code);
  }

  {
    const char *args[] = {"gallery_organizer",
                          "env",
                          "--rename-history",
                          "--rename-history-id-prefix",
                          "rop-2026",
                          "--rename-history-rollback",
                          "yes",
                          "--rename-history-from",
                          "2026-03-01",
                          "--rename-history-to",
                          "2026-03-04"};
    CliMainOptions options;
    CliRenameHistoryFilter filter;
    int exit_code = -1;
    bool ok = ParseArgs(args, (int)(sizeof(args) / sizeof(args[0])), &options,
                        &filter, &exit_code);
    ASSERT_TRUE(ok);
    ASSERT_EQ(0, exit_code);
    ASSERT_TRUE(CliMainHasRenameAction(&options));
    ASSERT_STR_EQ("rop-2026", filter.operation_id_prefix);
    ASSERT_EQ(CLI_RENAME_ROLLBACK_FILTER_YES, filter.rollback_filter);
    ASSERT_TRUE(strstr(filter.created_from_utc, "2026-03-01") != NULL);
    ASSERT_TRUE(strstr(filter.created_to_utc, "2026-03-04") != NULL);
  }

  {
    const char *args[] = {"gallery_organizer", "env", "--rename-history",
                          "--rename-history-from", "2026-03-05",
                          "--rename-history-to", "2026-03-04"};
    CliMainOptions options;
    CliRenameHistoryFilter filter;
    int exit_code = -1;
    bool ok = ParseArgs(args, 7, &options, &filter, &exit_code);
    ASSERT_FALSE(ok);
    ASSERT_EQ(1, exit_code);
  }
}

void test_cli_main_options_rename_action_exclusivity(void) {
  {
    const char *args[] = {"gallery_organizer", "env", "--rename-preview",
                          "--rename-history"};
    CliMainOptions options;
    CliRenameHistoryFilter filter;
    int exit_code = -1;
    bool ok = ParseArgs(args, 4, &options, &filter, &exit_code);
    ASSERT_FALSE(ok);
    ASSERT_EQ(1, exit_code);
  }

  {
    const char *args[] = {"gallery_organizer", "env", "--rename-apply",
                          "--rename-apply-latest", "--rename-from-preview",
                          "rp-1"};
    CliMainOptions options;
    CliRenameHistoryFilter filter;
    int exit_code = -1;
    bool ok = ParseArgs(args, 6, &options, &filter, &exit_code);
    ASSERT_FALSE(ok);
    ASSERT_EQ(1, exit_code);
  }

  {
    const char *args[] = {"gallery_organizer", "--rename-init", "src_only"};
    CliMainOptions options;
    CliRenameHistoryFilter filter;
    int exit_code = -1;
    bool ok = ParseArgs(args, 3, &options, &filter, &exit_code);
    ASSERT_FALSE(ok);
    ASSERT_EQ(1, exit_code);
  }
}

void test_cli_parse_utils_helpers(void) {
  bool sim_incremental = false;
  ASSERT_TRUE(CliParseSimIncrementalValue("on", &sim_incremental));
  ASSERT_TRUE(sim_incremental);
  ASSERT_TRUE(CliParseSimIncrementalValue("off", &sim_incremental));
  ASSERT_FALSE(sim_incremental);
  ASSERT_FALSE(CliParseSimIncrementalValue("maybe", &sim_incremental));

  AppSimilarityMemoryMode memory_mode = APP_SIM_MEMORY_CHUNKED;
  ASSERT_TRUE(CliParseSimilarityMemoryModeValue("eager", &memory_mode));
  ASSERT_EQ(APP_SIM_MEMORY_EAGER, memory_mode);
  ASSERT_TRUE(CliParseSimilarityMemoryModeValue("chunked", &memory_mode));
  ASSERT_EQ(APP_SIM_MEMORY_CHUNKED, memory_mode);
  ASSERT_FALSE(CliParseSimilarityMemoryModeValue("x", &memory_mode));

  AppCacheCompressionMode compression_mode = APP_CACHE_COMPRESSION_NONE;
  ASSERT_TRUE(CliParseCacheCompressionModeValue("none", &compression_mode));
  ASSERT_EQ(APP_CACHE_COMPRESSION_NONE, compression_mode);
  ASSERT_TRUE(CliParseCacheCompressionModeValue("zstd", &compression_mode));
  ASSERT_EQ(APP_CACHE_COMPRESSION_ZSTD, compression_mode);
  ASSERT_TRUE(CliParseCacheCompressionModeValue("auto", &compression_mode));
  ASSERT_EQ(APP_CACHE_COMPRESSION_AUTO, compression_mode);
  ASSERT_FALSE(CliParseCacheCompressionModeValue("zip", &compression_mode));

  long value = 0;
  ASSERT_TRUE(CliParseLongInRange("10", 1, 20, &value));
  ASSERT_EQ(10, value);
  ASSERT_FALSE(CliParseLongInRange("0", 1, 20, &value));
  ASSERT_FALSE(CliParseLongInRange("abc", 1, 20, &value));
  ASSERT_FALSE(CliParseLongInRange("10", 20, 1, &value));
}

void register_cli_main_options_tests(void) {
  register_test("test_cli_main_options_defaults_and_positionals",
                test_cli_main_options_defaults_and_positionals, "unit");
  register_test("test_cli_main_options_help_and_unknown_option",
                test_cli_main_options_help_and_unknown_option, "unit");
  register_test("test_cli_main_options_similarity_and_cache_modes",
                test_cli_main_options_similarity_and_cache_modes, "unit");
  register_test("test_cli_main_options_invalid_numeric_and_mode_values",
                test_cli_main_options_invalid_numeric_and_mode_values, "unit");
  register_test("test_cli_main_options_non_rename_conflicts",
                test_cli_main_options_non_rename_conflicts, "unit");
  register_test("test_cli_main_options_rename_rules_and_history_filters",
                test_cli_main_options_rename_rules_and_history_filters, "unit");
  register_test("test_cli_main_options_rename_action_exclusivity",
                test_cli_main_options_rename_action_exclusivity, "unit");
  register_test("test_cli_parse_utils_helpers",
                test_cli_parse_utils_helpers, "unit");
}
