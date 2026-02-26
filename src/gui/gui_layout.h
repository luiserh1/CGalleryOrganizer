#ifndef GUI_LAYOUT_H
#define GUI_LAYOUT_H

#include <stdbool.h>

typedef struct {
  float x;
  float y;
  float width;
  float height;
} GuiLayoutRect;

typedef struct {
  float effective_scale;
  int window_width;
  int window_height;
  int pad;
  int gap;
  int font_size;
  int line_height;
  int label_h;
  int input_h;
  int button_h;
  int small_button_w;
  int zoom_value_w;
  int tab_button_min_w;
  int min_text_input_w;
  int min_numeric_input_w;
} GuiLayoutMetrics;

typedef struct {
  GuiLayoutRect bounds;
  float row_start_x;
  float row_right_x;
  float cursor_x;
  float cursor_y;
  float row_height;
  float gap;
  bool debug_valid;
  int debug_rect_count;
  GuiLayoutRect debug_rects[128];
} GuiLayoutContext;

int GuiLayoutClampUiScalePercent(int ui_scale_percent);
float GuiLayoutComputeEffectiveScale(int window_width, int window_height,
                                     int ui_scale_percent);
void GuiLayoutComputeMetrics(GuiLayoutMetrics *out, int window_width,
                             int window_height, int ui_scale_percent);
void GuiLayoutComputeMinWindowSize(int *out_width, int *out_height);

void GuiLayoutInit(GuiLayoutContext *ctx, GuiLayoutRect bounds,
                   const GuiLayoutMetrics *metrics);
void GuiLayoutBeginRow(GuiLayoutContext *ctx);
GuiLayoutRect GuiLayoutPlaceFixed(GuiLayoutContext *ctx, float width, float height);
GuiLayoutRect GuiLayoutPlaceFlex(GuiLayoutContext *ctx, float min_width, float height);
void GuiLayoutNextLine(GuiLayoutContext *ctx);
float GuiLayoutMeasureTextWidth(const char *text, const GuiLayoutMetrics *metrics);
float GuiLayoutButtonWidth(const char *text, const GuiLayoutMetrics *metrics,
                           float min_width);
float GuiLayoutRemainingWidth(const GuiLayoutContext *ctx);
bool GuiLayoutContextIsValid(const GuiLayoutContext *ctx);

bool GuiLayoutRectInBounds(GuiLayoutRect outer, GuiLayoutRect inner);
bool GuiLayoutRectsOverlap(GuiLayoutRect a, GuiLayoutRect b);
bool GuiLayoutValidateNoOverlap(const GuiLayoutRect *rects, int rect_count,
                                GuiLayoutRect bounds);

#endif // GUI_LAYOUT_H
