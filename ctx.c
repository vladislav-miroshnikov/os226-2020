#include <string.h>

#include "ctx.h"

void ctx_make(struct ctx *ctx, void *entry, void *sp) {
        memset(ctx, 0, sizeof(*ctx));
        ctx->rsp = (unsigned long) sp;
        *(unsigned long *)ctx->rsp = (unsigned long) entry;
}
