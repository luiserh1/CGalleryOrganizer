#ifndef ML_API_H
#define ML_API_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  ML_FEATURE_CLASSIFICATION = 0,
  ML_FEATURE_TEXT_DETECTION = 1
} MlFeature;

typedef struct {
  char label[128];
  float confidence;
} MlClassificationTopK;

typedef struct {
  float x;
  float y;
  float w;
  float h;
  float confidence;
} MlTextDetectionBox;

typedef struct {
  bool has_classification;
  MlClassificationTopK topk[5];
  int topk_count;

  bool has_text_detection;
  int text_box_count;
  MlTextDetectionBox *text_boxes;

  char *provider_raw_json;
  char model_id_classification[64];
  char model_id_text_detection[64];
  float provider_latency_ms;
} MlResult;

bool MlInit(const char *models_root);
void MlShutdown(void);

bool MlInferImage(const char *filepath, MlFeature *features, int feature_count,
                  MlResult *out);

void MlFreeResult(MlResult *result);

#ifdef __cplusplus
}
#endif

#endif // ML_API_H
