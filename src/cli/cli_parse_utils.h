#ifndef CLI_PARSE_UTILS_H
#define CLI_PARSE_UTILS_H

#include <stdbool.h>

#include "app_api_types.h"

bool CliParseSimIncrementalValue(const char *value, bool *out_sim_incremental);

bool CliParseSimilarityMemoryModeValue(const char *mode,
                                       AppSimilarityMemoryMode *out_mode);

bool CliParseCacheCompressionModeValue(const char *mode,
                                       AppCacheCompressionMode *out_mode);

bool CliParseLongInRange(const char *raw, long min_value, long max_value,
                         long *out_value);

#endif // CLI_PARSE_UTILS_H
