#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "ml_provider.h"

bool MlBuildProviderRawJson(const MlResult *result, char **out_json) {
  if (!result || !out_json) {
    return false;
  }

  *out_json = NULL;

  cJSON *root = cJSON_CreateObject();
  if (!root) {
    return false;
  }

  cJSON_AddBoolToObject(root, "hasClassification", result->has_classification);
  cJSON_AddBoolToObject(root, "hasTextDetection", result->has_text_detection);
  cJSON_AddNumberToObject(root, "latencyMs", result->provider_latency_ms);

  if (result->model_id_classification[0] != '\0') {
    cJSON_AddStringToObject(root, "modelClassification",
                            result->model_id_classification);
  }
  if (result->model_id_text_detection[0] != '\0') {
    cJSON_AddStringToObject(root, "modelTextDetection",
                            result->model_id_text_detection);
  }

  cJSON *classes = cJSON_CreateArray();
  if (!classes) {
    cJSON_Delete(root);
    return false;
  }

  for (int i = 0; i < result->topk_count; i++) {
    cJSON *entry = cJSON_CreateObject();
    if (!entry) {
      cJSON_Delete(root);
      return false;
    }
    cJSON_AddStringToObject(entry, "label", result->topk[i].label);
    cJSON_AddNumberToObject(entry, "confidence", result->topk[i].confidence);
    cJSON_AddItemToArray(classes, entry);
  }
  cJSON_AddItemToObject(root, "classificationTopK", classes);

  cJSON *boxes = cJSON_CreateArray();
  if (!boxes) {
    cJSON_Delete(root);
    return false;
  }

  for (int i = 0; i < result->text_box_count; i++) {
    cJSON *box = cJSON_CreateObject();
    if (!box) {
      cJSON_Delete(root);
      return false;
    }
    cJSON_AddNumberToObject(box, "x", result->text_boxes[i].x);
    cJSON_AddNumberToObject(box, "y", result->text_boxes[i].y);
    cJSON_AddNumberToObject(box, "w", result->text_boxes[i].w);
    cJSON_AddNumberToObject(box, "h", result->text_boxes[i].h);
    cJSON_AddNumberToObject(box, "confidence",
                            result->text_boxes[i].confidence);
    cJSON_AddItemToArray(boxes, box);
  }
  cJSON_AddItemToObject(root, "textBoxes", boxes);

  char *raw = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);

  if (!raw) {
    return false;
  }

  *out_json = raw;
  return true;
}
