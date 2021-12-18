//
//  utils_dispatch.c
//  xe
//
//  Created by admin on 11/27/21.
//

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdatomic.h>

#include <dispatch/dispatch.h>

#include "util_dispatch.h"


struct map_ctx {
    void* data;
    size_t element_size;
    size_t element_count;
    dispatch_apply_fn map_fn;
    void* map_fn_ctx;
    _Atomic int error;
};


static dispatch_queue_t s_xe_dispatch_queue;


void map_apply(void* data, size_t index) {
    struct map_ctx* ctx = (struct map_ctx*)data;
    if (ctx->error != 0) {
        return;
    }
    int error = ctx->map_fn(ctx->map_fn_ctx, ctx->data + (ctx->element_size * index), index);
    if (error) {
        int expected = 0;
        atomic_compare_exchange_strong(&ctx->error, &expected, error);
    }
}


int xe_util_dispatch_apply(void* data, size_t element_size, size_t element_count, void* map_fn_ctx, dispatch_apply_fn map_fn) {
    if (element_count == 1) {
        return map_fn(map_fn_ctx, data, 0);
    }

    struct map_ctx ctx;
    ctx.data = data;
    ctx.element_size = element_size;
    ctx.element_count = element_count;
    ctx.map_fn = map_fn;
    ctx.map_fn_ctx = map_fn_ctx;
    atomic_init(&ctx.error, 0);
    dispatch_apply_f(element_count, DISPATCH_APPLY_AUTO, &ctx, map_apply);
    return ctx.error;
}

dispatch_queue_t xe_dispatch_queue(void) {
    if (!s_xe_dispatch_queue) {
        s_xe_dispatch_queue = dispatch_queue_create("xe_dispatch_queue", DISPATCH_QUEUE_CONCURRENT);
    }
    return s_xe_dispatch_queue;
}
