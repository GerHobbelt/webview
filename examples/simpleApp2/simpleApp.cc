//bin/echo; [ $(uname) = "Darwin" ] && FLAGS="-framework Webkit" || FLAGS="$(pkg-config --cflags --libs gtk+-3.0 webkit2gtk-4.0)" ; c++ "$0" $FLAGS -std=c++11 -g -o webview && ./webview ; exit
// +build ignore

#include "webview.h"

#include <iostream>

#ifdef _WIN32
int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                     LPSTR lpCmdLine, int nCmdShow)
#else
int main()
#endif
{
  struct webview w = {};
  w.title = "simpleApp";
  w.url ="http://localhost:8081";
  w.width = 480;
  w.height = 320;
  w.resizable = 1;
  w.debug=true;
  webview_init(&w);
  while (webview_loop(&w, 1) == 0) {
  }
  webview_exit(&w);
  
  return 0;
}
