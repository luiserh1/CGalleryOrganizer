#include <stddef.h>
#include <string.h>

#include "gui/frontends/functional/gui_fixed_layout.h"

#define SHELL_MARGIN 12.0f
#define SHELL_GAP 10.0f
#define TITLE_HEIGHT 34.0f
#define TAB_HEIGHT 42.0f
#define PANEL_HEIGHT 510.0f
#define STATUS_HEIGHT 180.0f
#define PANEL_PAD 14.0f

static GuiRect MakeRect(float x, float y, float width, float height) {
  GuiRect rect = {x, y, width, height};
  return rect;
}

void GuiBuildShellLayout(GuiShellLayout *out_layout) {
  if (!out_layout) {
    return;
  }

  memset(out_layout, 0, sizeof(*out_layout));

  out_layout->canvas =
      MakeRect(SHELL_MARGIN, SHELL_MARGIN,
               (float)GUI_FIXED_WINDOW_WIDTH - (SHELL_MARGIN * 2.0f),
               (float)GUI_FIXED_WINDOW_HEIGHT - (SHELL_MARGIN * 2.0f));

  out_layout->title =
      MakeRect(out_layout->canvas.x, out_layout->canvas.y,
               out_layout->canvas.width, TITLE_HEIGHT);

  float tabs_y = out_layout->title.y + out_layout->title.height + SHELL_GAP;
  float tab_w = 170.0f;
  for (int i = 0; i < 4; i++) {
    out_layout->tabs[i] =
        MakeRect(out_layout->canvas.x + (tab_w + SHELL_GAP) * (float)i, tabs_y,
                 tab_w, TAB_HEIGHT);
  }

  float panel_y = tabs_y + TAB_HEIGHT + SHELL_GAP;
  out_layout->panel_outer =
      MakeRect(out_layout->canvas.x, panel_y, out_layout->canvas.width,
               PANEL_HEIGHT);
  out_layout->panel_inner =
      MakeRect(out_layout->panel_outer.x + 1.0f, out_layout->panel_outer.y + 1.0f,
               out_layout->panel_outer.width - 2.0f,
               out_layout->panel_outer.height - 2.0f);

  float status_y = out_layout->panel_outer.y + out_layout->panel_outer.height + SHELL_GAP;
  out_layout->status_outer =
      MakeRect(out_layout->canvas.x, status_y, out_layout->canvas.width,
               STATUS_HEIGHT);
  out_layout->status_inner =
      MakeRect(out_layout->status_outer.x + 1.0f, out_layout->status_outer.y + 1.0f,
               out_layout->status_outer.width - 2.0f,
               out_layout->status_outer.height - 2.0f);

  float sx = out_layout->status_inner.x + PANEL_PAD;
  float sy = out_layout->status_inner.y + PANEL_PAD;
  out_layout->status_label = MakeRect(sx, sy, 115.0f, 28.0f);
  out_layout->status_value = MakeRect(sx + 120.0f, sy, 760.0f, 28.0f);
  out_layout->progress_text = out_layout->status_value;
  out_layout->cancel_button = MakeRect(
      out_layout->status_inner.x + out_layout->status_inner.width - PANEL_PAD - 130.0f,
      sy, 130.0f, 34.0f);

  out_layout->log_box = MakeRect(
      out_layout->status_inner.x + PANEL_PAD, sy + 40.0f,
      out_layout->status_inner.width - PANEL_PAD * 2.0f,
      out_layout->status_inner.height - PANEL_PAD * 2.0f - 34.0f);
}

void GuiBuildScanPanelLayout(GuiRect panel_bounds, GuiScanPanelLayout *out_layout) {
  if (!out_layout) {
    return;
  }

  memset(out_layout, 0, sizeof(*out_layout));

  float x = panel_bounds.x + PANEL_PAD;
  float y = panel_bounds.y + PANEL_PAD;
  float panel_right = panel_bounds.x + panel_bounds.width - PANEL_PAD;
  float path_label_w = 170.0f;
  float path_input_w = panel_right - (x + path_label_w + SHELL_GAP);

  out_layout->gallery_label = MakeRect(x, y, path_label_w, 30.0f);
  out_layout->gallery_input =
      MakeRect(x + path_label_w + SHELL_GAP, y, path_input_w, 34.0f);

  y += 44.0f;
  out_layout->env_label = MakeRect(x, y, path_label_w, 30.0f);
  out_layout->env_input =
      MakeRect(x + path_label_w + SHELL_GAP, y, path_input_w, 34.0f);

  y += 46.0f;
  out_layout->exhaustive = MakeRect(x, y, 260.0f, 30.0f);

  y += 46.0f;
  out_layout->jobs_label = MakeRect(x, y, 50.0f, 30.0f);
  out_layout->jobs_input = MakeRect(x + 55.0f, y, 90.0f, 34.0f);

  out_layout->cache_label = MakeRect(x + 160.0f, y, 180.0f, 30.0f);
  out_layout->cache_none = MakeRect(x + 345.0f, y, 95.0f, 38.0f);
  out_layout->cache_zstd = MakeRect(x + 450.0f, y, 95.0f, 38.0f);

  out_layout->level_label = MakeRect(x + 560.0f, y, 60.0f, 30.0f);
  out_layout->level_input = MakeRect(x + 625.0f, y, 90.0f, 34.0f);

  y += 56.0f;
  out_layout->actions_divider = MakeRect(x, y - 8.0f, panel_right - x, 1.0f);

  out_layout->scan_button = MakeRect(x, y, 170.0f, 42.0f);
  out_layout->ml_button = MakeRect(x + 180.0f, y, 170.0f, 42.0f);
  out_layout->download_models_button = MakeRect(x + 360.0f, y, 190.0f, 42.0f);
  out_layout->save_paths_button = MakeRect(x + 560.0f, y, 170.0f, 42.0f);
  out_layout->reset_paths_button = MakeRect(x + 740.0f, y, 210.0f, 42.0f);

  y += 54.0f;
  out_layout->info_label = MakeRect(x, y, 1080.0f, 30.0f);
}

void GuiBuildSimilarityPanelLayout(GuiRect panel_bounds,
                                   GuiSimilarityPanelLayout *out_layout) {
  if (!out_layout) {
    return;
  }

  memset(out_layout, 0, sizeof(*out_layout));

  float x = panel_bounds.x + PANEL_PAD;
  float y = panel_bounds.y + PANEL_PAD;

  out_layout->threshold_label = MakeRect(x, y, 230.0f, 30.0f);
  out_layout->threshold_input = MakeRect(x + 235.0f, y, 120.0f, 34.0f);
  out_layout->topk_label = MakeRect(x + 380.0f, y, 60.0f, 30.0f);
  out_layout->topk_input = MakeRect(x + 445.0f, y, 120.0f, 34.0f);

  y += 46.0f;
  out_layout->incremental = MakeRect(x, y, 260.0f, 30.0f);

  y += 46.0f;
  out_layout->memory_label = MakeRect(x, y, 130.0f, 30.0f);
  out_layout->mode_chunked = MakeRect(x + 135.0f, y, 120.0f, 38.0f);
  out_layout->mode_eager = MakeRect(x + 265.0f, y, 120.0f, 38.0f);

  y += 52.0f;
  out_layout->run_button = MakeRect(x, y, 260.0f, 42.0f);

  y += 54.0f;
  out_layout->info_label = MakeRect(x, y, 760.0f, 30.0f);
}

void GuiBuildOrganizePanelLayout(GuiRect panel_bounds,
                                 GuiOrganizePanelLayout *out_layout) {
  if (!out_layout) {
    return;
  }

  memset(out_layout, 0, sizeof(*out_layout));

  float x = panel_bounds.x + PANEL_PAD;
  float y = panel_bounds.y + PANEL_PAD;

  out_layout->group_label = MakeRect(x, y, 130.0f, 30.0f);
  out_layout->group_input = MakeRect(x + 135.0f, y, 420.0f, 34.0f);

  y += 50.0f;
  out_layout->preview_button = MakeRect(x, y, 220.0f, 42.0f);
  out_layout->execute_button = MakeRect(x + 230.0f, y, 220.0f, 42.0f);
  out_layout->rollback_button = MakeRect(x + 460.0f, y, 160.0f, 42.0f);

  y += 56.0f;
  out_layout->info_label = MakeRect(x, y, 780.0f, 30.0f);
}

void GuiBuildDuplicatesPanelLayout(GuiRect panel_bounds,
                                   GuiDuplicatesPanelLayout *out_layout) {
  if (!out_layout) {
    return;
  }

  memset(out_layout, 0, sizeof(*out_layout));

  float x = panel_bounds.x + PANEL_PAD;
  float y = panel_bounds.y + PANEL_PAD;

  out_layout->info_top = MakeRect(x, y, 520.0f, 30.0f);

  y += 48.0f;
  out_layout->analyze_button = MakeRect(x, y, 240.0f, 42.0f);
  out_layout->move_button = MakeRect(x + 250.0f, y, 260.0f, 42.0f);

  y += 56.0f;
  out_layout->info_bottom = MakeRect(x, y, 420.0f, 30.0f);
}

bool GuiRectInBounds(GuiRect outer, GuiRect inner) {
  return inner.x >= outer.x && inner.y >= outer.y &&
         inner.x + inner.width <= outer.x + outer.width &&
         inner.y + inner.height <= outer.y + outer.height;
}

bool GuiRectsOverlap(GuiRect a, GuiRect b) {
  bool separated = a.x + a.width <= b.x || b.x + b.width <= a.x ||
                   a.y + a.height <= b.y || b.y + b.height <= a.y;
  return !separated;
}

bool GuiValidateNoOverlap(GuiRect outer, const GuiRect *rects, int rect_count) {
  if (!rects || rect_count < 0) {
    return false;
  }

  for (int i = 0; i < rect_count; i++) {
    if (!GuiRectInBounds(outer, rects[i])) {
      return false;
    }
  }

  for (int i = 0; i < rect_count; i++) {
    for (int j = i + 1; j < rect_count; j++) {
      if (GuiRectsOverlap(rects[i], rects[j])) {
        return false;
      }
    }
  }

  return true;
}
