package webview

/*
#include "native/webview_models.h"
#include "native/webview_methods.h"
#include "native/webview_go_glue.h"

#include <stdlib.h>
#include <stdint.h>

extern void goOnChildWindowCreatedCallback(int, webview_t);
extern void _webviewDispatchGoCallback(void*);
extern void _webviewBindingGoCallback(webview_t, char *, char *,uintptr_t);

void c_goOnChildWindowCreatedCallback(int window_id, webview_t window){
	goOnChildWindowCreatedCallback(window_id, window);
}
void c_webviewDispatchGoCallback(void* w) {
	_webviewDispatchGoCallback(w);
}
void c_webviewBindingGoCallback(webview_t w, char * c1, char * c2, uintptr_t p1){
	_webviewBindingGoCallback(w, c1, c2, p1);
}
*/
import "C"
