#ifndef CLI_COMMANDS_H
#define CLI_COMMANDS_H

#include <stdbool.h>

bool CliBuildSimilarityReportFromCache(const char *report_path, float threshold,
                                       int topk);

#endif // CLI_COMMANDS_H
