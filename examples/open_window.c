#include "webview.h"
#include <stddef.h>

#ifdef _WIN32
#include <Windows.h>
#endif
#define UNUSED(x) (void)x
webview_t w = NULL;
void increment(const char *seq, const char *req, void *arg) {
  UNUSED(req);
  char *url = (char *)arg;
  webview_navigate_popup(w, url, 1);
  webview_return(w, seq, 0, "");
}
void hello(const char *seq, const char *req, void *arg) { 
	UNUSED(req);
	printf("hello called\n");
	webview_return(w, seq, 0, "");
}
void child_window_callback(int window_id, webview_t window) {
  printf("child_window_callback window=%d window=%p\n", window_id, window);
  webview_set_size(window, 1024, 768, 0);
  webview_bind(window, "hello", hello, NULL);
}
void child_window_closed_callback(int window_id) {
    printf("child window closed callback, window id=%d \n", window_id);
}
#ifdef _WIN32
//int WINAPI WinMain(HINSTANCE hInt, HINSTANCE hPrevInst, LPSTR lpCmdLine,
//                   int nCmdShow) {
int main() {
#else
int main() {
#endif
  char url_arg[] = "http://127.0.0.1:3030/child.html";
  w = webview_create(1, NULL);
  webview_set_title(w, "Basic Example");
  webview_set_size(w, 480, 320, WEBVIEW_HINT_NONE);
  webview_set_child_window_opened_callback(child_window_callback);
  webview_set_child_window_closed_callback(child_window_closed_callback);
  webview_bind(w, "openPopup", increment, &url_arg);
  webview_navigate(w, "http://127.0.0.1:3030/webview.html");
  webview_run(w);
  webview_destroy(w);
  return 0;
}
