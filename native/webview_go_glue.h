#ifndef WEBVIEW_GO_GLUE
#define WEBVIEW_GO_GLUE

#include "webview_models.h"
#include <stdint.h>
#include <stdlib.h>

typedef void(*_webviewDispatchGoCallback_t) (void*);
typedef void(*_webviewBindingGoCallback_t) (webview_t, char *, char *,uintptr_t);

WEBVIEW_API void CgoWebViewDispatch(webview_t w, uintptr_t arg);
WEBVIEW_API void CgoWebViewBind(webview_t w, const char *name, uintptr_t index);

WEBVIEW_API void set_webviewDispatchGoCallback(_webviewDispatchGoCallback_t callback);
WEBVIEW_API void set_webviewBindingGoCallback(_webviewBindingGoCallback_t callback);

#endif // !WEBVIEW_GO_GLUE