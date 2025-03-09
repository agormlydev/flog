#define WIN32_LEAN_AND_MEAN
#include "flog.h"
#include <Windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static FILE *fout = NULL;
static FlogAttrs fa;
static void get_file_name_timestamp(char **buffer);
static void get_log_entry_timestamp(char **buffer);
static void maybe_get_existing_log_file_and_delete_old_log_files(FILE **f);
int compare_file_attrs(const void *a, const void *b);
typedef struct {
  char name[MAX_PATH];
  FILETIME last_write_time;
  size_t file_size;
} FileAttrs;

#define WARN_LOG_NOT_OPENED(level)                                             \
  "[WARNING] Attempted to write " #level                                       \
  " level log entry when log file was closed or never opened. Log entry can "  \
  "only be made after log_open is called and before log_close is called.\n"

void info_w(wchar_t *s) {
  char *utf8_log_msg;
  int utf8_num_bytes;
  utf8_num_bytes = WideCharToMultiByte(CP_UTF8, 0, s, -1, NULL, 0, NULL, NULL);
  utf8_log_msg = malloc(utf8_num_bytes);
  WideCharToMultiByte(CP_UTF8, 0, s, -1, utf8_log_msg, utf8_num_bytes, NULL,
                      NULL);
  info(utf8_log_msg);
  free(utf8_log_msg);
}

void info(char *s) {
  if (fout == NULL) {
    fprintf(stderr, WARN_LOG_NOT_OPENED(info));
    return;
  }
  char *ts = NULL;
  get_log_entry_timestamp(&ts);
  fprintf(fout, "%s %s %s\n", INFO_TAG, ts, s);
  free(ts);
  // TODO: check return value
  fpos_t fsize;
  // TODO: check return value
  fgetpos(fout, &fsize);
  if (fsize < fa.size_hint_bytes) return;
  printf("[INFO] Rotating log files\n");
  flog_close();
  flog_open(&fa);
}

void flog_open(FlogAttrs *arg_fa) {
  if (fout != NULL) {
    fprintf(stderr, "[WARNING] log_open called when log was already opened. "
                    "Only one log file open at a time is supported. log_close "
                    "should be called before calling log_open again.\n");
    return;
  }
  fa = *arg_fa;
  maybe_get_existing_log_file_and_delete_old_log_files(&fout);
  if (fout != NULL)
    return;
  char *ts = NULL;
  get_file_name_timestamp(&ts);
  char buf[MAX_PATH];
  // TODO: check return value
  sprintf(buf, "%s\\%s%s.%s", fa.dir, fa.name, ts, fa.ext);
  // TODO: check return value
  fopen_s(&fout, buf, "w+");
  free(ts);
}

void flog_close() {
  if (fout == NULL) {
    fprintf(stderr, "[WARNING] log_close called when log was already closed or "
                    "never opened.\n");
    return;
  }
  fclose(fout);
  fout = NULL;
}

/**
 * buffer should be NULL when this function is called
 * buffer will be allocated within this function
 * buffer must be freed by caller
 */
static void get_log_entry_timestamp(char **buffer) {
  char *ts_fmt = "%04d-%02d-%02dT%02d:%02d:%02d.%03d";
  // includes null terminator
  const int ts_capacity = 24;
  *buffer = malloc(ts_capacity);
  SYSTEMTIME st;
  GetLocalTime(&st);

  _snprintf_s(*buffer, ts_capacity, ts_capacity, ts_fmt, st.wYear, st.wMonth,
              st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
}

/**
 * buffer should be NULL when this function is called
 * buffer will be allocated within this function
 * buffer must be freed by caller
 */
static void get_file_name_timestamp(char **buffer) {
  char *ts_fmt = "%04d%02d%02dT%02d%02d%02d%03d";
  // includes null terminator
  const int ts_capacity = 24;
  *buffer = malloc(ts_capacity);
  SYSTEMTIME st;
  GetLocalTime(&st);

  _snprintf_s(*buffer, ts_capacity, ts_capacity, ts_fmt, st.wYear, st.wMonth,
              st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
}

/**
 * f should be NULL when this function is called
 * Checks the log directory for files containing the log name and ending in the
 * ext If there are not any matches, returns without setting f If there are
 * matches, checks the last edited file contains the log name and ends in the
 * ext and if it has a size less than the size_hint_bytes it will be opened (if
 * possible) and returned If there are any errors they will be printed to stderr
 */
static void maybe_get_existing_log_file_and_delete_old_log_files(FILE **f) {
  char source_dir_with_mask[MAX_PATH];
  sprintf_s(source_dir_with_mask, sizeof(source_dir_with_mask) + 1,
            "%s\\*%s*%s", fa.dir, fa.name, fa.ext);
  HANDLE find_handle;
  WIN32_FIND_DATAA find_data;
  find_handle = FindFirstFileA(source_dir_with_mask, &find_data);
  if (find_handle == INVALID_HANDLE_VALUE) {
    return;
  }

  int n_allocated_fa = fa.max_num_logs + 2;
  FileAttrs *existing_log_files = malloc(sizeof(FileAttrs) * (n_allocated_fa));
  int n_log_files = 0;

  do {
    if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
      continue;
    n_log_files++;
    if (n_allocated_fa < n_log_files) {
      existing_log_files =
          realloc(existing_log_files, sizeof(FileAttrs) * n_log_files);
      n_allocated_fa = n_log_files;
    }
    strcpy_s(existing_log_files[n_log_files - 1].name, MAX_PATH,
             find_data.cFileName);
    existing_log_files[n_log_files - 1].last_write_time =
        find_data.ftLastWriteTime;
    existing_log_files[n_log_files - 1].file_size =
        (find_data.nFileSizeHigh * (MAXDWORD + 1)) + find_data.nFileSizeLow;
  } while (FindNextFileA(find_handle, &find_data));
  FindClose(find_handle);
  qsort(existing_log_files, n_log_files, sizeof(FileAttrs), compare_file_attrs);
  FileAttrs last_edited_file = existing_log_files[n_log_files - 1];
  if (n_log_files == 0)
    return;
  // delete existing files
  int n_logs_to_delete = n_log_files - fa.max_num_logs;
  if (last_edited_file.file_size >= fa.size_hint_bytes)
    n_logs_to_delete++;
  if (n_logs_to_delete > 0) {
    // delete one extra if the last file is too large because then we will
    // create a new file
    for (int i = 0; i < n_logs_to_delete; i++) {
      char full_path[MAX_PATH];
      sprintf(full_path, "%s\\%s", fa.dir, existing_log_files[i].name);
      printf("Deleting '%s'\n", full_path);
      // TODO: check return value
      DeleteFileA(full_path);
    }
  }
  if (last_edited_file.file_size >= fa.size_hint_bytes)
    return;
  printf("'%zu' < '%zu'; using existing file '%s'\n",
         last_edited_file.file_size, fa.size_hint_bytes,
         last_edited_file.name);
  char full_path[MAX_PATH];
  // TODO: check return value
  sprintf_s(full_path, MAX_PATH, "%s\\%s", fa.dir, last_edited_file.name);
  fopen_s(f, last_edited_file.name, "a");
}

static int compare_file_attrs(const void *a, const void *b) {
  const FileAttrs *file_a = (const FileAttrs *)a;
  const FileAttrs *file_b = (const FileAttrs *)b;

  return CompareFileTime(&file_a->last_write_time, &file_b->last_write_time);
}
