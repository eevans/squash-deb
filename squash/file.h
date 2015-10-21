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

#ifndef SQUASH_FILE_H
#define SQUASH_FILE_H

#if !defined (SQUASH_H_INSIDE) && !defined (SQUASH_COMPILATION)
#error "Only <squash/squash.h> can be included directly."
#endif

#include <stdbool.h>
#include <stdio.h>

SQUASH_BEGIN_DECLS

SQUASH_API SquashFile*  squash_file_open                     (const char* codec,
                                                              const char* filename,
                                                              const char* mode,
                                                              ...);
SQUASH_API SquashFile*  squash_file_open_codec               (SquashCodec* codec,
                                                              const char* filename,
                                                              const char* mode,
                                                              ...);
SQUASH_API SquashFile*  squash_file_open_with_options        (const char* codec,
                                                              const char* filename,
                                                              const char* mode,
                                                              SquashOptions* options);
SQUASH_API SquashFile*  squash_file_open_codec_with_options  (SquashCodec* codec,
                                                              const char* filename,
                                                              const char* mode,
                                                              SquashOptions* options);

SQUASH_API SquashFile*  squash_file_steal                    (const char* codec,
                                                              FILE* fp,
                                                              ...);
SQUASH_API SquashFile*  squash_file_steal_codec              (SquashCodec* codec,
                                                              FILE* fp,
                                                              ...);
SQUASH_API SquashFile*  squash_file_steal_with_options       (const char* codec,
                                                              FILE* fp,
                                                              SquashOptions* options);
SQUASH_API SquashFile*  squash_file_steal_codec_with_options (SquashCodec* codec,
                                                              FILE* fp,
                                                              SquashOptions* options);

SQUASH_API SquashStatus squash_file_read                     (SquashFile* file,
                                                              size_t* decompressed_length,
                                                              uint8_t decompressed[SQUASH_ARRAY_PARAM(*decompressed_length)]);
SQUASH_API SquashStatus squash_file_write                    (SquashFile* file,
                                                              size_t uncompressed_length,
                                                              const uint8_t uncompressed[SQUASH_ARRAY_PARAM(uncompressed_length)]);
SQUASH_API SquashStatus squash_file_flush                    (SquashFile* file);

SQUASH_API SquashStatus squash_splice                        (const char* codec,
                                                              SquashStreamType stream_type,
                                                              FILE* fp_out,
                                                              FILE* fp_in,
                                                              size_t length,
                                                              ...);
SQUASH_API SquashStatus squash_splice_codec                  (SquashCodec* codec,
                                                              SquashStreamType stream_type,
                                                              FILE* fp_out,
                                                              FILE* fp_in,
                                                              size_t length,
                                                              ...);
SQUASH_API SquashStatus squash_splice_with_options           (const char* codec,
                                                              SquashStreamType stream_type,
                                                              FILE* fp_out,
                                                              FILE* fp_in,
                                                              size_t length,
                                                              SquashOptions* options);
SQUASH_API SquashStatus squash_splice_codec_with_options     (SquashCodec* codec,
                                                              SquashStreamType stream_type,
                                                              FILE* fp_out,
                                                              FILE* fp_in,
                                                              size_t length,
                                                              SquashOptions* options);

SQUASH_API SquashStatus squash_file_close                    (SquashFile* file);
SQUASH_API SquashStatus squash_file_free                     (SquashFile* file,
                                                              FILE** fp);
SQUASH_API bool         squash_file_eof                      (SquashFile* file);

SQUASH_END_DECLS

#endif /* SQUASH_FILE_H */
