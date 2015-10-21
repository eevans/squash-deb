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

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <errno.h>

#include <squash/squash.h>

#include "zstd/lib/zstd.h"
#include "zstd/lib/zstd_static.h"

SQUASH_PLUGIN_EXPORT
SquashStatus squash_plugin_init_codec (SquashCodec* codec, SquashCodecImpl* impl);

static size_t
squash_zstd_get_max_compressed_size (SquashCodec* codec, size_t uncompressed_length) {
  return ZSTD_compressBound (uncompressed_length);
}

static SquashStatus
squash_zstd_status_from_zstd_error (size_t res) {
  if (!ZSTD_isError (res))
    return SQUASH_OK;

  switch ((ZSTD_errorCodes) (-(int)(res))) {
    case ZSTD_OK_NoError:
      return SQUASH_OK;
    case ZSTD_ERROR_GENERIC:
      return squash_error (SQUASH_FAILED);
    case ZSTD_ERROR_MagicNumber:
      return squash_error (SQUASH_INVALID_BUFFER);
    case ZSTD_ERROR_SrcSize:
      return squash_error (SQUASH_BUFFER_EMPTY);
    case ZSTD_ERROR_maxDstSize_tooSmall:
      return squash_error (SQUASH_BUFFER_FULL);
    case ZSTD_ERROR_corruption:
      return squash_error (SQUASH_INVALID_BUFFER);
    case ZSTD_ERROR_maxCode:
    default:
      return squash_error (SQUASH_FAILED);
  }

  squash_assert_unreachable ();
}

static SquashStatus
squash_zstd_decompress_buffer (SquashCodec* codec,
                               size_t* decompressed_length,
                               uint8_t decompressed[SQUASH_ARRAY_PARAM(*decompressed_length)],
                               size_t compressed_length,
                               const uint8_t compressed[SQUASH_ARRAY_PARAM(compressed_length)],
                               SquashOptions* options) {
  *decompressed_length = ZSTD_decompress (decompressed, *decompressed_length, compressed, compressed_length);

  return squash_zstd_status_from_zstd_error (*decompressed_length);
}

static SquashStatus
squash_zstd_compress_buffer (SquashCodec* codec,
                             size_t* compressed_length,
                             uint8_t compressed[SQUASH_ARRAY_PARAM(*compressed_length)],
                             size_t uncompressed_length,
                             const uint8_t uncompressed[SQUASH_ARRAY_PARAM(uncompressed_length)],
                             SquashOptions* options) {
  *compressed_length = ZSTD_compress (compressed, *compressed_length, uncompressed, uncompressed_length);

  return squash_zstd_status_from_zstd_error (*compressed_length);
}

SquashStatus
squash_plugin_init_codec (SquashCodec* codec, SquashCodecImpl* impl) {
  const char* name = squash_codec_get_name (codec);

  if (strcmp ("zstd", name) == 0) {
    impl->get_max_compressed_size = squash_zstd_get_max_compressed_size;
    impl->decompress_buffer = squash_zstd_decompress_buffer;
    impl->compress_buffer_unsafe = squash_zstd_compress_buffer;
  } else {
    return squash_error (SQUASH_UNABLE_TO_LOAD);
  }

  return SQUASH_OK;
}
