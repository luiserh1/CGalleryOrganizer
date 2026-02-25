#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "cJSON.h"
#include "similarity_engine.h"

static const char k_b64_table[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static int B64Value(char c) {
  if (c >= 'A' && c <= 'Z') {
    return c - 'A';
  }
  if (c >= 'a' && c <= 'z') {
    return c - 'a' + 26;
  }
  if (c >= '0' && c <= '9') {
    return c - '0' + 52;
  }
  if (c == '+') {
    return 62;
  }
  if (c == '/') {
    return 63;
  }
  return -1;
}

bool SimilarityEncodeEmbeddingBase64(const float *embedding, int dim,
                                     char **out_base64) {
  if (!embedding || dim <= 0 || !out_base64) {
    return false;
  }

  const unsigned char *bytes = (const unsigned char *)embedding;
  size_t byte_len = (size_t)dim * sizeof(float);
  size_t out_len = 4 * ((byte_len + 2) / 3);

  char *out = malloc(out_len + 1);
  if (!out) {
    return false;
  }

  size_t i = 0;
  size_t j = 0;
  while (i < byte_len) {
    unsigned int octet_a = i < byte_len ? bytes[i++] : 0;
    unsigned int octet_b = i < byte_len ? bytes[i++] : 0;
    unsigned int octet_c = i < byte_len ? bytes[i++] : 0;

    unsigned int triple = (octet_a << 16) | (octet_b << 8) | octet_c;
    out[j++] = k_b64_table[(triple >> 18) & 0x3F];
    out[j++] = k_b64_table[(triple >> 12) & 0x3F];
    out[j++] = k_b64_table[(triple >> 6) & 0x3F];
    out[j++] = k_b64_table[triple & 0x3F];
  }

  size_t mod = byte_len % 3;
  if (mod > 0) {
    out[out_len - 1] = '=';
    if (mod == 1) {
      out[out_len - 2] = '=';
    }
  }
  out[out_len] = '\0';

  *out_base64 = out;
  return true;
}

bool SimilarityDecodeEmbeddingBase64(const char *base64, float **out_embedding,
                                     int *out_dim) {
  if (!base64 || !out_embedding || !out_dim) {
    return false;
  }

  size_t len = strlen(base64);
  if (len == 0 || (len % 4) != 0) {
    return false;
  }

  size_t padding = 0;
  if (len >= 1 && base64[len - 1] == '=') {
    padding++;
  }
  if (len >= 2 && base64[len - 2] == '=') {
    padding++;
  }

  size_t decoded_len = (len / 4) * 3 - padding;
  if (decoded_len == 0 || (decoded_len % sizeof(float)) != 0) {
    return false;
  }

  unsigned char *decoded = malloc(decoded_len);
  if (!decoded) {
    return false;
  }

  size_t out_idx = 0;
  for (size_t i = 0; i < len; i += 4) {
    int v0 = B64Value(base64[i]);
    int v1 = B64Value(base64[i + 1]);
    int v2 = (base64[i + 2] == '=') ? -2 : B64Value(base64[i + 2]);
    int v3 = (base64[i + 3] == '=') ? -2 : B64Value(base64[i + 3]);

    if (v0 < 0 || v1 < 0 || v2 == -1 || v3 == -1) {
      free(decoded);
      return false;
    }

    unsigned int triple = ((unsigned int)v0 << 18) | ((unsigned int)v1 << 12) |
                          ((unsigned int)((v2 < 0) ? 0 : v2) << 6) |
                          (unsigned int)((v3 < 0) ? 0 : v3);

    if (out_idx < decoded_len) {
      decoded[out_idx++] = (unsigned char)((triple >> 16) & 0xFF);
    }
    if (v2 >= 0 && out_idx < decoded_len) {
      decoded[out_idx++] = (unsigned char)((triple >> 8) & 0xFF);
    }
    if (v3 >= 0 && out_idx < decoded_len) {
      decoded[out_idx++] = (unsigned char)(triple & 0xFF);
    }
  }

  *out_dim = (int)(decoded_len / sizeof(float));
  *out_embedding = (float *)decoded;
  return true;
}

bool SimilarityCosine(const float *a, const float *b, int dim,
                      float *out_score) {
  if (!a || !b || dim <= 0 || !out_score) {
    return false;
  }

  double dot = 0.0;
  double norm_a = 0.0;
  double norm_b = 0.0;

  for (int i = 0; i < dim; i++) {
    dot += (double)a[i] * (double)b[i];
    norm_a += (double)a[i] * (double)a[i];
    norm_b += (double)b[i] * (double)b[i];
  }

  if (norm_a <= 0.0 || norm_b <= 0.0) {
    return false;
  }

  double denom = sqrt(norm_a) * sqrt(norm_b);
  if (denom <= 0.0) {
    return false;
  }

  *out_score = (float)(dot / denom);
  return true;
}

typedef struct {
  int index;
  float score;
} ScoreIndex;

static int CompareScoreDesc(const void *a, const void *b) {
  const ScoreIndex *sa = (const ScoreIndex *)a;
  const ScoreIndex *sb = (const ScoreIndex *)b;
  if (sa->score < sb->score) {
    return 1;
  }
  if (sa->score > sb->score) {
    return -1;
  }
  return 0;
}

static void FreeTempEmbeddings(float **embeddings, int count) {
  if (!embeddings) {
    return;
  }
  for (int i = 0; i < count; i++) {
    free(embeddings[i]);
  }
  free(embeddings);
}

static void FreeGroupsPartial(SimilarityGroup *groups, int group_count) {
  if (!groups) {
    return;
  }
  for (int i = 0; i < group_count; i++) {
    free(groups[i].anchorPath);
    for (int j = 0; j < groups[i].neighborCount; j++) {
      free(groups[i].neighbors[j].path);
    }
    free(groups[i].neighbors);
  }
  free(groups);
}

bool SimilarityBuildReport(const char *model_id, float threshold, int topk,
                           const ImageMetadata *items, int item_count,
                           SimilarityReport *out_report) {
  if (!model_id || threshold < 0.0f || threshold > 1.0f || topk <= 0 ||
      !out_report || item_count < 0) {
    return false;
  }

  memset(out_report, 0, sizeof(SimilarityReport));
  strncpy(out_report->modelId, model_id, sizeof(out_report->modelId) - 1);
  out_report->threshold = threshold;
  out_report->topK = topk;

  time_t now = time(NULL);
  struct tm tm_utc;
#if defined(_WIN32)
  gmtime_s(&tm_utc, &now);
#else
  gmtime_r(&now, &tm_utc);
#endif
  strftime(out_report->generatedAt, sizeof(out_report->generatedAt),
           "%Y-%m-%dT%H:%M:%SZ", &tm_utc);

  if (item_count == 0 || !items) {
    return true;
  }

  float **decoded = calloc((size_t)item_count, sizeof(float *));
  int *dims = calloc((size_t)item_count, sizeof(int));
  SimilarityGroup *groups = calloc((size_t)item_count, sizeof(SimilarityGroup));
  if (!decoded || !dims || !groups) {
    free(decoded);
    free(dims);
    free(groups);
    return false;
  }

  for (int i = 0; i < item_count; i++) {
    if (!items[i].path || !items[i].mlEmbeddingBase64 || items[i].mlEmbeddingDim <= 0) {
      continue;
    }

    float *embedding = NULL;
    int dim = 0;
    if (!SimilarityDecodeEmbeddingBase64(items[i].mlEmbeddingBase64, &embedding,
                                         &dim)) {
      continue;
    }
    if (dim != items[i].mlEmbeddingDim) {
      free(embedding);
      continue;
    }
    decoded[i] = embedding;
    dims[i] = dim;
  }

  int group_count = 0;
  for (int i = 0; i < item_count; i++) {
    if (!decoded[i] || !items[i].path) {
      continue;
    }

    ScoreIndex *candidates = calloc((size_t)item_count, sizeof(ScoreIndex));
    if (!candidates) {
      FreeTempEmbeddings(decoded, item_count);
      free(dims);
      FreeGroupsPartial(groups, group_count);
      return false;
    }

    int candidate_count = 0;
    for (int j = 0; j < item_count; j++) {
      if (i == j || !decoded[j] || !items[j].path) {
        continue;
      }
      if (dims[i] != dims[j]) {
        continue;
      }

      float score = 0.0f;
      if (!SimilarityCosine(decoded[i], decoded[j], dims[i], &score)) {
        continue;
      }
      if (score < threshold) {
        continue;
      }

      candidates[candidate_count].index = j;
      candidates[candidate_count].score = score;
      candidate_count++;
    }

    if (candidate_count > 0) {
      qsort(candidates, (size_t)candidate_count, sizeof(ScoreIndex),
            CompareScoreDesc);

      int keep = candidate_count < topk ? candidate_count : topk;
      groups[group_count].anchorPath = strdup(items[i].path);
      groups[group_count].neighborCount = keep;
      groups[group_count].neighbors =
          calloc((size_t)keep, sizeof(SimilarityNeighbor));
      if (!groups[group_count].anchorPath || !groups[group_count].neighbors) {
        free(candidates);
        FreeTempEmbeddings(decoded, item_count);
        free(dims);
        FreeGroupsPartial(groups, group_count + 1);
        return false;
      }

      for (int k = 0; k < keep; k++) {
        const int idx = candidates[k].index;
        groups[group_count].neighbors[k].path = strdup(items[idx].path);
        if (!groups[group_count].neighbors[k].path) {
          free(candidates);
          FreeTempEmbeddings(decoded, item_count);
          free(dims);
          FreeGroupsPartial(groups, group_count + 1);
          return false;
        }
        groups[group_count].neighbors[k].score = candidates[k].score;
      }

      group_count++;
    }

    free(candidates);
  }

  FreeTempEmbeddings(decoded, item_count);
  free(dims);

  out_report->groups = groups;
  out_report->groupCount = group_count;
  return true;
}

bool SimilarityWriteReportJson(const char *output_path,
                               const SimilarityReport *report) {
  if (!output_path || !report) {
    return false;
  }

  cJSON *root = cJSON_CreateObject();
  if (!root) {
    return false;
  }

  cJSON_AddStringToObject(root, "generatedAt", report->generatedAt);
  cJSON_AddStringToObject(root, "modelId", report->modelId);
  cJSON_AddNumberToObject(root, "threshold", report->threshold);
  cJSON_AddNumberToObject(root, "topK", report->topK);
  cJSON_AddNumberToObject(root, "groupCount", report->groupCount);

  cJSON *groups = cJSON_CreateArray();
  if (!groups) {
    cJSON_Delete(root);
    return false;
  }

  for (int i = 0; i < report->groupCount; i++) {
    cJSON *group = cJSON_CreateObject();
    if (!group) {
      cJSON_Delete(root);
      return false;
    }
    cJSON_AddStringToObject(group, "anchorPath", report->groups[i].anchorPath);
    cJSON_AddNumberToObject(group, "neighborCount",
                            report->groups[i].neighborCount);

    cJSON *neighbors = cJSON_CreateArray();
    if (!neighbors) {
      cJSON_Delete(root);
      return false;
    }

    for (int j = 0; j < report->groups[i].neighborCount; j++) {
      cJSON *neighbor = cJSON_CreateObject();
      if (!neighbor) {
        cJSON_Delete(root);
        return false;
      }
      cJSON_AddStringToObject(neighbor, "path",
                              report->groups[i].neighbors[j].path);
      cJSON_AddNumberToObject(neighbor, "score",
                              report->groups[i].neighbors[j].score);
      cJSON_AddItemToArray(neighbors, neighbor);
    }

    cJSON_AddItemToObject(group, "neighbors", neighbors);
    cJSON_AddItemToArray(groups, group);
  }

  cJSON_AddItemToObject(root, "groups", groups);

  char *json = cJSON_Print(root);
  cJSON_Delete(root);
  if (!json) {
    return false;
  }

  FILE *f = fopen(output_path, "w");
  if (!f) {
    free(json);
    return false;
  }
  fputs(json, f);
  fclose(f);
  free(json);
  return true;
}

void SimilarityFreeReport(SimilarityReport *report) {
  if (!report) {
    return;
  }

  for (int i = 0; i < report->groupCount; i++) {
    free(report->groups[i].anchorPath);
    for (int j = 0; j < report->groups[i].neighborCount; j++) {
      free(report->groups[i].neighbors[j].path);
    }
    free(report->groups[i].neighbors);
  }
  free(report->groups);
  memset(report, 0, sizeof(SimilarityReport));
}
