#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "cJSON.h"
#include "similarity_engine.h"

typedef struct {
  int index;
  float score;
} ScoreIndex;

static SimilarityMemoryMode MEMORY_MODE = SIM_MEMORY_MODE_CHUNKED;

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

static void TopKConsider(ScoreIndex *top, int *count, int maxk, int index,
                         float score) {
  if (!top || !count || maxk <= 0) {
    return;
  }

  if (*count < maxk) {
    top[*count].index = index;
    top[*count].score = score;
    (*count)++;
    return;
  }

  int min_pos = 0;
  float min_score = top[0].score;
  for (int i = 1; i < *count; i++) {
    if (top[i].score < min_score) {
      min_score = top[i].score;
      min_pos = i;
    }
  }
  if (score > min_score) {
    top[min_pos].index = index;
    top[min_pos].score = score;
  }
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

static void InitReport(SimilarityReport *out_report, const char *model_id,
                       float threshold, int topk) {
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
}

static bool DecodeEmbeddingForItem(const ImageMetadata *item,
                                   float **out_embedding, int *out_dim) {
  if (!item || !item->path || !item->mlEmbeddingBase64 ||
      item->mlEmbeddingDim <= 0 || !out_embedding || !out_dim) {
    return false;
  }

  float *embedding = NULL;
  int dim = 0;
  if (!SimilarityDecodeEmbeddingBase64(item->mlEmbeddingBase64, &embedding,
                                       &dim)) {
    return false;
  }
  if (dim != item->mlEmbeddingDim) {
    free(embedding);
    return false;
  }

  *out_embedding = embedding;
  *out_dim = dim;
  return true;
}

static bool AppendGroup(const ImageMetadata *items, int anchor_index, int topk,
                        ScoreIndex *candidates, int candidate_count,
                        SimilarityGroup *groups, int *group_count) {
  if (!items || !candidates || !groups || !group_count || candidate_count <= 0) {
    return false;
  }

  qsort(candidates, (size_t)candidate_count, sizeof(ScoreIndex), CompareScoreDesc);

  int keep = candidate_count < topk ? candidate_count : topk;
  groups[*group_count].anchorPath = strdup(items[anchor_index].path);
  groups[*group_count].neighborCount = keep;
  groups[*group_count].neighbors = calloc((size_t)keep, sizeof(SimilarityNeighbor));
  if (!groups[*group_count].anchorPath || !groups[*group_count].neighbors) {
    return false;
  }

  for (int k = 0; k < keep; k++) {
    const int idx = candidates[k].index;
    groups[*group_count].neighbors[k].path = strdup(items[idx].path);
    if (!groups[*group_count].neighbors[k].path) {
      return false;
    }
    groups[*group_count].neighbors[k].score = candidates[k].score;
  }

  (*group_count)++;
  return true;
}

static bool BuildReportEager(float threshold, int topk, const ImageMetadata *items,
                             int item_count, SimilarityReport *out_report) {
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
    if (!DecodeEmbeddingForItem(&items[i], &decoded[i], &dims[i])) {
      continue;
    }
  }

  int group_count = 0;
  for (int i = 0; i < item_count; i++) {
    if (!decoded[i] || !items[i].path) {
      continue;
    }

    ScoreIndex *candidates = calloc((size_t)topk, sizeof(ScoreIndex));
    if (!candidates) {
      FreeTempEmbeddings(decoded, item_count);
      free(dims);
      FreeGroupsPartial(groups, group_count);
      return false;
    }

    int candidate_count = 0;
    for (int j = 0; j < item_count; j++) {
      if (i == j || !decoded[j] || !items[j].path || dims[i] != dims[j]) {
        continue;
      }

      float score = 0.0f;
      if (!SimilarityCosine(decoded[i], decoded[j], dims[i], &score) ||
          score < threshold) {
        continue;
      }

      TopKConsider(candidates, &candidate_count, topk, j, score);
    }

    if (candidate_count > 0 &&
        !AppendGroup(items, i, topk, candidates, candidate_count, groups,
                     &group_count)) {
      free(candidates);
      FreeTempEmbeddings(decoded, item_count);
      free(dims);
      FreeGroupsPartial(groups, group_count + 1);
      return false;
    }

    free(candidates);
  }

  FreeTempEmbeddings(decoded, item_count);
  free(dims);

  out_report->groups = groups;
  out_report->groupCount = group_count;
  return true;
}

static bool BuildReportChunked(float threshold, int topk, const ImageMetadata *items,
                               int item_count, SimilarityReport *out_report) {
  SimilarityGroup *groups = calloc((size_t)item_count, sizeof(SimilarityGroup));
  if (!groups) {
    return false;
  }

  int group_count = 0;
  for (int i = 0; i < item_count; i++) {
    float *anchor = NULL;
    int anchor_dim = 0;
    if (!DecodeEmbeddingForItem(&items[i], &anchor, &anchor_dim)) {
      continue;
    }

    ScoreIndex *candidates = calloc((size_t)topk, sizeof(ScoreIndex));
    if (!candidates) {
      free(anchor);
      FreeGroupsPartial(groups, group_count);
      return false;
    }

    int candidate_count = 0;
    for (int j = 0; j < item_count; j++) {
      if (i == j || !items[j].path) {
        continue;
      }

      float *peer = NULL;
      int peer_dim = 0;
      if (!DecodeEmbeddingForItem(&items[j], &peer, &peer_dim)) {
        continue;
      }
      if (anchor_dim != peer_dim) {
        free(peer);
        continue;
      }

      float score = 0.0f;
      bool ok = SimilarityCosine(anchor, peer, anchor_dim, &score);
      free(peer);
      if (!ok || score < threshold) {
        continue;
      }

      TopKConsider(candidates, &candidate_count, topk, j, score);
    }

    if (candidate_count > 0 &&
        !AppendGroup(items, i, topk, candidates, candidate_count, groups,
                     &group_count)) {
      free(candidates);
      free(anchor);
      FreeGroupsPartial(groups, group_count + 1);
      return false;
    }

    free(candidates);
    free(anchor);
  }

  out_report->groups = groups;
  out_report->groupCount = group_count;
  return true;
}

bool SimilarityBuildReport(const char *model_id, float threshold, int topk,
                           const ImageMetadata *items, int item_count,
                           SimilarityReport *out_report) {
  if (!model_id || threshold < 0.0f || threshold > 1.0f || topk <= 0 ||
      !out_report || item_count < 0) {
    return false;
  }

  InitReport(out_report, model_id, threshold, topk);

  if (item_count == 0 || !items) {
    return true;
  }

  if (MEMORY_MODE == SIM_MEMORY_MODE_EAGER) {
    return BuildReportEager(threshold, topk, items, item_count, out_report);
  }
  return BuildReportChunked(threshold, topk, items, item_count, out_report);
}

void SimilaritySetMemoryMode(SimilarityMemoryMode mode) {
  if (mode != SIM_MEMORY_MODE_EAGER && mode != SIM_MEMORY_MODE_CHUNKED) {
    return;
  }
  MEMORY_MODE = mode;
}

SimilarityMemoryMode SimilarityGetMemoryMode(void) { return MEMORY_MODE; }

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
      cJSON_Delete(groups);
      cJSON_Delete(root);
      return false;
    }

    cJSON_AddStringToObject(group, "anchorPath", report->groups[i].anchorPath);
    cJSON_AddNumberToObject(group, "neighborCount",
                            report->groups[i].neighborCount);

    cJSON *neighbors = cJSON_CreateArray();
    if (!neighbors) {
      cJSON_Delete(group);
      cJSON_Delete(groups);
      cJSON_Delete(root);
      return false;
    }

    for (int j = 0; j < report->groups[i].neighborCount; j++) {
      cJSON *neighbor = cJSON_CreateObject();
      if (!neighbor) {
        cJSON_Delete(neighbors);
        cJSON_Delete(group);
        cJSON_Delete(groups);
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
  report->groups = NULL;
  report->groupCount = 0;
}
