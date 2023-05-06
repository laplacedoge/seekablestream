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

/**
 * the ring buffer structure of the seekable stream:
 * 
 * ┌───────────── allocated memory for ring buffer ─────────────┐
 * ┌────────────────── ring buffer ──────────────────┐          │
 * ┌──────┐ ╔═══════════════╦═══════════════╗ ┌──────┐ ┌────────┐
 * │ free │ ║     stale     ║     fresh     ║ │ free │ │ unused │
 * └──────┘ ╚═══════════════╩═══════════════╝ └──────┘ └────────┘
 *          └────────── used  size ─────────┘
 * └───────────────── capacity size ─────────────────┘
 * └────────────────────── allocated size ──────────────────────┘
*/

#include <stdlib.h>
#include <string.h>

#include "seekablestream.h"

struct _sstm_ctx {
    struct _sstm_ctx_conf {

        /* the actual usable memory size
           (i.e. capacity size) for seekable
           stream. */
        sstm_size_t cap_size;
    } conf;
    struct _sstm_ctx_cache {

        /* the allocated memory size for
           seekable stream. */
        sstm_size_t alloc_size;

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
    } cache;

    /* ring buffer. */
    sstm_u8_t *ring_buff;

    sstm_size_t head_idx;
    sstm_size_t tail_idx;

    /* current seeking offset. */
    sstm_size_t seek_offs;
};

/**
 * @brief create a new seekable stream.
 * 
 * @param ctx the pointer pointing to a context pointer.
 * @param conf configuration pointer.
*/
sstm_res_t sstm_new(sstm_ctx_t **ctx, sstm_conf_t *conf) {
    sstm_size_t cap_size;
    sstm_size_t alloc_size;
    sstm_u8_t *ring_buff;
    sstm_ctx_t *new_ctx;

    SSTM_ASSERT(ctx != NULL);

    /* determine the capacity size. */
    if (conf == NULL) {
        cap_size = SSTM_CAP_SIZE_DEF;
    } else {
        if (conf->cap_size < SSTM_CAP_SIZE_MIN) {
            cap_size = SSTM_CAP_SIZE_DEF;
        } else {
            cap_size = conf->cap_size;
        }
    }

    /* in the ring buffer, the memory size we will use
       is actually cap_size + 1, so we have to make sure
       the allocated memory size is enough. */
    alloc_size = ((cap_size >> 3) + 1) << 3;
    ring_buff = (sstm_u8_t *)malloc(alloc_size);
    if (ring_buff == NULL) {
        return SSTM_ERR_NO_MEM;
    }

    /* allocate context and initialize it. */
    new_ctx = (sstm_ctx_t *)malloc(sizeof(sstm_ctx_t));
    if (ring_buff == NULL) {
        free(ring_buff);

        return SSTM_ERR_NO_MEM;
    }
    new_ctx->conf.cap_size = cap_size;
    new_ctx->cache.alloc_size = alloc_size;
    new_ctx->cache.used_size = 0;
    new_ctx->cache.stale_size = 0;
    new_ctx->cache.fresh_size = 0;
    new_ctx->cache.free_size = cap_size;
    new_ctx->ring_buff = ring_buff;
    new_ctx->head_idx = 0;
    new_ctx->tail_idx = 0;
    new_ctx->seek_offs = 0;

    *ctx = new_ctx;

    return SSTM_OK;
}

/**
 * @brief delete a seekable stream.
 * 
 * @param ctx context pointer.
*/
sstm_res_t sstm_del(sstm_ctx_t *ctx) {
    SSTM_ASSERT(ctx != NULL);

    free(ctx->ring_buff);
    free(ctx);

    return SSTM_OK;
}

/**
 * @brief get the status of the seekable stream.
 * 
 * @param ctx context pointer.
 * @param stat status pointer.
*/
sstm_res_t sstm_stat(sstm_ctx_t *ctx, sstm_stat_t *stat) {
    SSTM_ASSERT(ctx != NULL);
    SSTM_ASSERT(stat != NULL);

    stat->cap_size = ctx->conf.cap_size;
    stat->used_size = ctx->cache.used_size;
    stat->stale_size = ctx->cache.stale_size;
    stat->fresh_size = ctx->cache.fresh_size;
    stat->free_size = ctx->cache.free_size;
    stat->seek_offs = ctx->seek_offs;

    return SSTM_OK;
}

/**
 * @brief clean the stale section of the seekable stream.
 * 
 * @param ctx context pointer.
*/
sstm_res_t sstm_clean(sstm_ctx_t *ctx) {
    sstm_size_t stale_size;

    SSTM_ASSERT(ctx != NULL);

    stale_size = ctx->cache.stale_size;
    if (stale_size == 0) {
        return SSTM_OK;
    }

    ctx->head_idx = (ctx->head_idx + stale_size) % (ctx->conf.cap_size + 1);

    /* update cache. */
    ctx->cache.used_size -= stale_size;
    ctx->cache.stale_size = 0;
    ctx->cache.free_size += stale_size;
    ctx->seek_offs = 0;

    return SSTM_OK;
}

/**
 * @brief read data from the stream.
 * 
 * @param ctx context pointer.
 * @param data data pointer, when NULL, no data will be copied.
 * @param size data size.
 * @param cleanup whether to clean the stale section after read.
*/
sstm_res_t sstm_read(sstm_ctx_t *ctx, void *data, sstm_size_t size, sstm_bool_t cleanup) {
    sstm_u8_t *first_copy_ptr;
    sstm_size_t new_head_idx;

    SSTM_ASSERT(ctx != NULL);

    if (size == 0) {
        return SSTM_OK;
    }

    if (ctx->cache.fresh_size < size) {
        return SSTM_ERR_NO_DATA;
    }

    /* copy data. */
    new_head_idx = (ctx->head_idx + ctx->seek_offs) % (ctx->conf.cap_size + 1);
    first_copy_ptr = ctx->ring_buff + new_head_idx;
    if (data != NULL) {
        if (ctx->conf.cap_size + 1 - new_head_idx >= size) {
            memcpy(data, first_copy_ptr, size);
        } else {
            sstm_size_t first_copy_size = ctx->conf.cap_size + 1 - new_head_idx;
            sstm_size_t second_copy_size = size - first_copy_size;

            memcpy(data, first_copy_ptr, first_copy_size);
            memcpy((sstm_u8_t *)data + first_copy_size, ctx->ring_buff, second_copy_size);
        }
    }
    ctx->seek_offs += size;

    /* update cache. */
    ctx->cache.stale_size += size;
    ctx->cache.fresh_size -= size;

    if (cleanup) {
        sstm_clean(ctx);
    }

    return SSTM_OK;
}

/**
 * @brief write data to the seekable stream.
 * 
 * @param ctx seekable stream context.
 * @param data data pointer, when NULL, 0x00 will be written.
 * @param size data size.
*/
sstm_res_t sstm_write(sstm_ctx_t *ctx, const void *data, sstm_size_t size) {
    sstm_u8_t *first_copy_ptr;

    SSTM_ASSERT(ctx != NULL);

    if (size == 0) {
        return SSTM_OK;
    }

    if (ctx->cache.free_size < size) {
        return SSTM_ERR_NO_SPACE;
    }

    /* copy data. */
    first_copy_ptr = ctx->ring_buff + ctx->tail_idx;
    if (ctx->conf.cap_size + 1 - ctx->tail_idx >= size) {
        if (data != NULL) {
            memcpy(first_copy_ptr, data, size);
        } else {
            memset(first_copy_ptr, 0, size);
        }
        ctx->tail_idx = (ctx->tail_idx + size) % (ctx->conf.cap_size + 1);
    } else {
        sstm_size_t first_copy_size = ctx->conf.cap_size + 1 - ctx->tail_idx;
        sstm_size_t second_copy_size = size - first_copy_size;

        if (data != NULL) {
            memcpy(first_copy_ptr, data, first_copy_size);
            memcpy(ctx->ring_buff, (sstm_u8_t *)data + first_copy_size, second_copy_size);
        } else {
            memset(first_copy_ptr, 0, first_copy_size);
            memset(ctx->ring_buff, 0, second_copy_size);
        }
        ctx->tail_idx = second_copy_size;
    }

    /* update cache. */
    ctx->cache.used_size += size;
    ctx->cache.fresh_size += size;
    ctx->cache.free_size -= size;

    return SSTM_OK;
}

/**
 * @brief seek the seekable stream.
 * 
 * @param ctx seekable stream context.
 * @param offset offset.
 * @param whence whence.
*/
sstm_res_t sstm_seek(sstm_ctx_t *ctx, sstm_offs_t offset, sstm_whence_t whence) {
    sstm_offs_t abs_offs;

    SSTM_ASSERT(ctx != NULL);
    SSTM_ASSERT(whence == SSTM_SEEK_SET ||
                whence == SSTM_SEEK_CUR ||
                whence == SSTM_SEEK_END);

    /* calculate the absolute offset. */
    switch (whence) {
        case SSTM_SEEK_SET: abs_offs = offset; break;
        case SSTM_SEEK_CUR: abs_offs = (sstm_offs_t)ctx->seek_offs + offset; break;
        case SSTM_SEEK_END: abs_offs = (sstm_offs_t)ctx->cache.used_size + offset; break;
    }

    /* check offset. */
    if (abs_offs < 0) {
        return SSTM_ERR_BAD_OFFS;
    }
    if ((sstm_size_t)abs_offs > ctx->cache.used_size) {
        return SSTM_ERR_BAD_OFFS;
    }
    if ((sstm_size_t)abs_offs == ctx->seek_offs) {
        return SSTM_OK;
    }

    /* update cache. */
    ctx->seek_offs = (sstm_size_t)abs_offs;
    ctx->cache.stale_size = (sstm_size_t)abs_offs;
    ctx->cache.fresh_size = ctx->cache.used_size - ctx->cache.stale_size;

    return SSTM_OK;
}
