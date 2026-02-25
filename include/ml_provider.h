#ifndef ML_PROVIDER_H
#define ML_PROVIDER_H

#include <stdbool.h>

#include "ml_api.h"

typedef struct {
  const char *models_root;
} MlProviderConfig;

typedef struct {
  bool (*init)(const MlProviderConfig *config);
  void (*shutdown)(void);
  bool (*infer)(const char *filepath, MlFeature *features, int feature_count,
                MlResult *out);
  const char *(*name)(void);
} MlProviderVTable;

const MlProviderVTable *MlGetOnnxProvider(void);

bool MlBuildProviderRawJson(const MlResult *result, char **out_json);

#endif // ML_PROVIDER_H
