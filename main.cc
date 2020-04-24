//bin/echo; [ $(uname) = "Darwin" ] && FLAGS="-framework Webkit" || FLAGS="$(pkg-config --cflags --libs gtk+-3.0 webkit2gtk-4.0)" ; c++ "$0" $FLAGS -std=c++11 -g -o webview && ./webview ; exit
// +build ignore

#include "webview.h"

#include <iostream>

struct Settings{
  bool debug = true;
  std::string title = "App";
  int defaultWidth = 1080;
  int defaultHeight = 800;
  std::string url="http://localhost:3000";
};


#ifdef _WIN32
int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                     LPSTR lpCmdLine, int nCmdShow)
#else
int main()
#endif
{
  Settings settings;


  webview::webview w(settings.debug, nullptr);
  w.set_title(settings.title);
  w.set_size(settings.defaultWidth, settings.defaultHeight, WEBVIEW_HINT_NONE);
  w.set_size(180, 120, WEBVIEW_HINT_MIN);
  w.navigate(settings.url);
  w.run();
  return 0;
}
