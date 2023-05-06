/**
 * MIT License
 * 
 * Copyright (c) 2023 Alex Chen
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef __SSTM_H__
#define __SSTM_H__

#include <stddef.h>
#include <stdint.h>

/* basic data types. */
typedef int8_t              sstm_s8_t;
typedef uint8_t             sstm_u8_t;

typedef int16_t             sstm_s16_t;
typedef uint16_t            sstm_u16_t;

typedef int32_t             sstm_s32_t;
typedef uint32_t            sstm_u32_t;

typedef sstm_u8_t           sstm_bool_t;

typedef sstm_s32_t          sstm_res_t;

typedef sstm_u32_t          sstm_size_t;

typedef sstm_s32_t          sstm_offs_t;

typedef struct _sstm_ctx    sstm_ctx_t;

#ifndef SSTM_ASSERT
#define SSTM_ASSERT(cond)
#endif

typedef struct _sstm_stat {

    /* the actual usable memory size
       (i.e. capacity size) for seekable
       stream. */
    sstm_size_t cap_size;

    /* the currently used size. */
    sstm_size_t used_size;

    /* the size of data that has been read
        in used space within the seekable
        stream. */
    sstm_size_t stale_size;

    /* the size of data that hasn't been
        read in used space within the
        seekable stream. */
    sstm_size_t fresh_size;

    /* the currently available size. */
    sstm_size_t free_size;

    /* current seeking offset. */
    sstm_size_t seek_offs;
} sstm_stat_t;

typedef struct _sstm_conf {

    /* the capacity of seekable stream. */
    sstm_size_t cap_size;
} sstm_conf_t;

typedef enum _sstm_whence {

    /* seek from the start of the stream. */
    SSTM_SEEK_SET               = 0,

    /* seek from current position of the stream. */
    SSTM_SEEK_CUR,

    /* seek from the end of the stream. */
    SSTM_SEEK_END,
} sstm_whence_t;

#define SSTM_CAP_SIZE_MIN       128
#define SSTM_CAP_SIZE_DEF       1024

#define SSTM_OK                 0
#define SSTM_ERR                -1
#define SSTM_ERR_NO_MEM         -2
#define SSTM_ERR_NO_SPACE       -3
#define SSTM_ERR_NO_DATA        -4
#define SSTM_ERR_BAD_OFFS       -5

sstm_res_t sstm_new(sstm_ctx_t **ctx, sstm_conf_t *conf);

sstm_res_t sstm_del(sstm_ctx_t *ctx);

sstm_res_t sstm_stat(sstm_ctx_t *ctx, sstm_stat_t *stat);

sstm_res_t sstm_clean(sstm_ctx_t *ctx);

sstm_res_t sstm_read(sstm_ctx_t *ctx, void *data, sstm_size_t size, sstm_bool_t cleanup);

sstm_res_t sstm_write(sstm_ctx_t *ctx, const void *data, sstm_size_t size);

sstm_res_t sstm_seek(sstm_ctx_t *ctx, sstm_offs_t offset, sstm_whence_t whence);

#endif
