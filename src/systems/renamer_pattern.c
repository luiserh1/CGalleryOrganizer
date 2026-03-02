#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "systems/renamer_pattern.h"

#define RENAMER_INLINE_BUFFER 1024

static const char *RENAMER_TOKEN_ALLOWLIST[] = {
    "date", "time", "datetime", "camera", "make", "model", "format",
    "index", "tags_manual", "tags_meta", "tags", NULL};

static bool TokenIsAllowed(const char *token) {
  if (!token || token[0] == '\0') {
    return false;
  }

  for (int i = 0; RENAMER_TOKEN_ALLOWLIST[i]; i++) {
    if (strcmp(RENAMER_TOKEN_ALLOWLIST[i], token) == 0) {
      return true;
    }
  }
  return false;
}

static bool AppendText(char *out_text, size_t out_size, size_t *io_used,
                       const char *value) {
  if (!out_text || out_size == 0 || !io_used || !value) {
    return false;
  }

  size_t used = *io_used;
  size_t value_len = strlen(value);
  if (used + value_len >= out_size) {
    return false;
  }

  memcpy(out_text + used, value, value_len);
  used += value_len;
  out_text[used] = '\0';
  *io_used = used;
  return true;
}

static bool AppendChar(char *out_text, size_t out_size, size_t *io_used,
                       char c) {
  if (!out_text || out_size == 0 || !io_used) {
    return false;
  }

  size_t used = *io_used;
  if (used + 1 >= out_size) {
    return false;
  }

  out_text[used] = c;
  used++;
  out_text[used] = '\0';
  *io_used = used;
  return true;
}

static uint32_t Fnv1a32(const char *text) {
  uint32_t hash = 2166136261u;
  if (!text) {
    return hash;
  }

  const unsigned char *p = (const unsigned char *)text;
  while (*p) {
    hash ^= (uint32_t)(*p);
    hash *= 16777619u;
    p++;
  }
  return hash;
}

static void BuildSlug(const char *src, char *out_slug, size_t out_size,
                      const char *fallback) {
  if (!out_slug || out_size == 0) {
    return;
  }

  out_slug[0] = '\0';
  if (!src || src[0] == '\0') {
    if (fallback) {
      strncpy(out_slug, fallback, out_size - 1);
      out_slug[out_size - 1] = '\0';
    }
    return;
  }

  size_t used = 0;
  char last = '\0';
  for (size_t i = 0; src[i] != '\0'; i++) {
    unsigned char c = (unsigned char)src[i];
    char emit = '\0';

    if (isalnum(c)) {
      emit = (char)tolower(c);
    } else if (c == '.' || c == '-' || c == '_') {
      emit = (char)c;
    } else if (isspace(c) || c == '/' || c == '\\' || c == ':' || c == '*' ||
               c == '?' || c == '"' || c == '<' || c == '>' || c == '|') {
      emit = '-';
    } else {
      emit = '-';
    }

    if ((emit == '-' || emit == '_' || emit == '.') &&
        (last == '-' || last == '_' || last == '.')) {
      continue;
    }

    if (used + 1 >= out_size) {
      break;
    }
    out_slug[used++] = emit;
    last = emit;
  }
  out_slug[used] = '\0';

  while (out_slug[0] == '-' || out_slug[0] == '_' || out_slug[0] == '.') {
    memmove(out_slug, out_slug + 1, strlen(out_slug));
  }

  size_t len = strlen(out_slug);
  while (len > 0 &&
         (out_slug[len - 1] == '-' || out_slug[len - 1] == '_' ||
          out_slug[len - 1] == '.')) {
    out_slug[len - 1] = '\0';
    len--;
  }

  if (out_slug[0] == '\0' && fallback) {
    strncpy(out_slug, fallback, out_size - 1);
    out_slug[out_size - 1] = '\0';
  }
}

static void ResolveTokenValue(const char *token, const RenamerPatternContext *ctx,
                              char *out_value, size_t out_size) {
  if (!token || !ctx || !out_value || out_size == 0) {
    return;
  }

  if (strcmp(token, "date") == 0) {
    BuildSlug(ctx->date, out_value, out_size, "unknown-date");
  } else if (strcmp(token, "time") == 0) {
    BuildSlug(ctx->time, out_value, out_size, "unknown-time");
  } else if (strcmp(token, "datetime") == 0) {
    BuildSlug(ctx->datetime, out_value, out_size, "unknown-datetime");
  } else if (strcmp(token, "camera") == 0) {
    BuildSlug(ctx->camera, out_value, out_size, "unknown-camera");
  } else if (strcmp(token, "make") == 0) {
    BuildSlug(ctx->make, out_value, out_size, "unknown-make");
  } else if (strcmp(token, "model") == 0) {
    BuildSlug(ctx->model, out_value, out_size, "unknown-model");
  } else if (strcmp(token, "format") == 0) {
    BuildSlug(ctx->format, out_value, out_size, "unknown-format");
  } else if (strcmp(token, "index") == 0) {
    int index = ctx->index > 0 ? ctx->index : 1;
    snprintf(out_value, out_size, "%d", index);
  } else if (strcmp(token, "tags_manual") == 0) {
    BuildSlug(ctx->tags_manual, out_value, out_size, "untagged");
  } else if (strcmp(token, "tags_meta") == 0) {
    BuildSlug(ctx->tags_meta, out_value, out_size, "untagged");
  } else if (strcmp(token, "tags") == 0) {
    BuildSlug(ctx->tags, out_value, out_size, "untagged");
  } else {
    strncpy(out_value, "unknown", out_size - 1);
    out_value[out_size - 1] = '\0';
  }
}

static bool ValidateOrRenderInternal(const char *pattern,
                                     const RenamerPatternContext *ctx,
                                     bool render_mode, char *out_render,
                                     size_t out_render_size, char *out_error,
                                     size_t out_error_size) {
  if (out_error && out_error_size > 0) {
    out_error[0] = '\0';
  }

  if (!pattern || pattern[0] == '\0') {
    if (out_error && out_error_size > 0) {
      snprintf(out_error, out_error_size, "rename pattern cannot be empty");
    }
    return false;
  }

  size_t used = 0;
  if (render_mode && out_render && out_render_size > 0) {
    out_render[0] = '\0';
  }

  for (size_t i = 0; pattern[i] != '\0';) {
    if (pattern[i] == '[') {
      if (pattern[i + 1] == '[') {
        if (render_mode && !AppendChar(out_render, out_render_size, &used, '[')) {
          if (out_error && out_error_size > 0) {
            snprintf(out_error, out_error_size,
                     "rendered filename exceeded internal buffer");
          }
          return false;
        }
        i += 2;
        continue;
      }

      size_t start = i + 1;
      size_t end = start;
      while (pattern[end] != '\0' && pattern[end] != ']') {
        end++;
      }
      if (pattern[end] != ']') {
        if (out_error && out_error_size > 0) {
          snprintf(out_error, out_error_size,
                   "unterminated token starting at position %zu", i);
        }
        return false;
      }

      size_t token_len = end - start;
      if (token_len == 0 || token_len >= RENAMER_PATTERN_TOKEN_MAX) {
        if (out_error && out_error_size > 0) {
          snprintf(out_error, out_error_size,
                   "invalid token length at position %zu", i);
        }
        return false;
      }

      char token[RENAMER_PATTERN_TOKEN_MAX] = {0};
      memcpy(token, pattern + start, token_len);
      token[token_len] = '\0';

      if (!TokenIsAllowed(token)) {
        if (out_error && out_error_size > 0) {
          snprintf(out_error, out_error_size, "unknown token [%s]", token);
        }
        return false;
      }

      if (render_mode) {
        char value[256] = {0};
        ResolveTokenValue(token, ctx, value, sizeof(value));
        if (!AppendText(out_render, out_render_size, &used, value)) {
          if (out_error && out_error_size > 0) {
            snprintf(out_error, out_error_size,
                     "rendered filename exceeded internal buffer");
          }
          return false;
        }
      }

      i = end + 1;
      continue;
    }

    if (pattern[i] == ']') {
      if (pattern[i + 1] == ']') {
        if (render_mode && !AppendChar(out_render, out_render_size, &used, ']')) {
          if (out_error && out_error_size > 0) {
            snprintf(out_error, out_error_size,
                     "rendered filename exceeded internal buffer");
          }
          return false;
        }
        i += 2;
        continue;
      }

      if (out_error && out_error_size > 0) {
        snprintf(out_error, out_error_size,
                 "unexpected ']' at position %zu (use ']]' to escape)", i);
      }
      return false;
    }

    if (render_mode &&
        !AppendChar(out_render, out_render_size, &used, pattern[i])) {
      if (out_error && out_error_size > 0) {
        snprintf(out_error, out_error_size,
                 "rendered filename exceeded internal buffer");
      }
      return false;
    }

    i++;
  }

  return true;
}

static void ApplyLengthPolicy(char *name, bool *out_truncated, char *out_warning,
                              size_t out_warning_size) {
  if (!name) {
    return;
  }

  if (out_truncated) {
    *out_truncated = false;
  }
  if (out_warning && out_warning_size > 0) {
    out_warning[0] = '\0';
  }

  size_t len = strlen(name);
  if (len <= RENAMER_FILENAME_COMPONENT_MAX) {
    return;
  }

  uint32_t hash = Fnv1a32(name);
  char suffix[16] = {0};
  snprintf(suffix, sizeof(suffix), "-%08x", hash);

  const char *dot = strrchr(name, '.');
  size_t ext_len = 0;
  if (dot && dot != name) {
    ext_len = strlen(dot);
  }

  if (ext_len > 15) {
    ext_len = 0;
  }

  size_t suffix_len = strlen(suffix);
  size_t keep_len = 0;
  if (RENAMER_FILENAME_COMPONENT_MAX > suffix_len + ext_len) {
    keep_len = RENAMER_FILENAME_COMPONENT_MAX - suffix_len - ext_len;
  }

  char output[RENAMER_INLINE_BUFFER] = {0};
  if (keep_len > 0) {
    strncpy(output, name, keep_len);
    output[keep_len] = '\0';
  }
  strncat(output, suffix, sizeof(output) - strlen(output) - 1);
  if (ext_len > 0 && dot) {
    strncat(output, dot, sizeof(output) - strlen(output) - 1);
  }

  strncpy(name, output, RENAMER_INLINE_BUFFER - 1);
  name[RENAMER_INLINE_BUFFER - 1] = '\0';

  if (out_truncated) {
    *out_truncated = true;
  }
  if (out_warning && out_warning_size > 0) {
    snprintf(out_warning, out_warning_size,
             "filename exceeded %d chars; applied truncate+hash policy",
             RENAMER_FILENAME_COMPONENT_MAX);
  }
}

const char *RenamerPatternDefault(void) {
  return "pic-[datetime]-[index].[format]";
}

bool RenamerPatternValidate(const char *pattern, char *out_error,
                            size_t out_error_size) {
  return ValidateOrRenderInternal(pattern, NULL, false, NULL, 0, out_error,
                                  out_error_size);
}

bool RenamerPatternRender(const char *pattern, const RenamerPatternContext *ctx,
                          char *out_filename, size_t out_filename_size,
                          bool *out_truncated, char *out_warning,
                          size_t out_warning_size, char *out_error,
                          size_t out_error_size) {
  if (!ctx || !out_filename || out_filename_size == 0) {
    if (out_error && out_error_size > 0) {
      snprintf(out_error, out_error_size, "invalid render arguments");
    }
    return false;
  }

  char rendered[RENAMER_INLINE_BUFFER] = {0};
  if (!ValidateOrRenderInternal(pattern, ctx, true, rendered, sizeof(rendered),
                                out_error, out_error_size)) {
    return false;
  }

  RenamerPatternSanitizeSlug(rendered);
  ApplyLengthPolicy(rendered, out_truncated, out_warning, out_warning_size);

  if (rendered[0] == '\0') {
    strncpy(rendered, "file", sizeof(rendered) - 1);
    rendered[sizeof(rendered) - 1] = '\0';
  }

  if (strlen(rendered) >= out_filename_size) {
    if (out_error && out_error_size > 0) {
      snprintf(out_error, out_error_size,
               "rendered filename exceeded destination buffer");
    }
    return false;
  }

  strncpy(out_filename, rendered, out_filename_size - 1);
  out_filename[out_filename_size - 1] = '\0';
  return true;
}

void RenamerPatternSanitizeSlug(char *text) {
  if (!text || text[0] == '\0') {
    return;
  }

  char normalized[RENAMER_INLINE_BUFFER] = {0};
  BuildSlug(text, normalized, sizeof(normalized), "file");
  strncpy(text, normalized, RENAMER_INLINE_BUFFER - 1);
  text[RENAMER_INLINE_BUFFER - 1] = '\0';
}
