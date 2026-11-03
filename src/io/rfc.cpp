#include "rfc.h"
#include <stdint.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#ifdef _WIN32
#define RFC_INVALID_ARGUMENT ((unsigned long)ERROR_INVALID_PARAMETER)
#else
#define RFC_INVALID_ARGUMENT ((unsigned long)EINVAL)
#endif

/* One byte holding '\0': the start pointer returned for zero-length files,
   which mmap/MapViewOfFile cannot map. A parse of it yields an empty document
   on both platforms. Any write through it is a real bug and ASan reports it
   as a global-buffer-overflow, so no corruption can hide here silently. */
static char rfc_empty_file_content[1] = "";

/* Records a failure and guarantees the documented NULL/0 output state.
   Helpers may pass out == NULL because read_file_info already cleared it. */
static int rfc_fail(FileInfo *out, unsigned long *err, unsigned long error_code)
{
    if (out != NULL) {
        out->start = NULL;
        out->size = 0;
    }
    if (err != NULL) {
        *err = error_code;
    }
    return 0;
}

void *read_file_start(const char *path, size_t *size, unsigned long *err)
{
    FileInfo info;

    if (read_file_info(path, &info, err) == 0) {
        if (size != NULL) {
            *size = 0;
        }
        return NULL;
    }
    if (size != NULL) {
        *size = info.size;
    }
    return info.start;
}

#ifdef _WIN32

#define RFC_UTF16_PATH_MAX 1024

/* Converts a UTF-8 path to UTF-16 for CreateFileW, so file names behave the
   same as on Linux with UTF-8 paths instead of depending on the ANSI
   codepage of the machine. Rejects invalid UTF-8 and overlong paths. */
static int win_utf8_path(const char *path, wchar_t *utf16_path,
                         unsigned long *err)
{
    int wide_len;

    wide_len = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1,
                                   NULL, 0);
    if (wide_len <= 0) {
        return rfc_fail(NULL, err, (unsigned long)GetLastError());
    }
    if (wide_len > RFC_UTF16_PATH_MAX) {
        return rfc_fail(NULL, err, (unsigned long)ERROR_FILENAME_EXCED_RANGE);
    }
    (void)MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1,
                              utf16_path, RFC_UTF16_PATH_MAX);
    return 1;
}

/* Reads and validates the size. Empty files are allowed here and are handled
   by the caller; sizes that do not fit size_t are rejected. */
static int win_file_size(HANDLE file, size_t *size, DWORD *error)
{
    LARGE_INTEGER file_size;

    if (!GetFileSizeEx(file, &file_size)) {
        *error = GetLastError();
        return 0;
    }
    if (file_size.QuadPart < 0) {
        *error = ERROR_INVALID_DATA;
        return 0;
    }
#if SIZE_MAX < UINT64_MAX
    if ((uint64_t)file_size.QuadPart > (uint64_t)SIZE_MAX) {
        *error = ERROR_FILE_TOO_LARGE;
        return 0;
    }
#endif
    *size = (size_t)file_size.QuadPart;
    return 1;
}

/* Creates the mapping and its view, then closes the mapping handle. Win32
   keeps the view valid until UnmapViewOfFile, even after the mapping and
   file handles are closed (the system holds the file open while mapped). */
static int win_map_file(HANDLE file, void **view, unsigned long *err)
{
    HANDLE mapping;
    void *mapped;
    DWORD saved_error;

    mapping = CreateFileMappingW(file, NULL, PAGE_READONLY, 0, 0, NULL);
    if (mapping == NULL) {
        return rfc_fail(NULL, err, (unsigned long)GetLastError());
    }
    mapped = MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0);
    if (mapped == NULL) {
        saved_error = GetLastError();
        (void)CloseHandle(mapping);
        return rfc_fail(NULL, err, (unsigned long)saved_error);
    }
    if (!CloseHandle(mapping)) {
        saved_error = GetLastError();
        (void)UnmapViewOfFile(mapped);
        return rfc_fail(NULL, err, (unsigned long)saved_error);
    }
    *view = mapped;
    return 1;
}

static int read_file_info_windows(const char *path, FileInfo *out,
                                  unsigned long *err)
{
    wchar_t utf16_path[RFC_UTF16_PATH_MAX];
    HANDLE file;
    void *view = NULL;
    size_t size = 0;
    DWORD saved_error;

    if (!win_utf8_path(path, utf16_path, err)) {
        return 0;
    }
    file = CreateFileW(utf16_path, GENERIC_READ, FILE_SHARE_READ, NULL,
                       OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return rfc_fail(out, err, (unsigned long)GetLastError());
    }
    if (!win_file_size(file, &size, &saved_error)) {
        (void)CloseHandle(file);
        return rfc_fail(out, err, (unsigned long)saved_error);
    }
    if (size == 0) {
        (void)CloseHandle(file);
        out->start = rfc_empty_file_content;
        out->size = 0;
        return 1;
    }
    if (!win_map_file(file, &view, err)) {
        (void)CloseHandle(file);
        return 0;
    }
    if (!CloseHandle(file)) {
        saved_error = GetLastError();
        (void)UnmapViewOfFile(view);
        return rfc_fail(out, err, (unsigned long)saved_error);
    }
    out->start = view;
    out->size = size;
    return 1;
}

#else

/* Converts st_size to size_t. Returns 0 on success, otherwise an errno value:
   EINVAL for a negative size and EOVERFLOW when the size does not fit
   into size_t. Zero-length is allowed here and handled by the caller. */
static int posix_stat_size(const struct stat *st, size_t *size)
{
    if (st->st_size < 0) {
        return EINVAL;
    }
#if SIZE_MAX < UINT64_MAX
    if ((uintmax_t)st->st_size > (uintmax_t)SIZE_MAX) {
        return EOVERFLOW;
    }
#endif
    *size = (size_t)st->st_size;
    return 0;
}

/* Opens and stats the file. On success the descriptor stays open and is
   owned by the caller. */
static int posix_open_file(const char *path, int *fd, size_t *size,
                           unsigned long *err)
{
    int descriptor;
    struct stat st;
    int size_error;

    descriptor = open(path, O_RDONLY);
    if (descriptor < 0) {
        return rfc_fail(NULL, err, (unsigned long)errno);
    }
    if (fstat(descriptor, &st) != 0) {
        size_error = errno;
        (void)close(descriptor);
        return rfc_fail(NULL, err, (unsigned long)size_error);
    }
    size_error = posix_stat_size(&st, size);
    if (size_error != 0) {
        (void)close(descriptor);
        return rfc_fail(NULL, err, (unsigned long)size_error);
    }
    *fd = descriptor;
    return 1;
}

static int posix_map_file(int fd, size_t size, void **view, unsigned long *err)
{
    void *mapped = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);

    if (mapped == MAP_FAILED) {
        return rfc_fail(NULL, err, (unsigned long)errno);
    }
    *view = mapped;
    return 1;
}

static int read_file_info_posix(const char *path, FileInfo *out,
                                unsigned long *err)
{
    int fd = -1;
    size_t size = 0;
    void *view = NULL;
    int saved_errno;

    if (!posix_open_file(path, &fd, &size, err)) {
        return 0;
    }
    if (size == 0) {
        (void)close(fd);
        out->start = rfc_empty_file_content;
        out->size = 0;
        return 1;
    }
    if (!posix_map_file(fd, size, &view, err)) {
        (void)close(fd);
        return 0;
    }
    if (close(fd) != 0) {
        saved_errno = errno;
        (void)munmap(view, size);
        return rfc_fail(out, err, (unsigned long)saved_errno);
    }
    out->start = view;
    out->size = size;
    return 1;
}

#endif

int read_file_info(const char *path, FileInfo *out, unsigned long *err)
{
    if (path == NULL || path[0] == '\0' || out == NULL) {
        return rfc_fail(out, err, RFC_INVALID_ARGUMENT);
    }
    out->start = NULL;
    out->size = 0;
#ifdef _WIN32
    return read_file_info_windows(path, out, err);
#else
    return read_file_info_posix(path, out, err);
#endif
}
