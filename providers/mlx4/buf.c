/*
 * Copyright (c) 2006, 2007 Cisco, Inc.  All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <config.h>

#include <stdlib.h>
#include <errno.h>
#include <sys/mman.h>

#include "mlx4.h"

static void mlx4_free_buf_extern(struct mlx4_context *ctx, struct mlx4_buf *buf)
{
	ibv_dofork_range(buf->buf, buf->length);
	ctx->extern_alloc.free(buf->buf, ctx->extern_alloc.data);
}

static int mlx4_alloc_buf_extern(struct mlx4_context *ctx, struct mlx4_buf *buf,
				 size_t size)
{
	void *addr;

	addr = ctx->extern_alloc.alloc(size, ctx->extern_alloc.data);
	if (addr || size == 0) {
		if (ibv_dontfork_range(addr, size)) {
			ctx->extern_alloc.free(addr,
				ctx->extern_alloc.data);
			return -1;
		}
		buf->buf = addr;
		buf->length = size;
		return 0;
	}

	return -1;
}

static bool mlx4_is_extern_alloc(struct mlx4_context *context)
{
	return context->extern_alloc.alloc && context->extern_alloc.free;
}

int mlx4_alloc_buf(struct mlx4_context *ctx, struct mlx4_buf *buf,
		   size_t size, int page_size)
{
	int ret;

	if (mlx4_is_extern_alloc(ctx))
		return mlx4_alloc_buf_extern(ctx, buf, size);

	buf->length = align(size, page_size);
	buf->buf = mmap(NULL, buf->length, PROT_READ | PROT_WRITE,
			MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (buf->buf == MAP_FAILED)
		return errno;

	ret = ibv_dontfork_range(buf->buf, size);
	if (ret)
		munmap(buf->buf, buf->length);

	return ret;
}

void mlx4_free_buf(struct mlx4_context *context, struct mlx4_buf *buf)
{
	if (mlx4_is_extern_alloc(context))
		return mlx4_free_buf_extern(context, buf);

	if (buf->length) {
		ibv_dofork_range(buf->buf, buf->length);
		munmap(buf->buf, buf->length);
	}
}
