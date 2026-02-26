#include <math.h>

#include "similarity_engine.h"

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
