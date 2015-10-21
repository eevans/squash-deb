/* Copyright (c) 2015 The Squash Authors
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Authors:
 *   Evan Nemerson <evan@nemerson.com>
 */

#define _FILE_OFFSET_BITS 64
#define _POSIX_C_SOURCE 200112L

#include "internal.h"

#ifndef SQUASH_FILE_BUF_SIZE
#  define SQUASH_FILE_BUF_SIZE ((size_t) (1024 * 1024))
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>

/* #define SQUASH_MMAP_IO */

/**
 * @cond INTERNAL
 */

typedef struct SquashMappedFile_s {
  uint8_t* data;
  size_t length;
  size_t map_length;
  size_t window_offset;
  FILE* fp;
  bool writable;
} SquashMappedFile;

static const SquashMappedFile squash_mapped_file_empty = { MAP_FAILED, 0 };

struct _SquashFile {
  FILE* fp;
  bool eof;
  SquashStream* stream;
  SquashStatus last_status;
  SquashCodec* codec;
  SquashOptions* options;
  uint8_t buf[SQUASH_FILE_BUF_SIZE];
#if defined(SQUASH_MMAP_IO)
  SquashMappedFile map;
#endif
};

#if defined(__GNUC__)
__attribute__ ((__const__))
#endif
static size_t
squash_npot (size_t v) {
  v--;
  v |= v >> 1;
  v |= v >> 2;
  v |= v >> 4;
  v |= v >> 8;
  v |= v >> 16;
#if SIZE_MAX > UINT32_MAX
  v |= v >> 32;
#endif
  v++;
  return v;
}

static bool
squash_mapped_file_init_full (SquashMappedFile* mapped, FILE* fp, size_t length, bool length_is_suggestion, bool writable) {
  assert (mapped != NULL);
  assert (fp != NULL);

  if (mapped->data != MAP_FAILED)
    munmap (mapped->data - mapped->window_offset, mapped->length + mapped->window_offset);

  int fd = fileno (fp);
  if (fd == -1)
    return false;

  int ires;
  struct stat fp_stat;

  ires = fstat (fd, &fp_stat);
  if (ires == -1 || !S_ISREG(fp_stat.st_mode) || (!writable && fp_stat.st_size == 0))
    return false;

  off_t offset = ftello (fp);
  if (offset < 0)
    return false;

  if (writable) {
    ires = ftruncate (fd, offset + (off_t) length);
    if (ires == -1)
      return false;
  } else {
    const size_t remaining = fp_stat.st_size - (size_t) offset;
    if (remaining > 0) {
      if (length == 0 || (length > remaining && length_is_suggestion)) {
        length = remaining;
      } else if (length > remaining) {
        return false;
      }
    } else {
      return false;
    }
  }
  mapped->length = length;

  const size_t page_size = (size_t) sysconf (_SC_PAGE_SIZE);
  mapped->window_offset = (size_t) offset % page_size;
  mapped->map_length = length + mapped->window_offset;

  mapped->data = mmap (NULL, mapped->map_length, writable ? PROT_READ | PROT_WRITE : PROT_READ, MAP_SHARED, fd, offset - mapped->window_offset);
  if (mapped->data == MAP_FAILED)
    return false;

  mapped->data += mapped->window_offset;
  mapped->fp = fp;
  mapped->writable = writable;

  return true;
}

static bool
squash_mapped_file_init (SquashMappedFile* mapped, FILE* fp, size_t length, bool writable) {
  return squash_mapped_file_init_full (mapped, fp, length, false, writable);
}

static void
squash_mapped_file_destroy (SquashMappedFile* mapped, bool success) {
  if (mapped->data != MAP_FAILED) {
    munmap (mapped->data - mapped->window_offset, mapped->length + mapped->window_offset);
    mapped->data = MAP_FAILED;

    if (success) {
      fseeko (mapped->fp, mapped->length, SEEK_CUR);
      if (mapped->writable) {
        ftruncate (fileno (mapped->fp), ftello (mapped->fp));
      }
    }
  }
}

/**
 * @endcond
 */

/**
 * @defgroup SquashFile SquashFile
 * @brief stdio-like API and utilities
 *
 * These functions provide an API which should be familiar for those
 * used to dealing with the standard I/O functions.
 *
 * Additionally, a splice function family is provided which is
 * inspired by the Linux-specific splice(2) function.
 *
 * @{
 */

/**
 * @brief Open a file
 *
 * The @a mode parameter will be passed through to @a fopen, so the
 * value must valid.  Note that Squash may attempt to use @a mmap
 * regardless of whether the *m* flag is passed.
 *
 * The file is always assumed to be compressed—calling @ref
 * squash_file_write will always compress, and calling @ref
 * squash_file_read will always decompress.  Note, however, that you
 * cannot mix reading and writing to the same file as you can with a
 * standard *FILE*.
 *
 * @note Error handling for this function is somewhat limited, and it
 * may be difficult to determine the exact nature of problems such as
 * an invalid codec, where errno is not set.  If this is unacceptable
 * you should call @ref squash_get_codec and @ref squash_options_parse
 * yourself and pass the results to @ref
 * squash_file_open_codec_with_options (which will only fail due to
 * the underlying *fopen* failing).
 *
 * @param filename name of the file to open
 * @param mode file mode
 * @param codec codec to use
 * @param ... options
 * @return The opened file, or *NULL* on error
 * @see squash_file_open_codec
 * @see squash_file_open_with_options
 * @see squash_file_open_codec_with_options
 */
SquashFile*
squash_file_open (const char* codec, const char* filename, const char* mode, ...) {
  va_list ap;
  SquashOptions* options;
  SquashCodec* codec_i;

  assert (filename != NULL);
  assert (mode != NULL);
  assert (codec != NULL);

  codec_i = squash_get_codec (codec);
  if (codec_i == NULL)
    return NULL;

  va_start (ap, mode);
  options = squash_options_newv (codec_i, ap);
  va_end (ap);

  return squash_file_open_codec_with_options (codec_i, filename, mode, options);
}

/**
 * @brief Open a file using a codec instance
 *
 * @param filename name of the file to open
 * @param mode file mode
 * @param codec codec to use
 * @param ... options
 * @return The opened file, or *NULL* on error
 * @see squash_file_open
 */
SquashFile*
squash_file_open_codec (SquashCodec* codec, const char* filename, const char* mode, ...) {
  va_list ap;
  SquashOptions* options;

  assert (filename != NULL);
  assert (mode != NULL);
  assert (codec != NULL);

  va_start (ap, mode);
  options = squash_options_newv (codec, ap);
  va_end (ap);

  return squash_file_open_codec_with_options (codec, filename, mode, options);
}

/**
 * @brief Open a file with the specified options
 *
 * @param filename name of the file to open
 * @param mode file mode
 * @param codec codec to use
 * @param options options
 * @return The opened file, or *NULL* on error
 * @see squash_file_open
 */
SquashFile*
squash_file_open_with_options (const char* codec, const char* filename, const char* mode, SquashOptions* options) {
  assert (filename != NULL);
  assert (mode != NULL);
  assert (codec != NULL);

  SquashCodec* codec_i = squash_get_codec (codec);
  if (codec_i == NULL)
    return NULL;

  return squash_file_open_codec_with_options (codec_i, filename, mode, options);
}

/**
 * @brief Open a file using a codec instance with the specified options
 *
 * @param filename name of the file to open
 * @param mode file mode
 * @param codec codec to use
 * @param options options
 * @return The opened file, or *NULL* on error
 * @see squash_file_open
 */
SquashFile*
squash_file_open_codec_with_options (SquashCodec* codec, const char* filename, const char* mode, SquashOptions* options) {
  assert (filename != NULL);
  assert (mode != NULL);
  assert (codec != NULL);

  FILE* fp = fopen (filename, mode);
  if (fp == NULL)
    return NULL;

  return squash_file_steal_codec_with_options (codec, fp, options);
}


/**
 * @brief Open an existing stdio file
 *
 * @param fp the stdio file to use
 * @param codec codec to use
 * @param ... options
 * @return The opened file, or *NULL* on error
 * @see squash_file_steal_codec
 * @see squash_file_steal_with_options
 * @see squash_file_steal_codec_with_options
 */
SquashFile*
squash_file_steal (const char* codec, FILE* fp, ...) {
  va_list ap;
  SquashOptions* options;

  assert (fp != NULL);
  assert (codec != NULL);

  SquashCodec* codec_i = squash_get_codec (codec);
  if (codec_i == NULL)
    return NULL;
  va_start (ap, fp);
  options = squash_options_newv (codec_i, ap);
  va_end (ap);

  return squash_file_steal_codec_with_options (codec_i, fp, options);
}

/**
 * @brief Open an existing stdio file using a codec instance
 *
 * @param fp the stdio file to use
 * @param codec codec to use
 * @param ... options
 * @return The opened file, or *NULL* on error
 * @see squash_file_steal_codec
 * @see squash_file_steal_with_options
 * @see squash_file_steal_codec_with_options
 */
SquashFile*
squash_file_steal_codec (SquashCodec* codec, FILE* fp, ...) {
  va_list ap;
  SquashOptions* options;

  assert (fp != NULL);
  assert (codec != NULL);

  va_start (ap, fp);
  options = squash_options_newv (codec, ap);
  va_end (ap);

  return squash_file_steal_codec_with_options (codec, fp, options);
}

/**
 * @brief Open an existing stdio file with the specified options
 *
 * @param fp the stdio file to use
 * @param codec codec to use
 * @param options options
 * @return The opened file, or *NULL* on error
 * @see squash_file_steal_codec
 * @see squash_file_steal_with_options
 * @see squash_file_steal_codec_with_options
 */
SquashFile*
squash_file_steal_with_options (const char* codec, FILE* fp, SquashOptions* options) {
  assert (fp != NULL);
  assert (codec != NULL);

  SquashCodec* codec_i = squash_get_codec (codec);
  if (codec_i == NULL)
    return NULL;

  return squash_file_steal_codec_with_options (codec_i, fp, options);
}

/**
 * @brief Open an existing stdio file using a codec instance with the specified options
 *
 * @param fp the stdio file to use
 * @param codec codec to use
 * @param options options
 * @return The opened file, or *NULL* on error
 * @see squash_file_steal_codec
 * @see squash_file_steal_with_options
 * @see squash_file_steal_codec_with_options
 */
SquashFile*
squash_file_steal_codec_with_options (SquashCodec* codec, FILE* fp, SquashOptions* options) {
  assert (fp != NULL);
  assert (codec != NULL);

  SquashFile* file = malloc (sizeof (SquashFile));
  if (file == NULL)
    return NULL;

  file->fp = fp;
  file->eof = false;
  file->stream = NULL;
  file->last_status = SQUASH_OK;
  file->codec = codec;
  file->options = (options != NULL) ? squash_object_ref (options) : NULL;
#if defined(SQUASH_MMAP_IO)
  file->map = squash_mapped_file_empty;
#endif

  return file;
}

/**
 * @brief Read from a compressed file
 *
 * Attempt to read @a decompressed_length bytes of decompressed data
 * into the @a decompressed buffer.  The number of bytes of compressed
 * data read from the input file may be **significantly** more, or less,
 * than @a decompressed_length.
 *
 * The number of decompressed bytes successfully read from the file
 * will be stored in @a decompressed_read after this function is
 * execute.  This value will never be greater than @a
 * decompressed_length, but it may be less if there was an error or
 * the end of the input file was reached.
 *
 * @param file the file to read from
 * @param decompressed_length number of bytes to attempt to write to @a decompressed
 * @param decompressed buffer to write the decompressed data to
 * @return the result of the operation
 * @retval SQUASH_OK successfully read some data
 * @retval SQUASH_END_OF_STREAM the end of the file was reached
 */
SquashStatus
squash_file_read (SquashFile* file,
                  size_t* decompressed_length,
                  uint8_t decompressed[SQUASH_ARRAY_PARAM(*decompressed_length)]) {
  SquashStatus res = SQUASH_FAILED;

  assert (file != NULL);
  assert (decompressed_length != NULL);
  assert (decompressed != NULL);

  if (file->stream == NULL) {
    file->stream = squash_codec_create_stream_with_options (file->codec, SQUASH_STREAM_DECOMPRESS, file->options);
    if (file->stream == NULL)
      return squash_error (SQUASH_FAILED);
  }
  SquashStream* stream = file->stream;

  assert (stream->next_out == NULL);
  assert (stream->avail_out == 0);

  if (stream->state == SQUASH_STREAM_STATE_FINISHED) {
    *decompressed_length = 0;
    return SQUASH_END_OF_STREAM;
  }

  file->stream->next_out = decompressed;
  file->stream->avail_out = *decompressed_length;

  while (stream->avail_out != 0) {
    if (file->last_status < 0) {
      res = file->last_status;
      break;
    }

    if (stream->state == SQUASH_STREAM_STATE_FINISHED)
      break;

    if (file->last_status == SQUASH_PROCESSING) {
      if (stream->state < SQUASH_STREAM_STATE_FINISHING) {
        res = file->last_status = squash_stream_process (stream);
      } else {
        res = file->last_status = squash_stream_finish (stream);
      }

      continue;
    }

    assert (file->last_status == SQUASH_OK);

#if defined(SQUASH_MMAP_IO)
    if (file->map.data != MAP_FAILED)
      squash_mapped_file_destroy (&(file->map), true);

    if (squash_mapped_file_init_full(&(file->map), file->fp, SQUASH_FILE_BUF_SIZE, true, false)) {
      stream->next_in = file->map.data;
      stream->avail_in = file->map.length;
    } else
#endif
    {
      stream->next_in = file->buf;
      stream->avail_in = fread (file->buf, 1, SQUASH_FILE_BUF_SIZE, file->fp);
    }

    if (stream->avail_in == 0) {
      if (feof (file->fp)) {
        res = file->last_status = squash_stream_finish (stream);
      } else {
        res = SQUASH_IO;
        break;
      }
    } else {
      res = file->last_status = squash_stream_process (stream);
    }
  }

  *decompressed_length = (stream->next_out - decompressed);
  stream->next_out = 0;
  stream->avail_out = 0;

  return res;
}

static SquashStatus
squash_file_write_internal (SquashFile* file,
                            size_t uncompressed_length,
                            const uint8_t uncompressed[SQUASH_ARRAY_PARAM(uncompressed_length)],
                            SquashOperation operation) {
  SquashStatus res;

  assert (file != NULL);

  if (file->stream == NULL) {
    file->stream = squash_codec_create_stream_with_options (file->codec, SQUASH_STREAM_COMPRESS, file->options);
    if (file->stream == NULL)
      return squash_error (SQUASH_FAILED);
  }

  assert (file->stream->next_in == NULL);
  assert (file->stream->avail_in == 0);
  assert (file->stream->next_out == NULL);
  assert (file->stream->avail_out == 0);

  file->stream->next_in = uncompressed;
  file->stream->avail_in = uncompressed_length;

  file->stream->next_in = uncompressed;
  file->stream->avail_in = uncompressed_length;

  do {
    file->stream->next_out = file->buf;
    file->stream->avail_out = SQUASH_FILE_BUF_SIZE;

    switch (operation) {
      case SQUASH_OPERATION_PROCESS:
        res = squash_stream_process (file->stream);
        break;
      case SQUASH_OPERATION_FLUSH:
        res = squash_stream_flush (file->stream);
        break;
      case SQUASH_OPERATION_FINISH:
        res = squash_stream_finish (file->stream);
        break;
      case SQUASH_OPERATION_TERMINATE:
        squash_assert_unreachable ();
        break;
    }

    if (res > 0 && file->stream->avail_out != SQUASH_FILE_BUF_SIZE) {
      size_t bytes_written = fwrite (file->buf, 1, SQUASH_FILE_BUF_SIZE - file->stream->avail_out, file->fp);
      if (bytes_written != SQUASH_FILE_BUF_SIZE - file->stream->avail_out)
        return SQUASH_IO;
    }
  } while (res == SQUASH_PROCESSING);

  file->stream->next_in = NULL;
  file->stream->avail_in = 0;
  file->stream->next_out = NULL;
  file->stream->avail_out = 0;

  return res;
}

/**
 * @brief Write data to a compressed file
 *
 * Attempt to write the compressed equivalent of @a uncompressed to a
 * file.  The number of bytes of compressed data written to the output
 * file may be **significantly** more, or less, than the @a
 * uncompressed_length.
 *
 * @note It is likely the compressed data will actually be buffered,
 * not immediately written to the file.  For codecs which support
 * flushing you can use @ref squash_file_flush to flush the data,
 * otherwise the data may not be written until @ref squash_file_close
 * or @ref squash_file_free is called.
 *
 * @param file file to write to
 * @param uncompressed_length number of bytes of uncompressed data in
 *   @a uncompressed to attempt to write
 * @param uncompressed data to write
 * @return result of the operation
 */
SquashStatus
squash_file_write (SquashFile* file,
                   size_t uncompressed_length,
                   const uint8_t uncompressed[SQUASH_ARRAY_PARAM(uncompressed_length)]) {
  return squash_file_write_internal (file, uncompressed_length, uncompressed, SQUASH_OPERATION_PROCESS);
}

/**
 * @brief immediately write any buffered data to a file
 *
 * @note This function only works for codecs which support flushing
 * (see the @ref SQUASH_CODEC_INFO_CAN_FLUSH flag in the return value
 * of @ref squash_codec_get_info).
 *
 * @param file file to flush
 * @returns *TRUE* if flushing succeeeded, *FALSE* if flushing is not
 *   supported or there was another error.
 */
SquashStatus
squash_file_flush (SquashFile* file) {
  SquashStatus res = squash_file_write_internal (file, 0, NULL, SQUASH_OPERATION_FLUSH);
  fflush (file->fp);
  return res;
}

/**
 * @brief Determine whether the file has reached the end of file
 *
 * @param file file to check
 * @returns *TRUE* if EOF was reached, *FALSE* otherwise
 */
bool
squash_file_eof (SquashFile* file) {
  return (file->stream->state == SQUASH_STREAM_STATE_FINISHED) && (feof (file->fp));
}

/**
 * @brief compress or decompress the contents of one file to another
 *
 * This function will attempt to compress or decompress the contents
 * of one file to another.  It will attempt to use memory-mapped files
 * in order to reduce memory usage and increase performance, and so
 * should be preferred over writing similar code manually.
 *
 * @param fp_in the input *FILE* pointer
 * @param fp_out the output *FILE* pointer
 * @param length number of bytes (uncompressed) to transfer from @a
 *   fp_in to @a fp_out, or 0 to transfer the entire file
 * @param stream_type whether to compress or decompress the data
 * @param codec the name of the codec to use
 * @param ... list of options (with a *NULL* sentinel)
 * @returns @ref SQUASH_OK on success, or a negative error code on
 *   failure
 */
SquashStatus
squash_splice (const char* codec, SquashStreamType stream_type, FILE* fp_out, FILE* fp_in, size_t length, ...) {
  assert (fp_in != NULL);
  assert (fp_out != NULL);
  assert (stream_type == SQUASH_STREAM_COMPRESS || stream_type == SQUASH_STREAM_DECOMPRESS);

  SquashCodec* codec_i = squash_get_codec (codec);
  if (codec_i == NULL)
    return squash_error (SQUASH_BAD_PARAM);

  SquashOptions* options = NULL;
  va_list ap;
  va_start (ap, length);
  options = squash_options_newv (codec_i, ap);
  va_end (ap);

  return squash_splice_codec_with_options (codec_i, stream_type, fp_out, fp_in, length, options);
}

/**
 * @brief compress or decompress the contents of one file to another
 *
 * This function will attempt to compress or decompress the contents
 * of one file to another.  It will attempt to use memory-mapped files
 * in order to reduce memory usage and increase performance, and so
 * should be preferred over writing similar code manually.
 *
 * @param fp_in the input *FILE* pointer
 * @param fp_out the output *FILE* pointer
 * @param length number of bytes (uncompressed) to transfer from @a
 *   fp_in to @a fp_out
 * @param stream_type whether to compress or decompress the data
 * @param codec codec to use
 * @param ... list of options (with a *NULL* sentinel)
 * @returns @ref SQUASH_OK on success, or a negative error code on
 *   failure
 */
SquashStatus
squash_splice_codec (SquashCodec* codec, SquashStreamType stream_type, FILE* fp_out, FILE* fp_in, size_t length, ...) {
  assert (fp_in != NULL);
  assert (fp_out != NULL);
  assert (stream_type == SQUASH_STREAM_COMPRESS || stream_type == SQUASH_STREAM_DECOMPRESS);
  assert (codec != NULL);

  SquashOptions* options = NULL;
  va_list ap;
  va_start (ap, length);
  options = squash_options_newv (codec, ap);
  va_end (ap);

  return squash_splice_codec_with_options (codec, stream_type, fp_out, fp_in, length, options);
}

/**
 * @brief compress or decompress the contents of one file to another
 *
 * This function will attempt to compress or decompress the contents
 * of one file to another.  It will attempt to use memory-mapped files
 * in order to reduce memory usage and increase performance, and so
 * should be preferred over writing similar code manually.
 *
 * @param fp_in the input *FILE* pointer
 * @param fp_out the output *FILE* pointer
 * @param length number of bytes (uncompressed) to transfer from @a
 *   fp_in to @a fp_out
 * @param stream_type whether to compress or decompress the data
 * @param codec name of the codec to use
 * @param options options to pass to the codec
 * @returns @ref SQUASH_OK on success, or a negative error code on
 *   failure
 */
SquashStatus
squash_splice_with_options (const char* codec, SquashStreamType stream_type, FILE* fp_out, FILE* fp_in, size_t length, SquashOptions* options) {
  assert (fp_in != NULL);
  assert (fp_out != NULL);
  assert (stream_type == SQUASH_STREAM_COMPRESS || stream_type == SQUASH_STREAM_DECOMPRESS);

  SquashCodec* codec_i = squash_get_codec (codec);
  if (codec_i == NULL)
    return squash_error (SQUASH_BAD_PARAM);

  return squash_splice_codec_with_options (codec_i, stream_type, fp_out, fp_in, length, options);
}

static SquashStatus
squash_splice_map (FILE* fp_in, FILE* fp_out, size_t length, SquashStreamType stream_type, SquashCodec* codec, SquashOptions* options) {
  SquashStatus res = SQUASH_FAILED;
  SquashMappedFile mapped_in = squash_mapped_file_empty;
  SquashMappedFile mapped_out = squash_mapped_file_empty;

  if (stream_type == SQUASH_STREAM_COMPRESS) {
    if (!squash_mapped_file_init (&mapped_in, fp_in, length, false))
      goto cleanup;

    const size_t max_output_length = squash_codec_get_max_compressed_size(codec, mapped_in.length);
    if (!squash_mapped_file_init (&mapped_out, fp_out, max_output_length, true))
      goto cleanup;

    res = squash_codec_compress_with_options (codec, &mapped_out.length, mapped_out.data, mapped_in.length, mapped_in.data, options);
    if (res != SQUASH_OK)
      goto cleanup;

    squash_mapped_file_destroy (&mapped_in, true);
    squash_mapped_file_destroy (&mapped_out, true);
  } else {
    if (!squash_mapped_file_init (&mapped_in, fp_in, 0, false))
      goto cleanup;

    const SquashCodecInfo codec_info = squash_codec_get_info (codec);
    const bool knows_uncompressed = ((codec_info & SQUASH_CODEC_INFO_KNOWS_UNCOMPRESSED_SIZE) == SQUASH_CODEC_INFO_KNOWS_UNCOMPRESSED_SIZE);

    size_t max_output_length = knows_uncompressed ?
      squash_codec_get_uncompressed_size(codec, mapped_in.length, mapped_in.data) :
      squash_npot (mapped_in.length) << 3;

    do {
      if (!squash_mapped_file_init (&mapped_out, fp_out, max_output_length, true))
        goto cleanup;

      res = squash_codec_decompress_with_options (codec, &mapped_out.length, mapped_out.data, mapped_in.length, mapped_in.data, options);
      if (res == SQUASH_OK) {
        squash_mapped_file_destroy (&mapped_in, true);
        squash_mapped_file_destroy (&mapped_out, true);
      } else {
        max_output_length <<= 1;
      }
    } while (!knows_uncompressed && res == SQUASH_BUFFER_FULL);
  }

 cleanup:

  squash_mapped_file_destroy (&mapped_in, false);
  squash_mapped_file_destroy (&mapped_out, false);

  return res;
}

static SquashStatus
squash_splice_stream (FILE* fp_in,
                      FILE* fp_out,
                      size_t length,
                      SquashStreamType stream_type,
                      SquashCodec* codec,
                      SquashOptions* options) {
  SquashStatus res = SQUASH_FAILED;
  SquashFile* file = NULL;
  size_t remaining = length;
  uint8_t* data = NULL;
  size_t data_length = 0;
#if defined(SQUASH_MMAP_IO)
  bool first_block = true;
  SquashMappedFile map = squash_mapped_file_empty;

  if (stream_type == SQUASH_STREAM_COMPRESS) {
    file = squash_file_steal_codec_with_options (fp_out, codec, options);
    assert (file != NULL);

    while (length == 0 || remaining != 0) {
      const size_t req_size = (length == 0 || remaining > SQUASH_FILE_BUF_SIZE) ? SQUASH_FILE_BUF_SIZE : remaining;
      if (!squash_mapped_file_init_full(&map, fp_in, req_size, true, false)) {
        if (first_block)
          goto nomap;
        else {
          break;
        }
      } else {
        first_block = false;
      }

      res = squash_file_write (file, map.length, map.data);
      if (res != SQUASH_OK)
        goto cleanup;

      if (length != 0)
        remaining -= map.length;
      squash_mapped_file_destroy (&map, true);
    }
  } else { /* stream_type == SQUASH_STREAM_DECOMPRESS */
    file = squash_file_steal_codec_with_options (fp_in, codec, options);
    assert (file != NULL);

    while (length == 0 || remaining > 0) {
      const size_t req_size = (length == 0 || remaining > SQUASH_FILE_BUF_SIZE) ? SQUASH_FILE_BUF_SIZE : remaining;
      if (!squash_mapped_file_init_full(&map, fp_out, req_size, true, true)) {
        if (first_block)
          goto nomap;
        else {
          break;
        }
      } else {
        first_block = false;
      }

      res = squash_file_read (file, &map.length, map.data);
      if (res < 0)
        goto cleanup;

      if (res == SQUASH_END_OF_STREAM) {
        assert (map.length == 0);
        res = SQUASH_OK;
        squash_mapped_file_destroy (&map, true);
        goto cleanup;
      }

      squash_mapped_file_destroy (&map, true);
    }
  }

 nomap:
#endif /* defined(SQUASH_MMAP_IO) */

  if (res != SQUASH_OK) {
    file = squash_file_steal_codec_with_options (codec, (stream_type == SQUASH_STREAM_COMPRESS ? fp_out : fp_in), options);
    if (file == NULL) {
      res = squash_error (SQUASH_FAILED);
      goto cleanup;
    }

    data = malloc (SQUASH_FILE_BUF_SIZE);
    if (data == NULL) {
      res = squash_error (SQUASH_MEMORY);
      goto cleanup;
    }

    if (stream_type == SQUASH_STREAM_COMPRESS) {
      while (length == 0 || remaining != 0) {
        const size_t req_size = (length == 0 || remaining > SQUASH_FILE_BUF_SIZE) ? SQUASH_FILE_BUF_SIZE : remaining;

        data_length = fread (data, 1, req_size, fp_in);
        if (data_length == 0) {
          res = feof (fp_in) ? SQUASH_OK : squash_error (SQUASH_IO);
          goto cleanup;
        }

        res = squash_file_write (file, data_length, data);
        if (res != SQUASH_OK)
          goto cleanup;

        if (remaining != 0) {
          assert (data_length <= remaining);
          remaining -= data_length;
        }
      }
    } else {
      while (length == 0 || remaining != 0) {
        data_length = (length == 0 || remaining > SQUASH_FILE_BUF_SIZE) ? SQUASH_FILE_BUF_SIZE : remaining;
        res = squash_file_read (file, &data_length, data);
        if (res < 0) {
          break;
        } else if (res == SQUASH_PROCESSING) {
          res = SQUASH_OK;
        }

        if (data_length > 0) {
          size_t bytes_written = fwrite (data, 1, data_length, fp_out);
          /* fprintf (stderr, "%s:%d: bytes_written: %zu\n", __FILE__, __LINE__, data_length); */
          assert (bytes_written == data_length);
          if (bytes_written == 0) {
            res = squash_error (SQUASH_IO);
            break;
          }

          if (remaining != 0) {
            assert (data_length <= remaining);
            remaining -= data_length;
          }
        }

        if (res == SQUASH_END_OF_STREAM) {
          res = SQUASH_OK;
          break;
        }
      }
    }
  }

 cleanup:

  squash_file_free (file, NULL);
#if defined(SQUASH_MMAP_IO)
  squash_mapped_file_destroy (&map, false);
#endif
  free (data);

  return res;
}

/**
 * @brief compress or decompress the contents of one file to another
 *
 * This function will attempt to compress or decompress the contents
 * of one file to another.  It will attempt to use memory-mapped files
 * in order to reduce memory usage and increase performance, and so
 * should be preferred over writing similar code manually.
 *
 * @param fp_in the input *FILE* pointer
 * @param fp_out the output *FILE* pointer
 * @param length number of bytes (uncompressed) to transfer from @a
 *   fp_in to @a fp_out
 * @param stream_type whether to compress or decompress the data
 * @param codec codec to use
 * @param options options to pass to the codec
 * @returns @ref SQUASH_OK on success, or a negative error code on
 *   failure
 */
SquashStatus
squash_splice_codec_with_options (SquashCodec* codec,
                                  SquashStreamType stream_type,
                                  FILE* fp_out,
                                  FILE* fp_in,
                                  size_t length,
                                  SquashOptions* options) {
  SquashStatus res = SQUASH_FAILED;

  assert (fp_in != NULL);
  assert (fp_out != NULL);
  assert (stream_type == SQUASH_STREAM_COMPRESS || stream_type == SQUASH_STREAM_DECOMPRESS);
  assert (codec != NULL);

  if (codec->impl.create_stream == NULL)
    res = squash_splice_map (fp_in, fp_out, length, stream_type, codec, options);

  if (res != SQUASH_OK)
    return squash_splice_stream (fp_in, fp_out, length, stream_type, codec, options);

  return res;
}

/**
 * @brief Close a file
 *
 * If @a file is a compressor the stream will finish compressing,
 * writing any buffered data.  For codecs which do not provide a
 * native streaming interface, *all* of the actual compression will
 * take place during this call.  In other words, it may block for a
 * non-trivial period.  If this is a problem please file a bug against
 * Squash (including your use case), and we can discuss adding a
 * function call which will simply abort compression.
 *
 * In addition to freeing the @ref SquashFile instance, this function
 * will close the underlying *FILE* pointer.  If you wish to continue
 * using the *FILE* for something else, use @ref squash_file_free
 * instead.
 *
 * @param file file to close
 * @return @ref SQUASH_OK on success or a negative error code on
 *   failure
 */
SquashStatus
squash_file_close (SquashFile* file) {
  FILE* fp = NULL;
  SquashStatus res = squash_file_free (file, &fp);
  if (fp != NULL)
    fclose (fp);
  return res;
}

/**
 * @brief Free a file
 *
 * This function will free the @ref SquashFile, but unlike @ref
 * squash_file_close it will not actually close the underlying *FILE*

 * pointer.  Instead, it will return the value in the @a fp argument,
 * allowing you to further manipulate it.
 *
 * @param file file to free
 * @param[out] fp location to store the underlying *FILE* pointer
 * @return @ref SQUASH_OK on success or a negative error code on
 *   failure
 */
SquashStatus
squash_file_free (SquashFile* file, FILE** fp) {
  SquashStatus res = SQUASH_OK;

  if (file == NULL)
    return SQUASH_OK;

  if (file->stream != NULL && file->stream->stream_type == SQUASH_STREAM_COMPRESS)
    res = squash_file_write_internal (file, 0, NULL, SQUASH_OPERATION_FINISH);

  fflush (file->fp);

#if defined(SQUASH_MMAP_IO)
  squash_mapped_file_destroy (&(file->map), false);
#endif

  if (fp != NULL)
    *fp = file->fp;

  squash_object_unref (file->stream);
  squash_object_unref (file->options);
  free (file);

  return res;
}

/**
 * @}
 */
