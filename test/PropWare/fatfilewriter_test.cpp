/**
 * @file    fatfilewriter_test.cpp
 *
 * @author  David Zemon
 *
 * Prerequisites:
 *      - SD card connected with the following pins:
 *          - MOSI = P0
 *          - MISO = P1
 *          - SCLK = P2
 *          - CS   = P4
 *      - FAT16 or FAT32 Filesystem on the first partition of the SD card
 *      - File named "fat_test.txt" in this directory should be loaded into the root directory
 *
 * @copyright
 * The MIT License (MIT)<br>
 * <br>Copyright (c) 2013 David Zemon<br>
 * <br>Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:<br>
 * <br>The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.<br>
 * <br>THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "PropWareTests.h"
#include <PropWare/memory/sd.h>
#include <PropWare/filesystem/fat/fatfs.h>
#include <PropWare/filesystem/fat/fatfilewriter.h>
#include <PropWare/filesystem/fat/fatfilereader.h>

using PropWare::SD;
using PropWare::SPI;
using PropWare::Filesystem;
using PropWare::FatFS;
using PropWare::FatFileWriter;
using PropWare::FatFileReader;
using PropWare::BlockStorage;
using PropWare::File;

static const char             EXISTING_FILE[]       = "fat_test.txt";
static const char             EXISTING_FILE_UPPER[] = "FAT_TEST.TXT";
static const char             NEW_FILE_NAME[]       = "new_test.txt";
static SD                     g_driver;
static FatFS                  g_fs(g_driver);
static FatFileWriter          *testable;
static uint8_t                *dataBuffer;
static BlockStorage::Buffer   buffer;
static BlockStorage::MetaData bufferMeta;

void error_checker (const PropWare::ErrorCode err) {
    if (err) {
        if (SPI::BEG_ERROR <= err && err <= SPI::END_ERROR)
            SPI::get_instance().print_error_str(pwOut, (const SPI::ErrorCode) err);
        else if (SD::BEG_ERROR <= err && err <= SD::END_ERROR)
            ((SD *) g_fs.m_driver)->print_error_str(pwOut, (const SD::ErrorCode) err);
        else if (Filesystem::BEG_ERROR <= err && err <= Filesystem::END_ERROR)
            FatFS::print_error_str(pwOut, (const Filesystem::ErrorCode) err);
        else if (FatFS::BEG_ERROR <= err && err <= FatFS::END_ERROR)
            pwOut << "No print string yet for FatFS's error #" << err - FatFS::BEG_ERROR << " (raw = " << err << ")\n";
        else
            pwOut << "Unknown error: " << err << '\n';
    }
}

void clear_buffer (const BlockStorage *driver, BlockStorage::Buffer *buffer) {
    driver->flush(buffer);
    for (unsigned int i = 0; i < driver->get_sector_size(); ++i)
        buffer->buf[i] = 0;
    buffer->meta = NULL;
}

void clear_buffer (File *file) {
    clear_buffer(file->m_driver, file->m_buf);
}

SETUP {
    PropWare::ErrorCode err;
    dataBuffer = new uint8_t[g_driver.get_sector_size()];
    buffer.buf = dataBuffer;
    buffer.meta = &bufferMeta;
    testable = new FatFileWriter(g_fs, NEW_FILE_NAME, buffer);
    err      = testable->open();
    if (err) {
        MESSAGE("Setup failed!");
        error_checker(err);
    }
}

TEARDOWN {
    if (NULL != testable) {
        testable->close();
        clear_buffer(testable);
        delete testable;
        testable = NULL;
    }
    if (NULL != dataBuffer) {
        delete dataBuffer;
        dataBuffer = NULL;
    }
    g_fs.flush_fat();
}

TEST(ConstructorDestructor) {
    // Ensure the requested filename was not all upper case (that wouldn't be a very good test if it were)
    ASSERT_NEQ_MSG(0, strcmp(EXISTING_FILE, EXISTING_FILE_UPPER));

    testable = new FatFileWriter(g_fs, EXISTING_FILE);

    ASSERT_EQ_MSG(0, strcmp(EXISTING_FILE_UPPER, testable->get_name()));
    ASSERT_EQ_MSG((unsigned int) &pwOut, (unsigned int) testable->m_logger);
    ASSERT_EQ_MSG((unsigned int) g_fs.get_driver(), (unsigned int) testable->m_driver);
    ASSERT_EQ_MSG((unsigned int) &PropWare::SHARED_BUFFER, (unsigned int) testable->m_buf);
    ASSERT_EQ_MSG((unsigned int) testable->m_fs, (unsigned int) &g_fs);
    ASSERT_EQ_MSG(-1, testable->get_length());
    ASSERT_EQ_MSG(false, testable->m_fileMetadataModified);

    tearDown();
}

TEST(Exists_doesNotExist) {
    testable = new FatFileWriter(g_fs, NEW_FILE_NAME);
    ASSERT_FALSE(testable->exists());
    tearDown();
}

TEST(Exists_doesExist) {
    testable                   = new FatFileWriter(g_fs, EXISTING_FILE);

    PropWare::ErrorCode err;
    const bool          exists = testable->exists(err);
    error_checker(err);
    ASSERT_TRUE(exists)
    tearDown();
}

TEST(OpenClose_ExistingFile) {
    PropWare::ErrorCode err;

    testable = new FatFileWriter(g_fs, EXISTING_FILE);

    err = testable->open();
    error_checker(err);
    ASSERT_EQ_MSG(0, err);

    ASSERT_NEQ_MSG(0, testable->get_length());

    err = testable->close();
    error_checker(err);
    ASSERT_EQ_MSG(0, err);

    tearDown();
}

TEST(OpenCloseDelete_NonExistingFile) {
    PropWare::ErrorCode err;

    testable = new FatFileWriter(g_fs, NEW_FILE_NAME);

    ASSERT_FALSE(testable->exists());

    err = testable->open();
    error_checker(err);
    ASSERT_EQ_MSG(0, err);

    ASSERT_EQ_MSG(0, testable->get_length());

    err = testable->close();
    error_checker(err);
    ASSERT_EQ_MSG(0, err); // testable->close()

    clear_buffer(testable);
    ASSERT_TRUE(testable->exists());

    err = testable->remove();
    error_checker(err);
    ASSERT_EQ_MSG(0, err); // testable->remove()
    err = testable->flush();
    error_checker(err);
    ASSERT_EQ_MSG(0, err); // testable->flush()

    clear_buffer(testable);
    ASSERT_FALSE(testable->exists());

    tearDown();
}

TEST(SafePutChar_FileNotOpened) {
    // TODO
    tearDown();
}

TEST(SafePutChar_singleChar) {
    const char sampleChar = 'a';

    PropWare::ErrorCode err;
    setUp();

    ASSERT_EQ_MSG(0, testable->get_length());
    err = testable->safe_put_char(sampleChar);
    error_checker(err);
    ASSERT_EQ_MSG(0, err); // testable->safe_put_char('a')

    ASSERT_EQ_MSG(1, testable->get_length()); // Initial write
    err = testable->close();
    error_checker(err);
    ASSERT_EQ_MSG(0, err); // testable->close()

    {
        const BlockStorage   *driver = testable->m_driver;
        BlockStorage::Buffer *buffer = testable->m_buf;
        delete testable;
        g_fs.flush_fat();

        clear_buffer(driver, buffer);
    }

    FatFileReader reader(g_fs, NEW_FILE_NAME, buffer);
    ASSERT_EQ_MSG(0, reader.open());
    ASSERT_EQ_MSG(1, reader.get_length()); // Reader opens file after write
    ASSERT_EQ_MSG(sampleChar, reader.get_char());
    reader.close();

    testable = new FatFileWriter(g_fs, NEW_FILE_NAME, buffer);
    err      = testable->remove();
    error_checker(err);
    ASSERT_EQ_MSG(0, err); // testable->remove()
    err = testable->flush();
    error_checker(err);
    ASSERT_EQ_MSG(0, err); // testable->flush()

    clear_buffer(testable);
    ASSERT_FALSE(testable->exists());

    tearDown();
}

TEST(SafePutChar_MultiLine) {
    PropWare::ErrorCode err;
    setUp();

    const char testString[] = "Sample text line\n";
    ASSERT_EQ_MSG(0, testable->get_length());
    for (unsigned int i = 0; i < sizeof(testString); ++i) {
        err = testable->safe_put_char(testString[i]);
        error_checker(err);
        ASSERT_EQ_MSG(0, err);
    }

    ASSERT_EQ_MSG(sizeof(testString), testable->get_length());
    err = testable->close();
    error_checker(err);
    ASSERT_EQ_MSG(0, err);

    {
        const BlockStorage   *driver = testable->m_driver;
        BlockStorage::Buffer *buffer = testable->m_buf;
        delete testable;
        g_fs.flush_fat();

        clear_buffer(driver, buffer);
    }

    FatFileReader reader(g_fs, NEW_FILE_NAME, buffer);
    ASSERT_EQ_MSG(0, reader.open());
    ASSERT_EQ_MSG(sizeof(testString), reader.get_length());
    for (unsigned int i = 0; i < sizeof(testString); ++i)
        ASSERT_EQ_MSG(testString[i], reader.get_char());
    reader.close();

    testable = new FatFileWriter(g_fs, NEW_FILE_NAME, buffer);
    err      = testable->remove();
    error_checker(err);
    ASSERT_EQ_MSG(0, err); // testable->remove()
    err = testable->flush();
    error_checker(err);
    ASSERT_EQ_MSG(0, err); // testable->flush()

    clear_buffer(testable);
    ASSERT_FALSE(testable->exists());

    tearDown();
}

TEST(CopyFile) {
    PropWare::ErrorCode err;
    setUp();

    uint8_t                rawBuffer[g_driver.get_sector_size()];
    BlockStorage::MetaData bufferMeta;
    BlockStorage::Buffer   readBuffer = {rawBuffer, &bufferMeta};
    FatFileReader          reader(g_fs, EXISTING_FILE, readBuffer);
    ASSERT_EQ_MSG(0, reader.open());

    MESSAGE("Files opened...");
    while (!reader.eof()) {
        err = testable->safe_put_char(reader.get_char());
        error_checker(err);
        ASSERT_EQ_MSG(0, err);
    }
    MESSAGE("File copied...");

    err = testable->close();
    error_checker(err);
    ASSERT_EQ_MSG(0, err);

    MESSAGE("Writer closed...")

    {
        const BlockStorage   *driver = testable->m_driver;
        BlockStorage::Buffer *buffer = testable->m_buf;
        delete testable;
        ASSERT_EQ_MSG(0, g_fs.flush_fat());

        clear_buffer(driver, buffer);
    }

    MESSAGE("Writer deleted...")

    // Reset reader
    ASSERT_EQ_MSG(0, reader.seek(0, File::SeekDir::BEG));

    FatFileReader fileWriterChecker(g_fs, NEW_FILE_NAME, buffer);
    ASSERT_EQ_MSG(0, fileWriterChecker.open());
    ASSERT_EQ_MSG(reader.get_length(), fileWriterChecker.get_length());

    MESSAGE("Readers opened...")

    while (!fileWriterChecker.eof()) {
        char c;
        ASSERT_EQ_MSG(0, fileWriterChecker.safe_get_char(c));

        char expectedChar = reader.get_char();
        if (expectedChar != c) {
            FAIL("Failure on char %d", reader.tell() - 1);
        }
    }

    MESSAGE("File content confirmed! Cleaning up...")

    fileWriterChecker.close();

    testable = new FatFileWriter(g_fs, NEW_FILE_NAME, buffer);
    err      = testable->remove();
    error_checker(err);
    ASSERT_EQ_MSG(0, err); // testable->remove()
    err = testable->flush();
    error_checker(err);
    ASSERT_EQ_MSG(0, err); // testable->flush()

    clear_buffer(testable);
    ASSERT_FALSE(testable->exists());

    tearDown();
}

int main () {
    PropWare::ErrorCode err;

    START(FatFileReaderTest);

    uint8_t *tempBuffer = new uint8_t[g_driver.get_sector_size()];
    if ((err = g_fs.mount(tempBuffer, 1))) {
        error_checker(err);
        failures = (uint8_t) -1;
        COMPLETE();
    }
    delete tempBuffer;

    RUN_TEST(ConstructorDestructor);
    RUN_TEST(Exists_doesNotExist);
    RUN_TEST(Exists_doesExist);
    RUN_TEST(OpenClose_ExistingFile);
    RUN_TEST(OpenCloseDelete_NonExistingFile);
    RUN_TEST(SafePutChar_singleChar);
    RUN_TEST(SafePutChar_MultiLine);
    RUN_TEST(CopyFile);

    COMPLETE();
}
