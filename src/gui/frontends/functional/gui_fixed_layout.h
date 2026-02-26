#ifndef GUI_FIXED_LAYOUT_H
#define GUI_FIXED_LAYOUT_H

#include <stdbool.h>

#define GUI_FIXED_WINDOW_WIDTH 1280
#define GUI_FIXED_WINDOW_HEIGHT 820

typedef struct {
  float x;
  float y;
  float width;
  float height;
} GuiRect;

typedef struct {
  GuiRect canvas;
  GuiRect title;
  GuiRect tabs[4];
  GuiRect panel_outer;
  GuiRect panel_inner;
  GuiRect status_outer;
  GuiRect status_inner;
  GuiRect status_label;
  GuiRect status_value;
  GuiRect progress_text;
  GuiRect cancel_button;
  GuiRect log_box;
} GuiShellLayout;

typedef struct {
  GuiRect gallery_label;
  GuiRect gallery_input;
  GuiRect env_label;
  GuiRect env_input;
  GuiRect exhaustive;
  GuiRect jobs_label;
  GuiRect jobs_input;
  GuiRect cache_label;
  GuiRect cache_none;
  GuiRect cache_zstd;
  GuiRect cache_auto;
  GuiRect level_label;
  GuiRect level_input;
  GuiRect actions_divider;
  GuiRect scan_button;
  GuiRect ml_button;
  GuiRect save_paths_button;
  GuiRect reset_paths_button;
} GuiScanPanelLayout;

typedef struct {
  GuiRect threshold_label;
  GuiRect threshold_input;
  GuiRect topk_label;
  GuiRect topk_input;
  GuiRect incremental;
  GuiRect memory_label;
  GuiRect mode_chunked;
  GuiRect mode_eager;
  GuiRect run_button;
  GuiRect info_label;
} GuiSimilarityPanelLayout;

typedef struct {
  GuiRect group_label;
  GuiRect group_input;
  GuiRect preview_button;
  GuiRect execute_button;
  GuiRect rollback_button;
  GuiRect info_label;
} GuiOrganizePanelLayout;

typedef struct {
  GuiRect info_top;
  GuiRect analyze_button;
  GuiRect move_button;
  GuiRect info_bottom;
} GuiDuplicatesPanelLayout;

void GuiBuildShellLayout(GuiShellLayout *out_layout);
void GuiBuildScanPanelLayout(GuiRect panel_bounds, GuiScanPanelLayout *out_layout);
void GuiBuildSimilarityPanelLayout(GuiRect panel_bounds,
                                   GuiSimilarityPanelLayout *out_layout);
void GuiBuildOrganizePanelLayout(GuiRect panel_bounds,
                                 GuiOrganizePanelLayout *out_layout);
void GuiBuildDuplicatesPanelLayout(GuiRect panel_bounds,
                                   GuiDuplicatesPanelLayout *out_layout);

bool GuiRectInBounds(GuiRect outer, GuiRect inner);
bool GuiRectsOverlap(GuiRect a, GuiRect b);
bool GuiValidateNoOverlap(GuiRect outer, const GuiRect *rects, int rect_count);

#endif // GUI_FIXED_LAYOUT_H
