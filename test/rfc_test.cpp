#include "../src/io/rfc.h"
#include <gtest/gtest.h>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

const char *kTomlPath = "test.toml";
const char *kEmptyPath = "rfc_test_empty.toml";
const char *kBinaryPath = "rfc_test_binary.bin";
const char *kUtf8Path = "rfc_test_\xc3\xa9\xc3\xa8.toml"; /* é è */
const char *kMissingPath = "rfc_test_no_such_file.toml";

/* ENOENT (Linux) and ERROR_FILE_NOT_FOUND (Windows) are both value 2 */
constexpr unsigned long kNotFound = 2;
#if defined(_WIN32)
constexpr unsigned long kInvalidArg = 87; /* ERROR_INVALID_PARAMETER */
#else
constexpr unsigned long kInvalidArg = 22; /* EINVAL */
#endif

/* Reference read through plain stdio so tests do not trust the code under
   test. Returns false when the file cannot be read. */
bool reference_bytes(const std::filesystem::path &path, std::vector<char> *dst) {
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        return false;
    }
    in.seekg(0, std::ios::end);
    std::streamoff len = in.tellg();
    if (len < 0) {
        return false;
    }
    dst->resize((size_t)len);
    in.seekg(0);
    if (len > 0) {
        in.read(dst->data(), len);
    }
    return in.good() || len == 0;
}

bool reference_bytes(const char *path, std::vector<char> *dst) {
    return reference_bytes(std::filesystem::path(path), dst);
}

void write_bytes(const char *path, const std::vector<char> &content) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    ASSERT_TRUE(out.is_open());
    out.write(content.data(), (std::streamsize)content.size());
    ASSERT_TRUE(out.good());
}

class RfcTest : public ::testing::Test {
  protected:
    void TearDown() override {
        std::error_code ec;
        std::filesystem::remove(std::filesystem::u8path((const char *)kUtf8Path), ec);
        std::remove(kEmptyPath);
        std::remove(kBinaryPath);
    }
};

/* The mapped content must be exactly the file bytes, and the size must be
   the exact file size, including files with embedded '\0' bytes. */
TEST_F(RfcTest, ReadsTomlContent) {
    std::vector<char> expected;
    FileInfo info = {nullptr, 0};
    unsigned long err = 0;

    ASSERT_TRUE(reference_bytes(kTomlPath, &expected));
    ASSERT_EQ(read_file_info(kTomlPath, &info, &err), 1);
    ASSERT_NE(info.start, nullptr);
    ASSERT_EQ(info.size, expected.size());
    EXPECT_EQ(std::memcmp(info.start, expected.data(), expected.size()), 0);
    EXPECT_EQ(err, 0u);
}

/* Random-looking binary content with '\0' bytes must not be truncated:
   pattern hits 0 for every i == multiple of 256 in the first 4096 bytes. */
TEST_F(RfcTest, BinaryFileKeepsNulBytesAndExactSize) {
    std::vector<char> content;
    for (int i = 0; i < 4096; i++) {
        content.push_back((char)((i * 31) % 256));
    }
    std::vector<char> expected;
    FileInfo info = {nullptr, 0};
    unsigned long err = 0;

    write_bytes(kBinaryPath, content);
    ASSERT_TRUE(reference_bytes(kBinaryPath, &expected));
    ASSERT_EQ(read_file_info(kBinaryPath, &info, &err), 1);
    ASSERT_EQ(info.size, content.size());
    EXPECT_EQ(std::memcmp(info.start, expected.data(), expected.size()), 0);
}

/* mmap/MapViewOfFile cannot map a 0-byte file: the reader returns success
   with size 0 and a start pointer to one '\0' byte so the parser sees a
   valid empty document. */
TEST_F(RfcTest, EmptyFileIsSuccessWithZeroSize) {
    std::ofstream empty(kEmptyPath);
    empty.close();
    FileInfo info = {nullptr, 0};
    unsigned long err = 12345;

    ASSERT_EQ(read_file_info(kEmptyPath, &info, &err), 1);
    EXPECT_EQ(info.size, 0u);
    ASSERT_NE(info.start, nullptr);
    const char *bytes = (const char *)info.start;
    EXPECT_EQ(bytes[0], '\0');
}

/* Non-ASCII UTF-8 file names must open on both platforms (the Windows path
   goes through explicit UTF-8 -> UTF-16 conversion, not the ANSI codepage).
   The file is created through std::filesystem::u8path so the name on disk is
   the Unicode path itself; std::ofstream(char*) would re-encode the name
   through the ANSI codepage on Windows and create a differently named file. */
TEST_F(RfcTest, ReadsUtf8FileName) {
    std::filesystem::path utf8_file =
        std::filesystem::u8path((const char *)kUtf8Path);
    {
        std::ofstream file(utf8_file);
        file << "ok";
    }
    std::vector<char> expected;
    FileInfo info = {nullptr, 0};
    unsigned long err = 0;

    ASSERT_TRUE(reference_bytes(utf8_file, &expected));
    ASSERT_EQ(read_file_info(kUtf8Path, &info, &err), 1);
    ASSERT_EQ(info.size, expected.size());
    EXPECT_EQ(std::memcmp(info.start, expected.data(), expected.size()), 0);
    std::filesystem::remove(utf8_file);
}

TEST_F(RfcTest, MissingFileFailsWithNotFound) {
    FileInfo info = {nullptr, 0};
    unsigned long err = 0;

    EXPECT_EQ(read_file_info(kMissingPath, &info, &err), 0);
    EXPECT_EQ(info.start, nullptr);
    EXPECT_EQ(info.size, 0u);
    EXPECT_EQ(err, kNotFound);
}

TEST_F(RfcTest, NullAndEmptyArgumentsAreRejected) {
    FileInfo info = {nullptr, 0};
    unsigned long err = 0;

    EXPECT_EQ(read_file_info(nullptr, &info, &err), 0);
    EXPECT_EQ(info.start, nullptr);
    EXPECT_EQ(info.size, 0u);
    EXPECT_EQ(err, kInvalidArg);

    err = 0;
    info.start = (void *)0x1; /* must be re-cleared by the guard path too */
    info.size = 1;
    EXPECT_EQ(read_file_info("", &info, &err), 0);
    EXPECT_EQ(info.start, nullptr);
    EXPECT_EQ(info.size, 0u);
    EXPECT_EQ(err, kInvalidArg);

    /* caller may pass NULL for err and even for out — no crash, clear error */
    EXPECT_EQ(read_file_info("test.toml", nullptr, nullptr), 0);
    EXPECT_EQ(err, kInvalidArg); /* previous call must not have touched it */

    /* valid path with err == NULL still succeeds and clears stale state */
    EXPECT_EQ(read_file_info("test.toml", &info, nullptr), 1);
    EXPECT_NE(info.start, nullptr);
    EXPECT_NE(info.size, 0u);
}

/* read_file_start must agree with read_file_info: same bytes, same size,
   NULL on the same failures, and no size write when size is NULL. */
TEST_F(RfcTest, StartWrapperMatchesFileInfo) {
    std::vector<char> expected;
    size_t size = 0;
    unsigned long err = 0;

    ASSERT_TRUE(reference_bytes(kTomlPath, &expected));
    void *start = read_file_start(kTomlPath, &size, &err);
    ASSERT_NE(start, nullptr);
    EXPECT_EQ(size, expected.size());
    EXPECT_EQ(err, 0u);
    EXPECT_EQ(std::memcmp(start, expected.data(), expected.size()), 0);

    EXPECT_EQ(read_file_start(kMissingPath, &size, &err), nullptr);
    EXPECT_EQ(size, 0u);
    EXPECT_EQ(err, kNotFound);

    EXPECT_EQ(read_file_start(nullptr, nullptr, nullptr), nullptr);
    EXPECT_EQ(read_file_start("", nullptr, nullptr), nullptr);
}

} /* namespace */