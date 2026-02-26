#include <math.h>
#include <stddef.h>
#include <string.h>

#include "gui/gui_layout.h"
#include "gui/gui_state.h"

static float ClampFloat(float value, float min_value, float max_value) {
  if (value < min_value) {
    return min_value;
  }
  if (value > max_value) {
    return max_value;
  }
  return value;
}

static int ClampInt(int value, int min_value, int max_value) {
  if (value < min_value) {
    return min_value;
  }
  if (value > max_value) {
    return max_value;
  }
  return value;
}

int GuiLayoutClampUiScalePercent(int ui_scale_percent) {
  if (ui_scale_percent <= 0) {
    return GUI_STATE_DEFAULT_UI_SCALE_PERCENT;
  }
  return ClampInt(ui_scale_percent, GUI_STATE_MIN_UI_SCALE_PERCENT,
                  GUI_STATE_MAX_UI_SCALE_PERCENT);
}

float GuiLayoutComputeEffectiveScale(int window_width, int window_height,
                                     int ui_scale_percent) {
  float width_scale = (float)window_width / 1000.0f;
  float height_scale = (float)window_height / 760.0f;
  float auto_scale = ClampFloat(fminf(width_scale, height_scale), 0.85f, 1.75f);
  float user_scale =
      (float)GuiLayoutClampUiScalePercent(ui_scale_percent) / 100.0f;
  return auto_scale * user_scale;
}

void GuiLayoutComputeMetrics(GuiLayoutMetrics *out, int window_width,
                             int window_height, int ui_scale_percent) {
  if (!out) {
    return;
  }

  memset(out, 0, sizeof(*out));
  out->window_width = window_width;
  out->window_height = window_height;
  out->effective_scale =
      GuiLayoutComputeEffectiveScale(window_width, window_height, ui_scale_percent);

  out->pad = (int)lroundf(10.0f * out->effective_scale);
  out->gap = (int)lroundf(8.0f * out->effective_scale);
  out->font_size = (int)lroundf(20.0f * out->effective_scale);
  out->line_height = out->font_size + out->gap;
  out->label_h = (int)lroundf(24.0f * out->effective_scale);
  out->input_h = (int)lroundf(30.0f * out->effective_scale);
  out->button_h = (int)lroundf(34.0f * out->effective_scale);
  out->small_button_w = (int)lroundf(48.0f * out->effective_scale);
  out->zoom_value_w = (int)lroundf(72.0f * out->effective_scale);
  out->tab_button_min_w = (int)lroundf(120.0f * out->effective_scale);
  out->min_text_input_w = (int)lroundf(220.0f * out->effective_scale);
  out->min_numeric_input_w = (int)lroundf(72.0f * out->effective_scale);
}

float GuiLayoutMeasureTextWidth(const char *text, const GuiLayoutMetrics *metrics) {
  if (!text || !metrics) {
    return 0.0f;
  }

  size_t len = strlen(text);
  if (len == 0) {
    return 0.0f;
  }

  return (float)len * ((float)metrics->font_size * 0.56f);
}

float GuiLayoutButtonWidth(const char *text, const GuiLayoutMetrics *metrics,
                           float min_width) {
  float text_width = GuiLayoutMeasureTextWidth(text, metrics);
  float padded = text_width + (float)(metrics ? metrics->gap * 3 : 24);
  if (padded < min_width) {
    return min_width;
  }
  return padded;
}

void GuiLayoutComputeMinWindowSize(int *out_width, int *out_height) {
  GuiLayoutMetrics metrics = {0};
  GuiLayoutComputeMetrics(&metrics, GUI_STATE_DEFAULT_WINDOW_WIDTH,
                          GUI_STATE_DEFAULT_WINDOW_HEIGHT,
                          GUI_STATE_DEFAULT_UI_SCALE_PERCENT);

  float jobs_w = GuiLayoutButtonWidth("Jobs", &metrics, 0.0f);
  float cache_label_w = GuiLayoutButtonWidth("Cache compression", &metrics, 0.0f);
  float mode_none_w = GuiLayoutButtonWidth("none", &metrics, 66.0f);
  float mode_zstd_w = GuiLayoutButtonWidth("zstd", &metrics, 66.0f);
  float mode_auto_w = GuiLayoutButtonWidth("auto", &metrics, 66.0f);
  float level_w = GuiLayoutButtonWidth("Level", &metrics, 0.0f);

  float scan_controls_w =
      jobs_w + (float)metrics.min_numeric_input_w + cache_label_w + mode_none_w +
      mode_zstd_w + mode_auto_w + level_w + (float)metrics.min_numeric_input_w +
      (float)(metrics.gap * 7 + metrics.pad * 2);

  float action_row_w = GuiLayoutButtonWidth("Scan/Cache", &metrics, 120.0f) +
                       GuiLayoutButtonWidth("ML Enrich", &metrics, 120.0f) +
                       GuiLayoutButtonWidth("Save Paths", &metrics, 140.0f) +
                       GuiLayoutButtonWidth("Reset Saved Paths", &metrics, 180.0f) +
                       (float)(metrics.gap * 3 + metrics.pad * 2);

  float tabs_w = GuiLayoutButtonWidth("Scan", &metrics, (float)metrics.tab_button_min_w) +
                 GuiLayoutButtonWidth("Similarity", &metrics,
                                      (float)metrics.tab_button_min_w) +
                 GuiLayoutButtonWidth("Organize", &metrics,
                                      (float)metrics.tab_button_min_w) +
                 GuiLayoutButtonWidth("Duplicates", &metrics,
                                      (float)metrics.tab_button_min_w) +
                 GuiLayoutButtonWidth("A-", &metrics, (float)metrics.small_button_w) +
                 (float)metrics.zoom_value_w +
                 GuiLayoutButtonWidth("A+", &metrics, (float)metrics.small_button_w) +
                 GuiLayoutButtonWidth("Reset", &metrics, 78.0f) +
                 (float)(metrics.gap * 8 + metrics.pad * 2);

  int min_width = (int)lroundf(fmaxf(fmaxf(scan_controls_w, action_row_w), tabs_w));
  if (min_width < 860) {
    min_width = 860;
  }

  int min_height =
      metrics.pad + metrics.label_h + metrics.gap + metrics.button_h + metrics.gap +
      metrics.pad + metrics.input_h + metrics.gap + metrics.input_h + metrics.gap +
      metrics.label_h + metrics.gap + metrics.button_h + metrics.gap +
      metrics.button_h + metrics.gap + metrics.pad + metrics.label_h + metrics.gap +
      metrics.line_height * 8 + metrics.pad;
  if (min_height < 700) {
    min_height = 700;
  }

  if (out_width) {
    *out_width = min_width;
  }
  if (out_height) {
    *out_height = min_height;
  }
}

void GuiLayoutInit(GuiLayoutContext *ctx, GuiLayoutRect bounds,
                   const GuiLayoutMetrics *metrics) {
  if (!ctx || !metrics) {
    return;
  }

  memset(ctx, 0, sizeof(*ctx));
  ctx->bounds = bounds;
  ctx->gap = (float)metrics->gap;
  ctx->row_start_x = bounds.x + (float)metrics->pad;
  ctx->row_right_x = bounds.x + bounds.width - (float)metrics->pad;
  ctx->cursor_x = ctx->row_start_x;
  ctx->cursor_y = bounds.y + (float)metrics->pad;
  ctx->row_height = 0.0f;
  ctx->debug_valid = true;
  ctx->debug_rect_count = 0;
}

void GuiLayoutBeginRow(GuiLayoutContext *ctx) {
  if (!ctx) {
    return;
  }

  if (ctx->row_height > 0.0f && ctx->cursor_x > ctx->row_start_x) {
    ctx->cursor_x = ctx->row_start_x;
    ctx->cursor_y += ctx->row_height + ctx->gap;
    ctx->row_height = 0.0f;
  }
}

void GuiLayoutNextLine(GuiLayoutContext *ctx) {
  if (!ctx) {
    return;
  }

  if (ctx->row_height <= 0.0f) {
    return;
  }

  ctx->cursor_x = ctx->row_start_x;
  ctx->cursor_y += ctx->row_height + ctx->gap;
  ctx->row_height = 0.0f;
}

static void LayoutWrapIfNeeded(GuiLayoutContext *ctx, float width) {
  if (!ctx) {
    return;
  }

  if (ctx->cursor_x > ctx->row_start_x &&
      ctx->cursor_x + width > ctx->row_right_x + 0.01f) {
    ctx->cursor_x = ctx->row_start_x;
    ctx->cursor_y += ctx->row_height + ctx->gap;
    ctx->row_height = 0.0f;
  }
}

static void RegisterRectForDebug(GuiLayoutContext *ctx, GuiLayoutRect rect) {
  if (!ctx) {
    return;
  }

  if (!GuiLayoutRectInBounds(ctx->bounds, rect)) {
    ctx->debug_valid = false;
  }

  if (ctx->debug_rect_count < (int)(sizeof(ctx->debug_rects) /
                                    sizeof(ctx->debug_rects[0]))) {
    for (int i = 0; i < ctx->debug_rect_count; i++) {
      if (GuiLayoutRectsOverlap(ctx->debug_rects[i], rect)) {
        ctx->debug_valid = false;
        break;
      }
    }
    ctx->debug_rects[ctx->debug_rect_count++] = rect;
  } else {
    ctx->debug_valid = false;
  }
}

GuiLayoutRect GuiLayoutPlaceFixed(GuiLayoutContext *ctx, float width, float height) {
  GuiLayoutRect rect = {0};
  if (!ctx) {
    return rect;
  }

  if (width < 1.0f) {
    width = 1.0f;
  }
  if (height < 1.0f) {
    height = 1.0f;
  }

  LayoutWrapIfNeeded(ctx, width);

  rect.x = ctx->cursor_x;
  rect.y = ctx->cursor_y;
  rect.width = width;
  rect.height = height;

  ctx->cursor_x += width + ctx->gap;
  if (height > ctx->row_height) {
    ctx->row_height = height;
  }
  RegisterRectForDebug(ctx, rect);
  return rect;
}

GuiLayoutRect GuiLayoutPlaceFlex(GuiLayoutContext *ctx, float min_width,
                                 float height) {
  GuiLayoutRect rect = {0};
  if (!ctx) {
    return rect;
  }

  if (min_width < 1.0f) {
    min_width = 1.0f;
  }
  if (height < 1.0f) {
    height = 1.0f;
  }

  float remaining = GuiLayoutRemainingWidth(ctx);
  if (ctx->cursor_x > ctx->row_start_x && remaining < min_width) {
    ctx->cursor_x = ctx->row_start_x;
    ctx->cursor_y += ctx->row_height + ctx->gap;
    ctx->row_height = 0.0f;
    remaining = GuiLayoutRemainingWidth(ctx);
  }

  if (remaining < min_width) {
    remaining = min_width;
    if (ctx->row_right_x > ctx->row_start_x) {
      float max_width = ctx->row_right_x - ctx->row_start_x;
      if (remaining > max_width) {
        remaining = max_width;
      }
    }
  }

  rect.x = ctx->cursor_x;
  rect.y = ctx->cursor_y;
  rect.width = remaining;
  rect.height = height;

  ctx->cursor_x += remaining + ctx->gap;
  if (height > ctx->row_height) {
    ctx->row_height = height;
  }
  RegisterRectForDebug(ctx, rect);
  return rect;
}

float GuiLayoutRemainingWidth(const GuiLayoutContext *ctx) {
  if (!ctx) {
    return 0.0f;
  }
  float remaining = ctx->row_right_x - ctx->cursor_x;
  if (remaining < 0.0f) {
    return 0.0f;
  }
  return remaining;
}

bool GuiLayoutContextIsValid(const GuiLayoutContext *ctx) {
  if (!ctx) {
    return false;
  }
  return ctx->debug_valid;
}

bool GuiLayoutRectInBounds(GuiLayoutRect outer, GuiLayoutRect inner) {
  return inner.x >= outer.x && inner.y >= outer.y &&
         inner.x + inner.width <= outer.x + outer.width &&
         inner.y + inner.height <= outer.y + outer.height;
}

bool GuiLayoutRectsOverlap(GuiLayoutRect a, GuiLayoutRect b) {
  bool separated = a.x + a.width <= b.x || b.x + b.width <= a.x ||
                   a.y + a.height <= b.y || b.y + b.height <= a.y;
  return !separated;
}

bool GuiLayoutValidateNoOverlap(const GuiLayoutRect *rects, int rect_count,
                                GuiLayoutRect bounds) {
  if (!rects || rect_count < 0) {
    return false;
  }

  for (int i = 0; i < rect_count; i++) {
    if (!GuiLayoutRectInBounds(bounds, rects[i])) {
      return false;
    }
  }

  for (int i = 0; i < rect_count; i++) {
    for (int j = i + 1; j < rect_count; j++) {
      if (GuiLayoutRectsOverlap(rects[i], rects[j])) {
        return false;
      }
    }
  }
  return true;
}
