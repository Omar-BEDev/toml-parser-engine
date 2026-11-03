#ifndef RFC
#define RFC
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* result of reading a whole file: pointer to its first byte and its size */
typedef struct FileInfo {
    void *start;   /* pointer to the first byte of the file content (read-only use) */
    size_t size;   /* file size in bytes */
} FileInfo;

/* Reads the whole file 'path' into a read-only OS memory mapping.
   On success fills *out and returns 1.
   On failure returns 0, leaves *out fields NULL/0, and stores the platform
   error into *err (Windows: GetLastError(); Linux: errno); err may be NULL.

   Zero-length files succeed with out->size == 0 and out->start pointing to a
   single '\0' byte, because an empty file cannot be mapped and an empty TOML
   document is still a valid document.
   Paths are UTF-8 on both platforms; on Windows they are converted to UTF-16
   (invalid UTF-8 fails with an error, paths longer than RFC_UTF16_PATH_MAX
   fail with ERROR_FILENAME_EXCED_RANGE).
   The mapping is never unmapped: out->start stays valid until the process
   exits, when the OS reclaims the whole mapping. */
int read_file_info(const char *path, FileInfo *out, unsigned long *err);

/* Convenience wrapper: like read_file_info but returns the file's start
   pointer directly (NULL on failure) and writes the size into *size when
   size is not NULL. Same lifetime and error behavior as read_file_info. */
void *read_file_start(const char *path, size_t *size, unsigned long *err);

#ifdef __cplusplus
}
#endif

#endif /* RFC */
