#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "cJSON.h"
#include "cli/cli_app_error.h"
#include "cli/cli_args.h"
#include "cli/cli_rename_commands.h"
#include "cli/cli_rename_history_commands.h"
#include "cli/cli_rename_utils.h"

static void PrintRenameMetadataFields(const char *details_json);
static bool SaveTextFile(const char *path, const char *text);

static int HandleRenameInit(const char *target_dir, const char *env_dir,
                            const char *argv0) {
  if (!target_dir || target_dir[0] == '\0' || !env_dir || env_dir[0] == '\0') {
    printf("Error: --rename-init requires <target_dir> <env_dir>.\n");
    CliPrintUsage(argv0);
    return 1;
  }

  CliRenameInitResult result = {0};
  char error[APP_MAX_ERROR] = {0};
  if (!CliRenameInitEnvironment(target_dir, env_dir, &result, error,
                                sizeof(error))) {
    printf("Error: Rename init failed: %s\n", error);
    if (strstr(error, "does not exist") != NULL) {
      char suggestion[MAX_PATH_LENGTH] = {0};
      if (CliRenameSuggestPath(target_dir, suggestion, sizeof(suggestion))) {
        printf("Hint: did you mean '%s'?\n", suggestion);
      }
    }
    return 1;
  }

  printf("Rename init ready.\n");
  printf("Target (abs): %s\n", result.target_abs);
  printf("Env (abs): %s\n", result.env_abs);
  printf("Files in scope: %d\n", result.media_files_in_scope);
  printf("Cache layout: %s/.cache/{rename_previews,rename_history}\n",
         result.env_abs);
  return 0;
}

static int HandleRenameBootstrap(const char *target_dir, const char *env_dir,
                                 const char *argv0) {
  if (!target_dir || target_dir[0] == '\0' || !env_dir || env_dir[0] == '\0') {
    printf("Error: --rename-bootstrap-tags-from-filename requires <target_dir> "
           "<env_dir>.\n");
    CliPrintUsage(argv0);
    return 1;
  }

  CliRenameBootstrapResult result = {0};
  char error[APP_MAX_ERROR] = {0};
  if (!CliRenameBootstrapTagsFromFilename(target_dir, env_dir, &result, error,
                                          sizeof(error))) {
    printf("Error: rename bootstrap failed: %s\n", error);
    if (strstr(error, "does not exist") != NULL) {
      char suggestion[MAX_PATH_LENGTH] = {0};
      if (CliRenameSuggestPath(target_dir, suggestion, sizeof(suggestion))) {
        printf("Hint: did you mean '%s'?\n", suggestion);
      }
    }
    return 1;
  }

  printf("Rename tag bootstrap completed.\n");
  printf("Tags map: %s\n", result.map_path);
  printf("Files scanned: %d\n", result.files_scanned);
  printf("Files with inferred tags: %d\n", result.files_with_tags);
  printf("Use with preview: --rename-tags-map \"%s\"\n", result.map_path);
  return 0;
}

static int HandleRenamePreview(AppContext *app, const char *target_dir,
                               const char *env_dir, const char *pattern,
                               const char *tags_map_path,
                               const char *tag_add_csv,
                               const char *tag_remove_csv,
                               const char *meta_tag_add_csv,
                               const char *meta_tag_remove_csv,
                               bool show_metadata_fields,
                               bool preview_json,
                               const char *preview_json_out,
                               const char *argv0) {
  if (!target_dir || target_dir[0] == '\0' || !env_dir || env_dir[0] == '\0') {
    printf("Error: --rename-preview requires <target_dir> <env_dir>.\n");
    CliPrintUsage(argv0);
    return 1;
  }

  struct stat st;
  if (stat(target_dir, &st) != 0 || !S_ISDIR(st.st_mode)) {
    printf("Error: Failed to generate rename preview: target directory '%s' "
           "does not exist.\n",
           target_dir);
    char suggestion[MAX_PATH_LENGTH] = {0};
    if (CliRenameSuggestPath(target_dir, suggestion, sizeof(suggestion))) {
      printf("Hint: did you mean '%s'?\n", suggestion);
    }
    return 1;
  }

  AppRenamePreviewRequest request = {
      .target_dir = target_dir,
      .env_dir = env_dir,
      .pattern = pattern,
      .tags_map_json_path = tags_map_path,
      .tag_add_csv = tag_add_csv,
      .tag_remove_csv = tag_remove_csv,
      .meta_tag_add_csv = meta_tag_add_csv,
      .meta_tag_remove_csv = meta_tag_remove_csv,
      .hooks = {0},
  };
  AppRenamePreviewResult result = {0};

  AppStatus status = AppPreviewRename(app, &request, &result);
  if (status != APP_STATUS_OK) {
    CliPrintAppError(app, "Error: Failed to generate rename preview");
    AppFreeRenamePreviewResult(&result);
    return 1;
  }

  printf("Rename preview ready.\n");
  printf("Preview ID: %s\n", result.preview_id);
  printf("Preview artifact: %s\n", result.preview_path);
  printf("Pattern: %s\n", result.effective_pattern);
  printf("Files in scope: %d\n", result.file_count);
  printf("Collision groups: %d\n", result.collision_group_count);
  printf("Colliding items: %d\n", result.collision_count);
  printf("Truncated names: %d\n", result.truncation_count);
  if (result.requires_auto_suffix_acceptance) {
    printf("Apply guard: collisions detected; --rename-accept-auto-suffix "
           "is required on apply.\n");
  }
  if (preview_json_out && preview_json_out[0] != '\0') {
    if (!result.details_json || result.details_json[0] == '\0') {
      printf("Warning: preview details JSON was empty; nothing written to '%s'.\n",
             preview_json_out);
    } else if (!SaveTextFile(preview_json_out, result.details_json)) {
      printf("Error: Failed to write preview JSON to '%s'.\n", preview_json_out);
      AppFreeRenamePreviewResult(&result);
      return 1;
    } else {
      printf("Preview JSON saved: %s\n", preview_json_out);
    }
  }
  if (preview_json && result.details_json && result.details_json[0] != '\0') {
    printf("\n--- Preview Details (JSON) ---\n%s\n", result.details_json);
  } else if (!preview_json) {
    printf("Hint: use --rename-preview-json or --rename-preview-json-out <path> "
           "for full JSON details.\n");
  }
  if (show_metadata_fields) {
    PrintRenameMetadataFields(result.details_json);
  }

  AppFreeRenamePreviewResult(&result);
  return 0;
}

static int HandleRenameApply(AppContext *app, const char *target_dir,
                             const char *env_dir,
                             const char *rename_from_preview,
                             bool accept_auto_suffix, const char *argv0) {
  const char *apply_env = CliResolveRollbackEnvDir(target_dir, env_dir);
  if (!apply_env) {
    printf("Error: --rename-apply requires an environment directory.\n");
    CliPrintUsage(argv0);
    return 1;
  }
  if (!rename_from_preview || rename_from_preview[0] == '\0') {
    printf("Error: --rename-apply requires --rename-from-preview <preview_id>.\n");
    return 1;
  }

  AppRenameApplyRequest request = {
      .env_dir = apply_env,
      .preview_id = rename_from_preview,
      .accept_auto_suffix = accept_auto_suffix,
      .hooks = {0},
  };
  AppRenameApplyResult result = {0};

  AppStatus status = AppApplyRename(app, &request, &result);
  if (status != APP_STATUS_OK) {
    CliPrintAppError(app, "Error: Rename apply failed");
    return 1;
  }

  printf("Rename apply completed.\n");
  printf("Operation ID: %s\n", result.operation_id);
  printf("Renamed: %d\n", result.renamed_count);
  printf("Skipped: %d\n", result.skipped_count);
  printf("Failed: %d\n", result.failed_count);
  printf("Collision resolutions: %d\n", result.collision_resolved_count);
  printf("Truncated names in plan: %d\n", result.truncation_count);
  if (result.auto_suffix_applied) {
    printf("Auto suffix policy applied: yes\n");
  }

  return 0;
}

static void PrintRenameMetadataFields(const char *details_json) {
  if (!details_json || details_json[0] == '\0') {
    printf("Metadata tag fields in scope: unavailable (no preview JSON details).\n");
    return;
  }

  cJSON *root = cJSON_Parse(details_json);
  if (!root) {
    printf("Metadata tag fields in scope: unavailable (preview JSON parse failed).\n");
    return;
  }

  cJSON *fields = cJSON_GetObjectItem(root, "metadataTagFields");
  if (!cJSON_IsArray(fields) || cJSON_GetArraySize(fields) <= 0) {
    printf("Metadata tag fields in scope: none detected from whitelist.\n");
    cJSON_Delete(root);
    return;
  }

  int count = cJSON_GetArraySize(fields);
  printf("Metadata tag fields in scope (%d):\n", count);
  for (int i = 0; i < count; i++) {
    cJSON *field = cJSON_GetArrayItem(fields, i);
    if (!cJSON_IsString(field) || !field->valuestring ||
        field->valuestring[0] == '\0') {
      continue;
    }
    printf("  - %s\n", field->valuestring);
  }

  cJSON_Delete(root);
}

static bool SaveTextFile(const char *path, const char *text) {
  if (!path || path[0] == '\0' || !text) {
    return false;
  }

  FILE *f = fopen(path, "wb");
  if (!f) {
    return false;
  }

  bool ok = fputs(text, f) >= 0;
  fclose(f);
  return ok;
}

int CliRunRenameActions(AppContext *app, const CliMainOptions *options,
                        const CliRenameHistoryFilter *rename_history_filter,
                        const char *argv0) {
  if (!app || !options || !rename_history_filter || !argv0) {
    return 1;
  }

  if (options->rename_init) {
    return HandleRenameInit(options->target_dir, options->env_dir, argv0);
  }
  if (options->rename_bootstrap_tags_from_filename) {
    return HandleRenameBootstrap(options->target_dir, options->env_dir, argv0);
  }
  if (options->rename_preview) {
    return HandleRenamePreview(
        app, options->target_dir, options->env_dir, options->rename_pattern,
        options->rename_tags_map_path, options->rename_tag_add_csv,
        options->rename_tag_remove_csv, options->rename_meta_tag_add_csv,
        options->rename_meta_tag_remove_csv, options->rename_metadata_fields,
        options->rename_preview_json, options->rename_preview_json_out, argv0);
  }
  if (options->rename_apply_action) {
    const char *resolved_preview_id = options->rename_from_preview;
    char latest_preview_id[64] = {0};
    char operation_preview_id[64] = {0};
    if (options->rename_apply_latest) {
      const char *apply_env =
          CliResolveRollbackEnvDir(options->target_dir, options->env_dir);
      if (!apply_env) {
        printf("Error: --rename-apply-latest requires an environment directory.\n");
        return 1;
      }
      char resolve_error[APP_MAX_ERROR] = {0};
      if (!CliRenameResolveLatestPreviewId(apply_env, latest_preview_id,
                                           sizeof(latest_preview_id),
                                           resolve_error,
                                           sizeof(resolve_error))) {
        printf("Error: --rename-apply-latest failed: %s\n", resolve_error);
        return 1;
      }
      printf("Resolved latest preview id: %s\n", latest_preview_id);
      resolved_preview_id = latest_preview_id;
    }
    if (options->rename_redo) {
      const char *apply_env =
          CliResolveRollbackEnvDir(options->target_dir, options->env_dir);
      if (!apply_env) {
        printf("Error: --rename-redo requires an environment directory.\n");
        return 1;
      }
      char resolve_error[APP_MAX_ERROR] = {0};
      if (!CliRenameResolvePreviewIdFromOperation(
              apply_env, options->rename_redo_operation, operation_preview_id,
              sizeof(operation_preview_id), resolve_error,
              sizeof(resolve_error))) {
        printf("Error: --rename-redo failed: %s\n", resolve_error);
        return 1;
      }
      printf("Resolved preview id from operation %s: %s\n",
             options->rename_redo_operation, operation_preview_id);
      resolved_preview_id = operation_preview_id;
    }

    return HandleRenameApply(app, options->target_dir, options->env_dir,
                             resolved_preview_id,
                             options->rename_accept_auto_suffix, argv0);
  }
  if (options->rename_history) {
    return CliHandleRenameHistory(app, options->target_dir, options->env_dir,
                                  rename_history_filter, argv0);
  }
  if (options->rename_history_export) {
    return CliHandleRenameHistoryExport(
        app, options->target_dir, options->env_dir, rename_history_filter,
        options->rename_history_export_path, argv0);
  }
  if (options->rename_history_prune) {
    return CliHandleRenameHistoryPrune(app, options->target_dir, options->env_dir,
                                       options->rename_history_prune_keep_count,
                                       argv0);
  }
  if (options->rename_history_latest_id) {
    return CliHandleRenameHistoryLatestId(options->target_dir, options->env_dir,
                                          argv0);
  }
  if (options->rename_history_detail) {
    return CliHandleRenameHistoryDetail(
        app, options->target_dir, options->env_dir,
        options->rename_history_detail_operation, argv0);
  }
  if (options->rename_preview_latest_id) {
    return CliHandleRenamePreviewLatestId(options->target_dir, options->env_dir,
                                          argv0);
  }
  if (options->rename_rollback) {
    return CliHandleRenameRollback(app, options->target_dir, options->env_dir,
                                   options->rename_rollback_operation, argv0);
  }
  if (options->rename_rollback_preflight) {
    return CliHandleRenameRollbackPreflight(
        app, options->target_dir, options->env_dir,
        options->rename_rollback_preflight_operation, argv0);
  }

  printf("Error: No rename action selected.\n");
  return 1;
}
