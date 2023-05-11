#include "webview_models.h"
#include "webview_go_glue.h"
#include "webview_methods.h"
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>


struct binding_context {
  webview_t w;
  uintptr_t index;
};

static _webviewDispatchGoCallback_t __webviewDispatchGoCallback = NULL;
static _webviewBindingGoCallback_t __webviewBindingGoCallback = NULL;

void _webview_dispatch_cb(webview_t w, void *arg) {
  if (__webviewDispatchGoCallback) {
    __webviewDispatchGoCallback(arg);
  } else {
    printf("__webviewDispatchGoCallback is NULL \n");
  }
}

void _webview_binding_cb(const char *id, const char *req, void *arg) {
  struct binding_context *ctx = (struct binding_context *)arg;
  if (__webviewBindingGoCallback) {
    __webviewBindingGoCallback(ctx->w, (char *)id, (char *)req, ctx->index);
  } else {
    printf("__webviewBindingGoCallback is NULL \n");
  }
}

WEBVIEW_API void CgoWebViewDispatch(webview_t w, uintptr_t arg) {
  webview_dispatch(w, _webview_dispatch_cb, (void *)arg);
}

WEBVIEW_API void CgoWebViewBind(webview_t w, const char *name, uintptr_t index) {
  struct binding_context *ctx = calloc(1, sizeof(struct binding_context));
  ctx->w = w;
  ctx->index = index;
  webview_bind(w, name, _webview_binding_cb, (void *)ctx);
}

WEBVIEW_API void set_webviewDispatchGoCallback(_webviewDispatchGoCallback_t callback) {
  __webviewDispatchGoCallback = callback;
}
WEBVIEW_API void set_webviewBindingGoCallback(_webviewBindingGoCallback_t callback) {
  __webviewBindingGoCallback = callback;
}