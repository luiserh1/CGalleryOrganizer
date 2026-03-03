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
  GuiRect tabs[5];
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
  GuiRect gallery_pick_button;
  GuiRect env_label;
  GuiRect env_input;
  GuiRect env_pick_button;
  GuiRect exhaustive;
  GuiRect jobs_label;
  GuiRect jobs_input;
  GuiRect cache_label;
  GuiRect cache_none;
  GuiRect cache_zstd;
  GuiRect level_label;
  GuiRect level_input;
  GuiRect actions_divider;
  GuiRect scan_button;
  GuiRect ml_button;
  GuiRect download_models_button;
  GuiRect save_paths_button;
  GuiRect reset_paths_button;
  GuiRect info_label;
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

typedef struct {
  GuiRect pattern_label;
  GuiRect pattern_input;
  GuiRect tags_map_label;
  GuiRect tags_map_input;
  GuiRect tags_map_pick_button;
  GuiRect tags_map_bootstrap_button;
  GuiRect tag_add_label;
  GuiRect tag_add_input;
  GuiRect tag_remove_label;
  GuiRect tag_remove_input;
  GuiRect meta_tag_add_label;
  GuiRect meta_tag_add_input;
  GuiRect meta_tag_remove_label;
  GuiRect meta_tag_remove_input;
  GuiRect preview_id_label;
  GuiRect preview_id_input;
  GuiRect preview_latest_button;
  GuiRect operation_id_label;
  GuiRect operation_id_input;
  GuiRect history_latest_button;
  GuiRect accept_suffix;
  GuiRect filter_collisions;
  GuiRect filter_warnings;
  GuiRect preview_button;
  GuiRect apply_button;
  GuiRect history_button;
  GuiRect detail_button;
  GuiRect redo_button;
  GuiRect rollback_button;
  GuiRect selected_tags_label;
  GuiRect selected_tags_input;
  GuiRect selected_tags_apply_button;
  GuiRect selected_meta_tags_label;
  GuiRect selected_meta_tags_input;
  GuiRect selected_meta_tags_apply_button;
  GuiRect preview_table;
  GuiRect info_label;
} GuiRenamePanelLayout;

void GuiBuildShellLayout(GuiShellLayout *out_layout);
void GuiBuildScanPanelLayout(GuiRect panel_bounds, GuiScanPanelLayout *out_layout);
void GuiBuildSimilarityPanelLayout(GuiRect panel_bounds,
                                   GuiSimilarityPanelLayout *out_layout);
void GuiBuildOrganizePanelLayout(GuiRect panel_bounds,
                                 GuiOrganizePanelLayout *out_layout);
void GuiBuildDuplicatesPanelLayout(GuiRect panel_bounds,
                                   GuiDuplicatesPanelLayout *out_layout);
void GuiBuildRenamePanelLayout(GuiRect panel_bounds,
                               GuiRenamePanelLayout *out_layout);

bool GuiRectInBounds(GuiRect outer, GuiRect inner);
bool GuiRectsOverlap(GuiRect a, GuiRect b);
bool GuiValidateNoOverlap(GuiRect outer, const GuiRect *rects, int rect_count);

#endif // GUI_FIXED_LAYOUT_H
