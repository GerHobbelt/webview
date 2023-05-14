/*
 * MIT License
 *
 * Copyright (c) 2017 Serge Zaitsev
 * Copyright (c) 2022 Steffen André Langnes
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
#ifndef WEBVIEW_H
#define WEBVIEW_H

#include "webview_models.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "webview_methods.h"

#ifdef __cplusplus
}

#include "logging.h"

#ifndef WEBVIEW_HEADER

#if !defined(WEBVIEW_GTK) && !defined(WEBVIEW_COCOA) && !defined(WEBVIEW_EDGE)
#if defined(__APPLE__)
#define WEBVIEW_COCOA
#elif defined(__unix__)
#define WEBVIEW_GTK
#elif defined(_WIN32)
#define WEBVIEW_EDGE
#else
#error "please, specify webview backend"
#endif
#endif

#ifndef WEBVIEW_DEPRECATED
#if __cplusplus >= 201402L
#define WEBVIEW_DEPRECATED(reason) [[deprecated(reason)]]
#elif defined(_MSC_VER)
#define WEBVIEW_DEPRECATED(reason) __declspec(deprecated(reason))
#else
#define WEBVIEW_DEPRECATED(reason) __attribute__((deprecated(reason)))
#endif
#endif

#ifndef WEBVIEW_DEPRECATED_PRIVATE
#define WEBVIEW_DEPRECATED_PRIVATE                                             \
  WEBVIEW_DEPRECATED("Private API should not be used")
#endif

#include <array>
#include <atomic>
#include <functional>
#include <future>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include <cstring>

namespace webview {

using dispatch_fn_t = std::function<void()>;

namespace detail {

// The library's version information.
constexpr const webview_version_info_t library_version_info{
    {WEBVIEW_VERSION_MAJOR, WEBVIEW_VERSION_MINOR, WEBVIEW_VERSION_PATCH},
    WEBVIEW_VERSION_NUMBER,
    WEBVIEW_VERSION_PRE_RELEASE,
    WEBVIEW_VERSION_BUILD_METADATA};

inline int json_parse_c(const char *s, size_t sz, const char *key, size_t keysz,
                        const char **value, size_t *valuesz) {
  enum {
    JSON_STATE_VALUE,
    JSON_STATE_LITERAL,
    JSON_STATE_STRING,
    JSON_STATE_ESCAPE,
    JSON_STATE_UTF8
  } state = JSON_STATE_VALUE;
  const char *k = nullptr;
  int index = 1;
  int depth = 0;
  int utf8_bytes = 0;

  *value = nullptr;
  *valuesz = 0;

  if (key == nullptr) {
    index = static_cast<decltype(index)>(keysz);
    if (index < 0) {
      return -1;
    }
    keysz = 0;
  }

  for (; sz > 0; s++, sz--) {
    enum {
      JSON_ACTION_NONE,
      JSON_ACTION_START,
      JSON_ACTION_END,
      JSON_ACTION_START_STRUCT,
      JSON_ACTION_END_STRUCT
    } action = JSON_ACTION_NONE;
    auto c = static_cast<unsigned char>(*s);
    switch (state) {
    case JSON_STATE_VALUE:
      if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == ',' ||
          c == ':') {
        continue;
      } else if (c == '"') {
        action = JSON_ACTION_START;
        state = JSON_STATE_STRING;
      } else if (c == '{' || c == '[') {
        action = JSON_ACTION_START_STRUCT;
      } else if (c == '}' || c == ']') {
        action = JSON_ACTION_END_STRUCT;
      } else if (c == 't' || c == 'f' || c == 'n' || c == '-' ||
                 (c >= '0' && c <= '9')) {
        action = JSON_ACTION_START;
        state = JSON_STATE_LITERAL;
      } else {
        return -1;
      }
      break;
    case JSON_STATE_LITERAL:
      if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == ',' ||
          c == ']' || c == '}' || c == ':') {
        state = JSON_STATE_VALUE;
        s--;
        sz++;
        action = JSON_ACTION_END;
      } else if (c < 32 || c > 126) {
        return -1;
      } // fallthrough
    case JSON_STATE_STRING:
      if (c < 32 || (c > 126 && c < 192)) {
        return -1;
      } else if (c == '"') {
        action = JSON_ACTION_END;
        state = JSON_STATE_VALUE;
      } else if (c == '\\') {
        state = JSON_STATE_ESCAPE;
      } else if (c >= 192 && c < 224) {
        utf8_bytes = 1;
        state = JSON_STATE_UTF8;
      } else if (c >= 224 && c < 240) {
        utf8_bytes = 2;
        state = JSON_STATE_UTF8;
      } else if (c >= 240 && c < 247) {
        utf8_bytes = 3;
        state = JSON_STATE_UTF8;
      } else if (c >= 128 && c < 192) {
        return -1;
      }
      break;
    case JSON_STATE_ESCAPE:
      if (c == '"' || c == '\\' || c == '/' || c == 'b' || c == 'f' ||
          c == 'n' || c == 'r' || c == 't' || c == 'u') {
        state = JSON_STATE_STRING;
      } else {
        return -1;
      }
      break;
    case JSON_STATE_UTF8:
      if (c < 128 || c > 191) {
        return -1;
      }
      utf8_bytes--;
      if (utf8_bytes == 0) {
        state = JSON_STATE_STRING;
      }
      break;
    default:
      return -1;
    }

    if (action == JSON_ACTION_END_STRUCT) {
      depth--;
    }

    if (depth == 1) {
      if (action == JSON_ACTION_START || action == JSON_ACTION_START_STRUCT) {
        if (index == 0) {
          *value = s;
        } else if (keysz > 0 && index == 1) {
          k = s;
        } else {
          index--;
        }
      } else if (action == JSON_ACTION_END ||
                 action == JSON_ACTION_END_STRUCT) {
        if (*value != nullptr && index == 0) {
          *valuesz = (size_t)(s + 1 - *value);
          return 0;
        } else if (keysz > 0 && k != nullptr) {
          if (keysz == (size_t)(s - k - 1) && memcmp(key, k + 1, keysz) == 0) {
            index = 0;
          } else {
            index = 2;
          }
          k = nullptr;
        }
      }
    }

    if (action == JSON_ACTION_START_STRUCT) {
      depth++;
    }
  }
  return -1;
}

inline std::string json_escape(const std::string &s) {
  // TODO: implement
  return '"' + s + '"';
}

inline int json_unescape(const char *s, size_t n, char *out) {
  int r = 0;
  if (*s++ != '"') {
    return -1;
  }
  while (n > 2) {
    char c = *s;
    if (c == '\\') {
      s++;
      n--;
      switch (*s) {
      case 'b':
        c = '\b';
        break;
      case 'f':
        c = '\f';
        break;
      case 'n':
        c = '\n';
        break;
      case 'r':
        c = '\r';
        break;
      case 't':
        c = '\t';
        break;
      case '\\':
        c = '\\';
        break;
      case '/':
        c = '/';
        break;
      case '\"':
        c = '\"';
        break;
      default: // TODO: support unicode decoding
        return -1;
      }
    }
    if (out != nullptr) {
      *out++ = c;
    }
    s++;
    n--;
    r++;
  }
  if (*s != '"') {
    return -1;
  }
  if (out != nullptr) {
    *out = '\0';
  }
  return r;
}

inline std::string json_parse(const std::string &s, const std::string &key,
                              const int index) {
  const char *value;
  size_t value_sz;
  if (key.empty()) {
    json_parse_c(s.c_str(), s.length(), nullptr, index, &value, &value_sz);
  } else {
    json_parse_c(s.c_str(), s.length(), key.c_str(), key.length(), &value,
                 &value_sz);
  }
  if (value != nullptr) {
    if (value[0] != '"') {
      return {value, value_sz};
    }
    int n = json_unescape(value, value_sz, nullptr);
    if (n > 0) {
      char *decoded = new char[n + 1];
      json_unescape(value, value_sz, decoded);
      std::string result(decoded, n);
      delete[] decoded;
      return result;
    }
  }
  return "";
}

} // namespace detail

WEBVIEW_DEPRECATED_PRIVATE
inline int json_parse_c(const char *s, size_t sz, const char *key, size_t keysz,
                        const char **value, size_t *valuesz) {
  return detail::json_parse_c(s, sz, key, keysz, value, valuesz);
}

WEBVIEW_DEPRECATED_PRIVATE
inline std::string json_escape(const std::string &s) {
  return detail::json_escape(s);
}

WEBVIEW_DEPRECATED_PRIVATE
inline int json_unescape(const char *s, size_t n, char *out) {
  return detail::json_unescape(s, n, out);
}

WEBVIEW_DEPRECATED_PRIVATE
inline std::string json_parse(const std::string &s, const std::string &key,
                              const int index) {
  return detail::json_parse(s, key, index);
}

} // namespace webview

#if defined(WEBVIEW_GTK)
//
// ====================================================================
//
// This implementation uses webkit2gtk backend. It requires gtk+3.0 and
// webkit2gtk-4.0 libraries. Proper compiler flags can be retrieved via:
//
//   pkg-config --cflags --libs gtk+-3.0 webkit2gtk-4.0
//
// ====================================================================
//
#include <JavaScriptCore/JavaScript.h>
#include <gtk/gtk.h>
#include <webkit2/webkit2.h>

namespace webview {
namespace detail {

class gtk_webkit_engine {
public:
  gtk_webkit_engine(bool debug, void *window)
      : m_window(static_cast<GtkWidget *>(window)) {
    if (gtk_init_check(nullptr, nullptr) == FALSE) {
      return;
    }
    m_window = static_cast<GtkWidget *>(window);
    if (m_window == nullptr) {
      m_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    }
    g_signal_connect(G_OBJECT(m_window), "destroy",
                     G_CALLBACK(+[](GtkWidget *, gpointer arg) {
                       static_cast<gtk_webkit_engine *>(arg)->terminate();
                     }),
                     this);
    // Initialize webview widget
    m_webview = webkit_web_view_new();
    WebKitUserContentManager *manager =
        webkit_web_view_get_user_content_manager(WEBKIT_WEB_VIEW(m_webview));
    g_signal_connect(manager, "script-message-received::external",
                     G_CALLBACK(+[](WebKitUserContentManager *,
                                    WebKitJavascriptResult *r, gpointer arg) {
                       auto *w = static_cast<gtk_webkit_engine *>(arg);
                       char *s = get_string_from_js_result(r);
                       w->on_message(s);
                       g_free(s);
                     }),
                     this);
    webkit_user_content_manager_register_script_message_handler(manager,
                                                                "external");
    init("window.external={invoke:function(s){window.webkit.messageHandlers."
         "external.postMessage(s);}}");

    gtk_container_add(GTK_CONTAINER(m_window), GTK_WIDGET(m_webview));
    gtk_widget_grab_focus(GTK_WIDGET(m_webview));

    WebKitSettings *settings =
        webkit_web_view_get_settings(WEBKIT_WEB_VIEW(m_webview));
    webkit_settings_set_javascript_can_access_clipboard(settings, true);
    if (debug) {
      webkit_settings_set_enable_write_console_messages_to_stdout(settings,
                                                                  true);
      webkit_settings_set_enable_developer_extras(settings, true);
    }

    gtk_widget_show_all(m_window);
  }
  virtual ~gtk_webkit_engine() = default;
  void *window() { return (void *)m_window; }
  void run() { gtk_main(); }
  void terminate() { gtk_main_quit(); }
  void dispatch(std::function<void()> f) {
    g_idle_add_full(G_PRIORITY_HIGH_IDLE, (GSourceFunc)([](void *f) -> int {
                      (*static_cast<dispatch_fn_t *>(f))();
                      return G_SOURCE_REMOVE;
                    }),
                    new std::function<void()>(f),
                    [](void *f) { delete static_cast<dispatch_fn_t *>(f); });
  }

  void set_title(const std::string &title) {
    gtk_window_set_title(GTK_WINDOW(m_window), title.c_str());
  }

  void set_size(int width, int height, int hints) {
    gtk_window_set_resizable(GTK_WINDOW(m_window), hints != WEBVIEW_HINT_FIXED);
    if (hints == WEBVIEW_HINT_NONE) {
      gtk_window_resize(GTK_WINDOW(m_window), width, height);
    } else if (hints == WEBVIEW_HINT_FIXED) {
      gtk_widget_set_size_request(m_window, width, height);
    } else {
      GdkGeometry g;
      g.min_width = g.max_width = width;
      g.min_height = g.max_height = height;
      GdkWindowHints h =
          (hints == WEBVIEW_HINT_MIN ? GDK_HINT_MIN_SIZE : GDK_HINT_MAX_SIZE);
      // This defines either MIN_SIZE, or MAX_SIZE, but not both:
      gtk_window_set_geometry_hints(GTK_WINDOW(m_window), nullptr, &g, h);
    }
  }

  void navigate(const std::string &url) {
    webkit_web_view_load_uri(WEBKIT_WEB_VIEW(m_webview), url.c_str());
  }

  void set_html(const std::string &html) {
    webkit_web_view_load_html(WEBKIT_WEB_VIEW(m_webview), html.c_str(),
                              nullptr);
  }

  void init(const std::string &js) {
    WebKitUserContentManager *manager =
        webkit_web_view_get_user_content_manager(WEBKIT_WEB_VIEW(m_webview));
    webkit_user_content_manager_add_script(
        manager,
        webkit_user_script_new(js.c_str(), WEBKIT_USER_CONTENT_INJECT_TOP_FRAME,
                               WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_START,
                               nullptr, nullptr));
  }

  void eval(const std::string &js) {
    webkit_web_view_run_javascript(WEBKIT_WEB_VIEW(m_webview), js.c_str(),
                                   nullptr, nullptr, nullptr);
  }

private:
  virtual void on_message(const std::string &msg) = 0;

  static char *get_string_from_js_result(WebKitJavascriptResult *r) {
    char *s;
#if WEBKIT_MAJOR_VERSION >= 2 && WEBKIT_MINOR_VERSION >= 22
    JSCValue *value = webkit_javascript_result_get_js_value(r);
    s = jsc_value_to_string(value);
#else
    JSGlobalContextRef ctx = webkit_javascript_result_get_global_context(r);
    JSValueRef value = webkit_javascript_result_get_value(r);
    JSStringRef js = JSValueToStringCopy(ctx, value, nullptr);
    size_t n = JSStringGetMaximumUTF8CStringSize(js);
    s = g_new(char, n);
    JSStringGetUTF8CString(js, s, n);
    JSStringRelease(js);
#endif
    return s;
  }

  GtkWidget *m_window;
  GtkWidget *m_webview;
};

} // namespace detail

using browser_engine = detail::gtk_webkit_engine;

} // namespace webview

#elif defined(WEBVIEW_COCOA)

//
// ====================================================================
//
// This implementation uses Cocoa WKWebView backend on macOS. It is
// written using ObjC runtime and uses WKWebView class as a browser runtime.
// You should pass "-framework Webkit" flag to the compiler.
//
// ====================================================================
//

#include <CoreGraphics/CoreGraphics.h>
#include <objc/NSObjCRuntime.h>
#include <objc/objc-runtime.h>

namespace webview {
namespace detail {
namespace objc {

// A convenient template function for unconditionally casting the specified
// C-like function into a function that can be called with the given return
// type and arguments. Caller takes full responsibility for ensuring that
// the function call is valid. It is assumed that the function will not
// throw exceptions.
template <typename Result, typename Callable, typename... Args>
Result invoke(Callable callable, Args... args) noexcept {
  return reinterpret_cast<Result (*)(Args...)>(callable)(args...);
}

// Calls objc_msgSend.
template <typename Result, typename... Args>
Result msg_send(Args... args) noexcept {
  return invoke<Result>(objc_msgSend, args...);
}

} // namespace objc

enum NSBackingStoreType : NSUInteger { NSBackingStoreBuffered = 2 };

enum NSWindowStyleMask : NSUInteger {
  NSWindowStyleMaskTitled = 1,
  NSWindowStyleMaskClosable = 2,
  NSWindowStyleMaskMiniaturizable = 4,
  NSWindowStyleMaskResizable = 8
};

enum NSApplicationActivationPolicy : NSInteger {
  NSApplicationActivationPolicyRegular = 0
};

enum WKUserScriptInjectionTime : NSInteger {
  WKUserScriptInjectionTimeAtDocumentStart = 0
};

enum NSModalResponse : NSInteger { NSModalResponseOK = 1 };

// Convenient conversion of string literals.
inline id operator"" _cls(const char *s, std::size_t) {
  return (id)objc_getClass(s);
}
inline SEL operator"" _sel(const char *s, std::size_t) {
  return sel_registerName(s);
}
inline id operator"" _str(const char *s, std::size_t) {
  return objc::msg_send<id>("NSString"_cls, "stringWithUTF8String:"_sel, s);
}

class cocoa_wkwebview_engine {
public:
  cocoa_wkwebview_engine(bool debug, void *window)
      : m_debug{debug}, m_parent_window{window} {
    auto app = get_shared_application();
    auto delegate = create_app_delegate();
    objc_setAssociatedObject(delegate, "webview", (id)this,
                             OBJC_ASSOCIATION_ASSIGN);
    objc::msg_send<void>(app, "setDelegate:"_sel, delegate);

    // See comments related to application lifecycle in create_app_delegate().
    if (window) {
      on_application_did_finish_launching(delegate, app);
    } else {
      // Start the main run loop so that the app delegate gets the
      // NSApplicationDidFinishLaunchingNotification notification after the run
      // loop has started in order to perform further initialization.
      // We need to return from this constructor so this run loop is only
      // temporary.
      objc::msg_send<void>(app, "run"_sel);
    }
  }
  virtual ~cocoa_wkwebview_engine() = default;
  void *window() { return (void *)m_window; }
  void terminate() {
    auto app = get_shared_application();
    objc::msg_send<void>(app, "terminate:"_sel, nullptr);
  }
  void run() {
    auto app = get_shared_application();
    objc::msg_send<void>(app, "run"_sel);
  }
  void dispatch(std::function<void()> f) {
    dispatch_async_f(dispatch_get_main_queue(), new dispatch_fn_t(f),
                     (dispatch_function_t)([](void *arg) {
                       auto f = static_cast<dispatch_fn_t *>(arg);
                       (*f)();
                       delete f;
                     }));
  }
  void set_title(const std::string &title) {
    objc::msg_send<void>(m_window, "setTitle:"_sel,
                         objc::msg_send<id>("NSString"_cls,
                                            "stringWithUTF8String:"_sel,
                                            title.c_str()));
  }
  void set_size(int width, int height, int hints) {
    auto style = static_cast<NSWindowStyleMask>(
        NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
        NSWindowStyleMaskMiniaturizable);
    if (hints != WEBVIEW_HINT_FIXED) {
      style =
          static_cast<NSWindowStyleMask>(style | NSWindowStyleMaskResizable);
    }
    objc::msg_send<void>(m_window, "setStyleMask:"_sel, style);

    if (hints == WEBVIEW_HINT_MIN) {
      objc::msg_send<void>(m_window, "setContentMinSize:"_sel,
                           CGSizeMake(width, height));
    } else if (hints == WEBVIEW_HINT_MAX) {
      objc::msg_send<void>(m_window, "setContentMaxSize:"_sel,
                           CGSizeMake(width, height));
    } else {
      objc::msg_send<void>(m_window, "setFrame:display:animate:"_sel,
                           CGRectMake(0, 0, width, height), YES, NO);
    }
    objc::msg_send<void>(m_window, "center"_sel);
  }
  void navigate(const std::string &url) {
    auto nsurl = objc::msg_send<id>(
        "NSURL"_cls, "URLWithString:"_sel,
        objc::msg_send<id>("NSString"_cls, "stringWithUTF8String:"_sel,
                           url.c_str()));

    objc::msg_send<void>(
        m_webview, "loadRequest:"_sel,
        objc::msg_send<id>("NSURLRequest"_cls, "requestWithURL:"_sel, nsurl));
  }
  void set_html(const std::string &html) {
    objc::msg_send<void>(m_webview, "loadHTMLString:baseURL:"_sel,
                         objc::msg_send<id>("NSString"_cls,
                                            "stringWithUTF8String:"_sel,
                                            html.c_str()),
                         nullptr);
  }
  void init(const std::string &js) {
    // Equivalent Obj-C:
    // [m_manager addUserScript:[[WKUserScript alloc] initWithSource:[NSString stringWithUTF8String:js.c_str()] injectionTime:WKUserScriptInjectionTimeAtDocumentStart forMainFrameOnly:YES]]
    objc::msg_send<void>(
        m_manager, "addUserScript:"_sel,
        objc::msg_send<id>(objc::msg_send<id>("WKUserScript"_cls, "alloc"_sel),
                           "initWithSource:injectionTime:forMainFrameOnly:"_sel,
                           objc::msg_send<id>("NSString"_cls,
                                              "stringWithUTF8String:"_sel,
                                              js.c_str()),
                           WKUserScriptInjectionTimeAtDocumentStart, YES));
  }
  void eval(const std::string &js) {
    objc::msg_send<void>(m_webview, "evaluateJavaScript:completionHandler:"_sel,
                         objc::msg_send<id>("NSString"_cls,
                                            "stringWithUTF8String:"_sel,
                                            js.c_str()),
                         nullptr);
  }

private:
  virtual void on_message(const std::string &msg) = 0;
  id create_app_delegate() {
    // Note: Avoid registering the class name "AppDelegate" as it is the
    // default name in projects created with Xcode, and using the same name
    // causes objc_registerClassPair to crash.
    auto cls = objc_allocateClassPair((Class) "NSResponder"_cls,
                                      "WebviewAppDelegate", 0);
    class_addProtocol(cls, objc_getProtocol("NSTouchBarProvider"));
    class_addMethod(cls, "applicationShouldTerminateAfterLastWindowClosed:"_sel,
                    (IMP)(+[](id, SEL, id) -> BOOL { return 1; }), "c@:@");
    // If the library was not initialized with an existing window then the user
    // is likely managing the application lifecycle and we would not get the
    // "applicationDidFinishLaunching:" message and therefore do not need to
    // add this method.
    if (!m_parent_window) {
      class_addMethod(cls, "applicationDidFinishLaunching:"_sel,
                      (IMP)(+[](id self, SEL, id notification) {
                        auto app =
                            objc::msg_send<id>(notification, "object"_sel);
                        auto w = get_associated_webview(self);
                        w->on_application_did_finish_launching(self, app);
                      }),
                      "v@:@");
    }
    objc_registerClassPair(cls);
    return objc::msg_send<id>((id)cls, "new"_sel);
  }
  id create_script_message_handler() {
    auto cls = objc_allocateClassPair((Class) "NSResponder"_cls,
                                      "WebkitScriptMessageHandler", 0);
    class_addProtocol(cls, objc_getProtocol("WKScriptMessageHandler"));
    class_addMethod(
        cls, "userContentController:didReceiveScriptMessage:"_sel,
        (IMP)(+[](id self, SEL, id, id msg) {
          auto w = get_associated_webview(self);
          w->on_message(objc::msg_send<const char *>(
              objc::msg_send<id>(msg, "body"_sel), "UTF8String"_sel));
        }),
        "v@:@@");
    objc_registerClassPair(cls);
    auto instance = objc::msg_send<id>((id)cls, "new"_sel);
    objc_setAssociatedObject(instance, "webview", (id)this,
                             OBJC_ASSOCIATION_ASSIGN);
    return instance;
  }
  static id create_webkit_ui_delegate() {
    auto cls =
        objc_allocateClassPair((Class) "NSObject"_cls, "WebkitUIDelegate", 0);
    class_addProtocol(cls, objc_getProtocol("WKUIDelegate"));
    class_addMethod(
        cls,
        "webView:runOpenPanelWithParameters:initiatedByFrame:completionHandler:"_sel,
        (IMP)(+[](id, SEL, id, id parameters, id, id completion_handler) {
          auto allows_multiple_selection =
              objc::msg_send<BOOL>(parameters, "allowsMultipleSelection"_sel);
          auto allows_directories =
              objc::msg_send<BOOL>(parameters, "allowsDirectories"_sel);

          // Show a panel for selecting files.
          auto panel = objc::msg_send<id>("NSOpenPanel"_cls, "openPanel"_sel);
          objc::msg_send<void>(panel, "setCanChooseFiles:"_sel, YES);
          objc::msg_send<void>(panel, "setCanChooseDirectories:"_sel,
                               allows_directories);
          objc::msg_send<void>(panel, "setAllowsMultipleSelection:"_sel,
                               allows_multiple_selection);
          auto modal_response =
              objc::msg_send<NSModalResponse>(panel, "runModal"_sel);

          // Get the URLs for the selected files. If the modal was canceled
          // then we pass null to the completion handler to signify
          // cancellation.
          id urls = modal_response == NSModalResponseOK
                        ? objc::msg_send<id>(panel, "URLs"_sel)
                        : nullptr;

          // Invoke the completion handler block.
          auto sig = objc::msg_send<id>("NSMethodSignature"_cls,
                                        "signatureWithObjCTypes:"_sel, "v@?@");
          auto invocation = objc::msg_send<id>(
              "NSInvocation"_cls, "invocationWithMethodSignature:"_sel, sig);
          objc::msg_send<void>(invocation, "setTarget:"_sel,
                               completion_handler);
          objc::msg_send<void>(invocation, "setArgument:atIndex:"_sel, &urls,
                               1);
          objc::msg_send<void>(invocation, "invoke"_sel);
        }),
        "v@:@@@@");
    objc_registerClassPair(cls);
    return objc::msg_send<id>((id)cls, "new"_sel);
  }
  static id get_shared_application() {
    return objc::msg_send<id>("NSApplication"_cls, "sharedApplication"_sel);
  }
  static cocoa_wkwebview_engine *get_associated_webview(id object) {
    auto w =
        (cocoa_wkwebview_engine *)objc_getAssociatedObject(object, "webview");
    assert(w);
    return w;
  }
  static id get_main_bundle() noexcept {
    return objc::msg_send<id>("NSBundle"_cls, "mainBundle"_sel);
  }
  static bool is_app_bundled() noexcept {
    auto bundle = get_main_bundle();
    if (!bundle) {
      return false;
    }
    auto bundle_path = objc::msg_send<id>(bundle, "bundlePath"_sel);
    auto bundled =
        objc::msg_send<BOOL>(bundle_path, "hasSuffix:"_sel, ".app"_str);
    return !!bundled;
  }
  void on_application_did_finish_launching(id /*delegate*/, id app) {
    // See comments related to application lifecycle in create_app_delegate().
    if (!m_parent_window) {
      // Stop the main run loop so that we can return
      // from the constructor.
      objc::msg_send<void>(app, "stop:"_sel, nullptr);
    }

    // Activate the app if it is not bundled.
    // Bundled apps launched from Finder are activated automatically but
    // otherwise not. Activating the app even when it has been launched from
    // Finder does not seem to be harmful but calling this function is rarely
    // needed as proper activation is normally taken care of for us.
    // Bundled apps have a default activation policy of
    // NSApplicationActivationPolicyRegular while non-bundled apps have a
    // default activation policy of NSApplicationActivationPolicyProhibited.
    if (!is_app_bundled()) {
      // "setActivationPolicy:" must be invoked before
      // "activateIgnoringOtherApps:" for activation to work.
      objc::msg_send<void>(app, "setActivationPolicy:"_sel,
                           NSApplicationActivationPolicyRegular);
      // Activate the app regardless of other active apps.
      // This can be obtrusive so we only do it when necessary.
      objc::msg_send<void>(app, "activateIgnoringOtherApps:"_sel, YES);
    }

    // Main window
    if (!m_parent_window) {
      m_window = objc::msg_send<id>("NSWindow"_cls, "alloc"_sel);
      auto style = NSWindowStyleMaskTitled;
      m_window = objc::msg_send<id>(
          m_window, "initWithContentRect:styleMask:backing:defer:"_sel,
          CGRectMake(0, 0, 0, 0), style, NSBackingStoreBuffered, NO);
    } else {
      m_window = (id)m_parent_window;
    }

    // Webview
    auto config = objc::msg_send<id>("WKWebViewConfiguration"_cls, "new"_sel);
    m_manager = objc::msg_send<id>(config, "userContentController"_sel);
    m_webview = objc::msg_send<id>("WKWebView"_cls, "alloc"_sel);

    if (m_debug) {
      // Equivalent Obj-C:
      // [[config preferences] setValue:@YES forKey:@"developerExtrasEnabled"];
      objc::msg_send<id>(
          objc::msg_send<id>(config, "preferences"_sel), "setValue:forKey:"_sel,
          objc::msg_send<id>("NSNumber"_cls, "numberWithBool:"_sel, YES),
          "developerExtrasEnabled"_str);
    }

    // Equivalent Obj-C:
    // [[config preferences] setValue:@YES forKey:@"fullScreenEnabled"];
    objc::msg_send<id>(
        objc::msg_send<id>(config, "preferences"_sel), "setValue:forKey:"_sel,
        objc::msg_send<id>("NSNumber"_cls, "numberWithBool:"_sel, YES),
        "fullScreenEnabled"_str);

    // Equivalent Obj-C:
    // [[config preferences] setValue:@YES forKey:@"javaScriptCanAccessClipboard"];
    objc::msg_send<id>(
        objc::msg_send<id>(config, "preferences"_sel), "setValue:forKey:"_sel,
        objc::msg_send<id>("NSNumber"_cls, "numberWithBool:"_sel, YES),
        "javaScriptCanAccessClipboard"_str);

    // Equivalent Obj-C:
    // [[config preferences] setValue:@YES forKey:@"DOMPasteAllowed"];
    objc::msg_send<id>(
        objc::msg_send<id>(config, "preferences"_sel), "setValue:forKey:"_sel,
        objc::msg_send<id>("NSNumber"_cls, "numberWithBool:"_sel, YES),
        "DOMPasteAllowed"_str);

    auto ui_delegate = create_webkit_ui_delegate();
    objc::msg_send<void>(m_webview, "initWithFrame:configuration:"_sel,
                         CGRectMake(0, 0, 0, 0), config);
    objc::msg_send<void>(m_webview, "setUIDelegate:"_sel, ui_delegate);
    auto script_message_handler = create_script_message_handler();
    objc::msg_send<void>(m_manager, "addScriptMessageHandler:name:"_sel,
                         script_message_handler, "external"_str);

    init(R""(
      window.external = {
        invoke: function(s) {
          window.webkit.messageHandlers.external.postMessage(s);
        },
      };
      )"");
    objc::msg_send<void>(m_window, "setContentView:"_sel, m_webview);
    objc::msg_send<void>(m_window, "makeKeyAndOrderFront:"_sel, nullptr);
  }
  bool m_debug;
  void *m_parent_window;
  id m_window;
  id m_webview;
  id m_manager;
};

} // namespace detail

using browser_engine = detail::cocoa_wkwebview_engine;

} // namespace webview

#elif defined(WEBVIEW_EDGE)

//
// ====================================================================
//
// This implementation uses Win32 API to create a native window. It
// uses Edge/Chromium webview2 backend as a browser engine.
//
// ====================================================================
//

#define WIN32_LEAN_AND_MEAN
#include <shlobj.h>
#include <shlwapi.h>
#include <stdlib.h>
#include <windows.h>
#include "WebView2.h"
#include "wil/com.h"
#include <map>
#include <stdio.h>
#include <string>
#include <regex>

#ifdef _MSC_VER
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "version.lib")
#endif

namespace webview {

#ifndef WINDOW_ID_PARAM_PREFIX
#define WINDOW_ID_PARAM_PREFIX "____window_id"
#endif // !WINDOW_ID_PARAM_PREFIX

typedef std::function<void()> simple_callback;
typedef std::map<int, void *>  child_window_map_t;

class webview_provider {
private:
  int window_id = -1;
  inline static cb_ext_child_window_created s_cb_ext_child_window_created = nullptr;
  inline static cb_ext_child_window_closed s_cb_ext_child_window_closed  = nullptr;
  static child_window_map_t *child_webview_map_pointer() {
    return & (webview_provider::__child_webview_engines_map);
  }

public:
  static child_window_map_t __child_webview_engines_map;
  virtual ICoreWebView2 *webview_instance() = 0;
  virtual void set_m_webview2_on_webview_first_inited(
      simple_callback webview_first_inited) = 0;
  virtual int do_init() = 0;
  virtual void after_window_opened() = 0;
  virtual void terminate() = 0;
  virtual void setup_webview_before_navigation() = 0;
  int get_window_id() { return this->window_id; }
  bool set_window_id(int value) {
    debug_logf("Trying to update MY window id %d for %p current window_id is %d\n",
               value, this, this->window_id);
    // window_id can be set only once
    if (this->window_id >= 0) {
      return false;
    }
    this->window_id = value;
    return true;
  }
  static void set_cb_ext_child_window_created(
      cb_ext_child_window_created callback) {
    s_cb_ext_child_window_created = callback;
  }
  static cb_ext_child_window_created get_cb_ext_child_window_created() {
    return s_cb_ext_child_window_created;
  }
  static void set_cb_ext_child_window_closed(
      cb_ext_child_window_closed callback) {
    s_cb_ext_child_window_closed = callback;
  }
  static cb_ext_child_window_closed get_cb_ext_child_window_closed() {
    return s_cb_ext_child_window_closed;
  }
  static bool child_window_add(int key, void *value) {
    debug_logf("Trying adding window id %d, window pointer %p\n", key, value);
    auto map_instance = child_webview_map_pointer();
    auto result = map_instance->insert(std::pair(key, value));
    auto ok = result.second;
    // need to update
    if (ok) {
      debug_logf("Added window id %d = %p\n", key, value);
    } else {
      debug_logf("Failed to insert %d=%p into map, try updating\n", key, value);
      std::map<int, void *>::iterator item = map_instance->find(key);
      if (item != map_instance->end()) {
        debug_logf("Updated window id %d = %p, total = %d\n", key, value, (int)map_instance->size());
        item->second = value;
        return true;
      }
    }
    debug_logf("Total items is %d\n", (int)map_instance->size());
    return ok;
  }

  static bool child_window_remove(int key) {
    debug_logf("Trying to remove window id %d from map\n", key);
    auto map_instance = child_webview_map_pointer();
    std::map<int, void *>::iterator item = map_instance->find(key);
    if (item != map_instance->end()) {
      auto remove_result = map_instance->erase(item);
      debug_logf("Removed window id %d from map\n", key);
    }
    debug_logf("Total items is %d\n", (int)map_instance->size());
    return true;
  }

  static void *child_window_get(int key) {
    auto map_instance = child_webview_map_pointer();
    std::map<int, void *>::iterator item = map_instance->find(key);
    if (item != map_instance->end()) {
      return item->second;
    }
    return nullptr;
  }

  static int child_window_count() {
    auto map_instance = child_webview_map_pointer();
    return (int)map_instance->size();
  }
  /*
  * count all sub windows
  * return actual windows
  */
  static int child_window_all_ids(int count, int *into) {
    auto map_instance = child_webview_map_pointer();
    std::map<int, void *>::iterator item = map_instance->begin();
    auto end = map_instance->end();
    int index = 0;
    int end_index = count - 1;
    while (item != end) {
      if (index > end_index) {
        break;
      }
      into[index] = item->first;
      index ++;
      item ++;
    }
    debug_logf("Returing child_window_all_ids, total=%d\n", index);
    return index;
  }

  static void child_window_print_all() { 
      #ifdef NDEBUG
      return;
      #endif;
      int total = child_window_count();
      int* window_ids = (int*) malloc(sizeof(int) * total);
      int filled_count = child_window_all_ids(total, window_ids);
      debug_logf("Reported %d window ids, got %d window ids\n", total, filled_count);
      for (int i = 0; i < filled_count; i++) {
        int wid = window_ids[i];
        debug_logf("Window id=%d pointer=%p\n", wid, child_window_get(wid));
      }
      free(window_ids);
  }

  static void get_window_id_params(int window_id, char *result,
                                   int result_length) {
    sprintf_s(result, result_length, WINDOW_ID_PARAM_PREFIX "=%d", window_id);
  }
  static void ensure_window_id_params(int window_id, char *url, char *result,
                                      int result_length) {
    const int window_id_param_size = 128;
    char window_id_param[window_id_param_size];
    get_window_id_params(window_id, (char *)&window_id_param,
                         window_id_param_size);
    debug_logf("window id param is %s\n", window_id_param);
    std::string url_str(url);
    int index = (int)url_str.find(window_id_param, 0);
    debug_logf("index of window_id param '%s' from url %s is %d\n",
               window_id_param, url, index);
    if (index > 0) {
      sprintf_s(result, result_length, "%s", url);
      return;
    }
    index = (int)url_str.find("?", 0);
    sprintf_s(result, result_length, "%s%s%s", url, index > 0 ? "&" : "?",
              window_id_param);
  }
  static int extract_window_id_from_url(std::string url) {
    const std::regex exp(WINDOW_ID_PARAM_PREFIX "=(\\d+)");
    std::smatch match;
    if (std::regex_search(url, match, exp)) {
      auto content = match[1];
      auto window_id_str = content.str();
      return std::stoi(window_id_str);
    }
    return -1;
  }
};
child_window_map_t webview_provider::__child_webview_engines_map = {};

typedef webview_provider *webview_window_t;
webview_window_t create_new_webview(bool debug, void *window, int is_main, int lazy);
typedef webview_window_t (*create_webview_window_handler)(bool, void *, int, int);
namespace detail {

using msg_cb_t = std::function<void(const std::string)>;

// Converts a narrow (UTF-8-encoded) string into a wide (UTF-16-encoded) string.
inline std::wstring widen_string(const std::string &input) {
  if (input.empty()) {
    return std::wstring();
  }
  UINT cp = CP_UTF8;
  DWORD flags = MB_ERR_INVALID_CHARS;
  auto input_c = input.c_str();
  auto input_length = static_cast<int>(input.size());
  auto required_length =
      MultiByteToWideChar(cp, flags, input_c, input_length, nullptr, 0);
  if (required_length > 0) {
    std::wstring output(static_cast<std::size_t>(required_length), L'\0');
    if (MultiByteToWideChar(cp, flags, input_c, input_length, &output[0],
                            required_length) > 0) {
      return output;
    }
  }
  // Failed to convert string from UTF-8 to UTF-16
  return std::wstring();
}

// Converts a wide (UTF-16-encoded) string into a narrow (UTF-8-encoded) string.
inline std::string narrow_string(const std::wstring &input) {
  if (input.empty()) {
    return std::string();
  }
  UINT cp = CP_UTF8;
  DWORD flags = WC_ERR_INVALID_CHARS;
  auto input_c = input.c_str();
  auto input_length = static_cast<int>(input.size());
  auto required_length = WideCharToMultiByte(cp, flags, input_c, input_length,
                                             nullptr, 0, nullptr, nullptr);
  if (required_length > 0) {
    std::string output(static_cast<std::size_t>(required_length), '\0');
    if (WideCharToMultiByte(cp, flags, input_c, input_length, &output[0],
                            required_length, nullptr, nullptr) > 0) {
      return output;
    }
  }
  // Failed to convert string from UTF-16 to UTF-8
  return std::string();
}

// Parses a version string with 1-4 integral components, e.g. "1.2.3.4".
// Missing or invalid components default to 0, and excess components are ignored.
template <typename T>
std::array<unsigned int, 4>
parse_version(const std::basic_string<T> &version) noexcept {
  auto parse_component = [](auto sb, auto se) -> unsigned int {
    try {
      auto n = std::stol(std::basic_string<T>(sb, se));
      return n < 0 ? 0 : n;
    } catch (std::exception &) {
      return 0;
    }
  };
  auto end = version.end();
  auto sb = version.begin(); // subrange begin
  auto se = sb;              // subrange end
  unsigned int ci = 0;       // component index
  std::array<unsigned int, 4> components{};
  while (sb != end && se != end && ci < components.size()) {
    if (*se == static_cast<T>('.')) {
      components[ci++] = parse_component(sb, se);
      sb = ++se;
      continue;
    }
    ++se;
  }
  if (sb < se && ci < components.size()) {
    components[ci] = parse_component(sb, se);
  }
  return components;
}

template <typename T, std::size_t Length>
auto parse_version(const T (&version)[Length]) noexcept {
  return parse_version(std::basic_string<T>(version, Length));
}

std::wstring get_file_version_string(const std::wstring &file_path) noexcept {
  DWORD dummy_handle; // Unused
  DWORD info_buffer_length =
      GetFileVersionInfoSizeW(file_path.c_str(), &dummy_handle);
  if (info_buffer_length == 0) {
    return std::wstring();
  }
  std::vector<char> info_buffer;
  info_buffer.reserve(info_buffer_length);
  if (!GetFileVersionInfoW(file_path.c_str(), 0, info_buffer_length,
                           info_buffer.data())) {
    return std::wstring();
  }
  auto sub_block = L"\\StringFileInfo\\040904B0\\ProductVersion";
  LPWSTR version = nullptr;
  unsigned int version_length = 0;
  if (!VerQueryValueW(info_buffer.data(), sub_block,
                      reinterpret_cast<LPVOID *>(&version), &version_length)) {
    return std::wstring();
  }
  if (!version || version_length == 0) {
    return std::wstring();
  }
  return std::wstring(version, version_length);
}

// A wrapper around COM library initialization. Calls CoInitializeEx in the
// constructor and CoUninitialize in the destructor.
class com_init_wrapper {
public:
  com_init_wrapper(DWORD dwCoInit) {
    // We can safely continue as long as COM was either successfully
    // initialized or already initialized.
    // RPC_E_CHANGED_MODE means that CoInitializeEx was already called with
    // a different concurrency model.
    switch (CoInitializeEx(nullptr, dwCoInit)) {
    case S_OK:
    case S_FALSE:
      m_initialized = true;
      break;
    }
  }

  ~com_init_wrapper() {
    if (m_initialized) {
      CoUninitialize();
      m_initialized = false;
    }
  }

  com_init_wrapper(const com_init_wrapper &other) = delete;
  com_init_wrapper &operator=(const com_init_wrapper &other) = delete;
  com_init_wrapper(com_init_wrapper &&other) = delete;
  com_init_wrapper &operator=(com_init_wrapper &&other) = delete;

  bool is_initialized() const { return m_initialized; }

private:
  bool m_initialized = false;
};

// Holds a symbol name and associated type for code clarity.
template <typename T> class library_symbol {
public:
  using type = T;

  constexpr explicit library_symbol(const char *name) : m_name(name) {}
  constexpr const char *get_name() const { return m_name; }

private:
  const char *m_name;
};

// Loads a native shared library and allows one to get addresses for those
// symbols.
class native_library {
public:
  explicit native_library(const wchar_t *name) : m_handle(LoadLibraryW(name)) {}

  ~native_library() {
    if (m_handle) {
      FreeLibrary(m_handle);
      m_handle = nullptr;
    }
  }

  native_library(const native_library &other) = delete;
  native_library &operator=(const native_library &other) = delete;
  native_library(native_library &&other) = default;
  native_library &operator=(native_library &&other) = default;

  // Returns true if the library is currently loaded; otherwise false.
  operator bool() const { return is_loaded(); }

  // Get the address for the specified symbol or nullptr if not found.
  template <typename Symbol>
  typename Symbol::type get(const Symbol &symbol) const {
    if (is_loaded()) {
      return reinterpret_cast<typename Symbol::type>(
          GetProcAddress(m_handle, symbol.get_name()));
    }
    return nullptr;
  }

  // Returns true if the library is currently loaded; otherwise false.
  bool is_loaded() const { return !!m_handle; }

  void detach() { m_handle = nullptr; }

private:
  HMODULE m_handle = nullptr;
};

struct user32_symbols {
  using DPI_AWARENESS_CONTEXT = HANDLE;
  using SetProcessDpiAwarenessContext_t = BOOL(WINAPI *)(DPI_AWARENESS_CONTEXT);
  using SetProcessDPIAware_t = BOOL(WINAPI *)();

  static constexpr auto SetProcessDpiAwarenessContext =
      library_symbol<SetProcessDpiAwarenessContext_t>(
          "SetProcessDpiAwarenessContext");
  static constexpr auto SetProcessDPIAware =
      library_symbol<SetProcessDPIAware_t>("SetProcessDPIAware");
};

struct shcore_symbols {
  typedef enum { PROCESS_PER_MONITOR_DPI_AWARE = 2 } PROCESS_DPI_AWARENESS;
  using SetProcessDpiAwareness_t = HRESULT(WINAPI *)(PROCESS_DPI_AWARENESS);

  static constexpr auto SetProcessDpiAwareness =
      library_symbol<SetProcessDpiAwareness_t>("SetProcessDpiAwareness");
};

class reg_key {
public:
  explicit reg_key(HKEY root_key, const wchar_t *sub_key, DWORD options,
                   REGSAM sam_desired) {
    HKEY handle;
    auto status =
        RegOpenKeyExW(root_key, sub_key, options, sam_desired, &handle);
    if (status == ERROR_SUCCESS) {
      m_handle = handle;
    }
  }

  explicit reg_key(HKEY root_key, const std::wstring &sub_key, DWORD options,
                   REGSAM sam_desired)
      : reg_key(root_key, sub_key.c_str(), options, sam_desired) {}

  virtual ~reg_key() {
    if (m_handle) {
      RegCloseKey(m_handle);
      m_handle = nullptr;
    }
  }

  reg_key(const reg_key &other) = delete;
  reg_key &operator=(const reg_key &other) = delete;
  reg_key(reg_key &&other) = delete;
  reg_key &operator=(reg_key &&other) = delete;

  bool is_open() const { return !!m_handle; }
  bool get_handle() const { return m_handle; }

  std::wstring query_string(const wchar_t *name) const {
    DWORD buf_length = 0;
    // Get the size of the data in bytes.
    auto status = RegQueryValueExW(m_handle, name, nullptr, nullptr, nullptr,
                                   &buf_length);
    if (status != ERROR_SUCCESS && status != ERROR_MORE_DATA) {
      return std::wstring();
    }
    // Read the data.
    std::wstring result(buf_length / sizeof(wchar_t), 0);
    auto buf = reinterpret_cast<LPBYTE>(&result[0]);
    status =
        RegQueryValueExW(m_handle, name, nullptr, nullptr, buf, &buf_length);
    if (status != ERROR_SUCCESS) {
      return std::wstring();
    }
    // Remove trailing null-characters.
    for (std::size_t length = result.size(); length > 0; --length) {
      if (result[length - 1] != 0) {
        result.resize(length);
        break;
      }
    }
    return result;
  }

private:
  HKEY m_handle = nullptr;
};

inline bool enable_dpi_awareness() {
  auto user32 = native_library(L"user32.dll");
  if (auto fn = user32.get(user32_symbols::SetProcessDpiAwarenessContext)) {
    if (fn(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE)) {
      return true;
    }
    return GetLastError() == ERROR_ACCESS_DENIED;
  }
  if (auto shcore = native_library(L"shcore.dll")) {
    if (auto fn = shcore.get(shcore_symbols::SetProcessDpiAwareness)) {
      auto result = fn(shcore_symbols::PROCESS_PER_MONITOR_DPI_AWARE);
      return result == S_OK || result == E_ACCESSDENIED;
    }
  }
  if (auto fn = user32.get(user32_symbols::SetProcessDPIAware)) {
    return !!fn();
  }
  return true;
}

// Enable built-in WebView2Loader implementation by default.
#ifndef WEBVIEW_MSWEBVIEW2_BUILTIN_IMPL
#define WEBVIEW_MSWEBVIEW2_BUILTIN_IMPL 1
#endif

// Link WebView2Loader.dll explicitly by default only if the built-in
// implementation is enabled.
#ifndef WEBVIEW_MSWEBVIEW2_EXPLICIT_LINK
#define WEBVIEW_MSWEBVIEW2_EXPLICIT_LINK WEBVIEW_MSWEBVIEW2_BUILTIN_IMPL
#endif

// Explicit linking of WebView2Loader.dll should be used along with
// the built-in implementation.
#if WEBVIEW_MSWEBVIEW2_BUILTIN_IMPL == 1 &&                                    \
    WEBVIEW_MSWEBVIEW2_EXPLICIT_LINK != 1
#undef WEBVIEW_MSWEBVIEW2_EXPLICIT_LINK
#error Please set WEBVIEW_MSWEBVIEW2_EXPLICIT_LINK=1.
#endif

#if WEBVIEW_MSWEBVIEW2_BUILTIN_IMPL == 1
// Gets the last component of a Windows native file path.
// For example, if the path is "C:\a\b" then the result is "b".
template <typename T>
std::basic_string<T>
get_last_native_path_component(const std::basic_string<T> &path) {
  if (auto pos = path.find_last_of(static_cast<T>('\\'));
      pos != std::basic_string<T>::npos) {
    return path.substr(pos + 1);
  }
  return std::basic_string<T>();
}
#endif /* WEBVIEW_MSWEBVIEW2_BUILTIN_IMPL */

template <typename T> struct cast_info_t {
  using type = T;
  IID iid;
};

namespace mswebview2 {
static constexpr IID
    IID_ICoreWebView2CreateCoreWebView2ControllerCompletedHandler{
        0x6C4819F3, 0xC9B7, 0x4260, 0x81, 0x27, 0xC9,
        0xF5,       0xBD,   0xE7,   0xF6, 0x8C};
static constexpr IID
    IID_ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler{
        0x4E8A3389, 0xC9D8, 0x4BD2, 0xB6, 0xB5, 0x12,
        0x4F,       0xEE,   0x6C,   0xC1, 0x4D};
static constexpr IID IID_ICoreWebView2PermissionRequestedEventHandler{
    0x15E1C6A3, 0xC72A, 0x4DF3, 0x91, 0xD7, 0xD0, 0x97, 0xFB, 0xEC, 0x6B, 0xFD};
static constexpr IID IID_ICoreWebView2WebMessageReceivedEventHandler{
    0x57213F19, 0x00E6, 0x49FA, 0x8E, 0x07, 0x89, 0x8E, 0xA0, 0x1E, 0xCB, 0xD2};

#if WEBVIEW_MSWEBVIEW2_BUILTIN_IMPL == 1
enum class webview2_runtime_type { installed = 0, embedded = 1 };

namespace webview2_symbols {
using CreateWebViewEnvironmentWithOptionsInternal_t =
    HRESULT(STDMETHODCALLTYPE *)(
        bool, webview2_runtime_type, PCWSTR, IUnknown *,
        ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *);
using DllCanUnloadNow_t = HRESULT(STDMETHODCALLTYPE *)();

static constexpr auto CreateWebViewEnvironmentWithOptionsInternal =
    library_symbol<CreateWebViewEnvironmentWithOptionsInternal_t>(
        "CreateWebViewEnvironmentWithOptionsInternal");
static constexpr auto DllCanUnloadNow =
    library_symbol<DllCanUnloadNow_t>("DllCanUnloadNow");
} // namespace webview2_symbols
#endif /* WEBVIEW_MSWEBVIEW2_BUILTIN_IMPL */

#if WEBVIEW_MSWEBVIEW2_EXPLICIT_LINK == 1
namespace webview2_symbols {
using CreateCoreWebView2EnvironmentWithOptions_t = HRESULT(STDMETHODCALLTYPE *)(
    PCWSTR, PCWSTR, ICoreWebView2EnvironmentOptions *,
    ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *);
using GetAvailableCoreWebView2BrowserVersionString_t =
    HRESULT(STDMETHODCALLTYPE *)(PCWSTR, LPWSTR *);

static constexpr auto CreateCoreWebView2EnvironmentWithOptions =
    library_symbol<CreateCoreWebView2EnvironmentWithOptions_t>(
        "CreateCoreWebView2EnvironmentWithOptions");
static constexpr auto GetAvailableCoreWebView2BrowserVersionString =
    library_symbol<GetAvailableCoreWebView2BrowserVersionString_t>(
        "GetAvailableCoreWebView2BrowserVersionString");
} // namespace webview2_symbols
#endif /* WEBVIEW_MSWEBVIEW2_EXPLICIT_LINK */

class loader {
public:
  HRESULT create_environment_with_options(
      PCWSTR browser_dir, PCWSTR user_data_dir,
      ICoreWebView2EnvironmentOptions *env_options,
      ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler
          *created_handler) const {
#if WEBVIEW_MSWEBVIEW2_EXPLICIT_LINK == 1
    if (m_lib.is_loaded()) {
      if (auto fn = m_lib.get(
              webview2_symbols::CreateCoreWebView2EnvironmentWithOptions)) {
        return fn(browser_dir, user_data_dir, env_options, created_handler);
      }
    }
#if WEBVIEW_MSWEBVIEW2_BUILTIN_IMPL == 1
    return create_environment_with_options_impl(browser_dir, user_data_dir,
                                                env_options, created_handler);
#else
    return S_FALSE;
#endif
#else
    return ::CreateCoreWebView2EnvironmentWithOptions(
        browser_dir, user_data_dir, env_options, created_handler);
#endif /* WEBVIEW_MSWEBVIEW2_EXPLICIT_LINK */
  }

  HRESULT
  get_available_browser_version_string(PCWSTR browser_dir,
                                       LPWSTR *version) const {
#if WEBVIEW_MSWEBVIEW2_EXPLICIT_LINK == 1
    if (m_lib.is_loaded()) {
      if (auto fn = m_lib.get(
              webview2_symbols::GetAvailableCoreWebView2BrowserVersionString)) {
        return fn(browser_dir, version);
      }
    }
#if WEBVIEW_MSWEBVIEW2_BUILTIN_IMPL == 1
    return get_available_browser_version_string_impl(browser_dir, version);
#else
    return S_FALSE;
#endif
#else
    return ::GetAvailableCoreWebView2BrowserVersionString(browser_dir, version);
#endif /* WEBVIEW_MSWEBVIEW2_EXPLICIT_LINK */
  }

private:
#if WEBVIEW_MSWEBVIEW2_BUILTIN_IMPL == 1
  struct client_info_t {
    bool found = false;
    std::wstring dll_path;
    std::wstring version;
    webview2_runtime_type runtime_type;
  };

  HRESULT create_environment_with_options_impl(
      PCWSTR browser_dir, PCWSTR user_data_dir,
      ICoreWebView2EnvironmentOptions *env_options,
      ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler
          *created_handler) const {
    auto found_client = find_available_client(browser_dir);
    if (!found_client.found) {
      return -1;
    }
    auto client_dll = native_library(found_client.dll_path.c_str());
    if (auto fn = client_dll.get(
            webview2_symbols::CreateWebViewEnvironmentWithOptionsInternal)) {
      return fn(true, found_client.runtime_type, user_data_dir, env_options,
                created_handler);
    }
    if (auto fn = client_dll.get(webview2_symbols::DllCanUnloadNow)) {
      if (!fn()) {
        client_dll.detach();
      }
    }
    return ERROR_SUCCESS;
  }

  HRESULT
  get_available_browser_version_string_impl(PCWSTR browser_dir,
                                            LPWSTR *version) const {
    if (!version) {
      return -1;
    }
    auto found_client = find_available_client(browser_dir);
    if (!found_client.found) {
      return -1;
    }
    auto info_length_bytes =
        found_client.version.size() * sizeof(found_client.version[0]);
    auto info = static_cast<LPWSTR>(CoTaskMemAlloc(info_length_bytes));
    if (!info) {
      return -1;
    }
    CopyMemory(info, found_client.version.c_str(), info_length_bytes);
    *version = info;
    return 0;
  }

  client_info_t find_available_client(PCWSTR browser_dir) const {
    if (browser_dir) {
      return find_embedded_client(api_version, browser_dir);
    }
    auto found_client =
        find_installed_client(api_version, true, default_release_channel_guid);
    if (!found_client.found) {
      found_client = find_installed_client(api_version, false,
                                           default_release_channel_guid);
    }
    return found_client;
  }

  std::wstring make_client_dll_path(const std::wstring &dir) const {
    auto dll_path = dir;
    if (!dll_path.empty()) {
      auto last_char = dir[dir.size() - 1];
      if (last_char != L'\\' && last_char != L'/') {
        dll_path += L'\\';
      }
    }
    dll_path += L"EBWebView\\";
#if defined(_M_X64) || defined(__x86_64__)
    dll_path += L"x64";
#elif defined(_M_IX86) || defined(__i386__)
    dll_path += L"x86";
#elif defined(_M_ARM64) || defined(__aarch64__)
    dll_path += L"arm64";
#else
#error WebView2 integration for this platform is not yet supported.
#endif
    dll_path += L"\\EmbeddedBrowserWebView.dll";
    return dll_path;
  }

  client_info_t
  find_installed_client(unsigned int min_api_version, bool system,
                        const std::wstring &release_channel) const {
    std::wstring sub_key = client_state_reg_sub_key;
    sub_key += release_channel;
    auto root_key = system ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
    reg_key key(root_key, sub_key, 0, KEY_READ | KEY_WOW64_32KEY);
    if (!key.is_open()) {
      return {};
    }
    auto ebwebview_value = key.query_string(L"EBWebView");

    auto client_version_string =
        get_last_native_path_component(ebwebview_value);
    auto client_version = parse_version(client_version_string);
    if (client_version[2] < min_api_version) {
      // Our API version is greater than the runtime API version.
      return {};
    }

    auto client_dll_path = make_client_dll_path(ebwebview_value);
    return {true, client_dll_path, client_version_string,
            webview2_runtime_type::installed};
  }

  client_info_t find_embedded_client(unsigned int min_api_version,
                                     const std::wstring &dir) const {
    auto client_dll_path = make_client_dll_path(dir);

    auto client_version_string = get_file_version_string(client_dll_path);
    auto client_version = parse_version(client_version_string);
    if (client_version[2] < min_api_version) {
      // Our API version is greater than the runtime API version.
      return {};
    }

    return {true, client_dll_path, client_version_string,
            webview2_runtime_type::embedded};
  }

  // The minimum WebView2 API version we need regardless of the SDK release
  // actually used. The number comes from the SDK release version,
  // e.g. 1.0.1150.38. To be safe the SDK should have a number that is greater
  // than or equal to this number. The Edge browser webview client must
  // have a number greater than or equal to this number.
  static constexpr unsigned int api_version = 1150;

  static constexpr auto client_state_reg_sub_key =
      L"SOFTWARE\\Microsoft\\EdgeUpdate\\ClientState\\";

  // GUID for the stable release channel.
  static constexpr auto stable_release_guid =
      L"{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}";

  static constexpr auto default_release_channel_guid = stable_release_guid;
#endif /* WEBVIEW_MSWEBVIEW2_BUILTIN_IMPL */

#if WEBVIEW_MSWEBVIEW2_EXPLICIT_LINK == 1
  native_library m_lib{L"WebView2Loader.dll"};
#endif
};

namespace cast_info {
static constexpr auto controller_completed =
    cast_info_t<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>{
        IID_ICoreWebView2CreateCoreWebView2ControllerCompletedHandler};

static constexpr auto environment_completed =
    cast_info_t<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>{
        IID_ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler};

static constexpr auto message_received =
    cast_info_t<ICoreWebView2WebMessageReceivedEventHandler>{
        IID_ICoreWebView2WebMessageReceivedEventHandler};

static constexpr auto permission_requested =
    cast_info_t<ICoreWebView2PermissionRequestedEventHandler>{
        IID_ICoreWebView2PermissionRequestedEventHandler};
} // namespace cast_info
} // namespace mswebview2

// forward definitions
class win32_edge_engine;
class webview2_com_handler
    : public ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler,
      public ICoreWebView2CreateCoreWebView2ControllerCompletedHandler,
      public ICoreWebView2WebMessageReceivedEventHandler,
      public ICoreWebView2PermissionRequestedEventHandler,
      public ICoreWebView2NewWindowRequestedEventHandler,
      public ICoreWebView2NavigationCompletedEventHandler {
  using webview2_com_handler_cb_t =
      std::function<void(ICoreWebView2Controller *, ICoreWebView2 *webview)>;

public:
  webview2_com_handler(HWND hwnd, msg_cb_t msgCb, webview2_com_handler_cb_t cb,
                       create_webview_window_handler create_window_handler,
                       bool debug, simple_callback webview_inited)
      : m_window(hwnd), m_msgCb(msgCb), m_cb(cb),
        create_window_handler(create_window_handler), debug_mode(debug),
        m_webview2_on_webview_first_inited(webview_inited){}

  virtual ~webview2_com_handler() = default;
  webview2_com_handler(const webview2_com_handler &other) = delete;
  webview2_com_handler &operator=(const webview2_com_handler &other) = delete;
  webview2_com_handler(webview2_com_handler &&other) = delete;
  webview2_com_handler &operator=(webview2_com_handler &&other) = delete;

  ULONG STDMETHODCALLTYPE AddRef() { 
    return ++m_ref_count; 
  }
  ULONG STDMETHODCALLTYPE Release() {
    if (m_ref_count > 1) {
      return --m_ref_count;
    }
    delete this;
    return 0;
  }
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, LPVOID *ppv) {
    using namespace mswebview2::cast_info;

    if (!ppv) {
      return E_POINTER;
    }

    // All of the COM interfaces we implement should be added here regardless
    // of whether they are required.
    // This is just to be on the safe side in case the WebView2 Runtime ever
    // requests a pointer to an interface we implement.
    // The WebView2 Runtime must at the very least be able to get a pointer to
    // ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler when we use
    // our custom WebView2 loader implementation, and observations have shown
    // that it is the only interface requested in this case. None have been
    // observed to be requested when using the official WebView2 loader.

    if (cast_if_equal_iid(riid, controller_completed, ppv) ||
        cast_if_equal_iid(riid, environment_completed, ppv) ||
        cast_if_equal_iid(riid, message_received, ppv) ||
        cast_if_equal_iid(riid, permission_requested, ppv)) {
      return S_OK;
    }

    return E_NOINTERFACE;
  }
  HRESULT STDMETHODCALLTYPE Invoke(HRESULT res, ICoreWebView2Environment *env) {
    if (SUCCEEDED(res)) {
      res = env->CreateCoreWebView2Controller(m_window, this);
      if (SUCCEEDED(res)) {
        return S_OK;
      }
    }
    try_create_environment();
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE Invoke(HRESULT res,
                                   ICoreWebView2Controller *controller) {
    if (FAILED(res)) {
      // See try_create_environment() regarding
      // HRESULT_FROM_WIN32(ERROR_INVALID_STATE).
      // The result is E_ABORT if the parent window has been destroyed already.
      switch (res) {
      case HRESULT_FROM_WIN32(ERROR_INVALID_STATE):
      case E_ABORT:
        return S_OK;
      }
      try_create_environment();
      return S_OK;
    }

    ICoreWebView2 *webview;
    ::EventRegistrationToken token;
    controller->get_CoreWebView2(&webview);
    webview->add_WebMessageReceived(this, &token);
    webview->add_PermissionRequested(this, &token);
    m_cb(controller, webview);
    // callback for m_webview2_on_webview_first_inited
    if (this->m_webview2_on_webview_first_inited) {
      debug_logf("Invoking m_webview2_on_webview_first_inited\n");
      this->m_webview2_on_webview_first_inited();
    }
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE Invoke(
      ICoreWebView2 *sender, ICoreWebView2WebMessageReceivedEventArgs *args) {
    LPWSTR message;
    args->TryGetWebMessageAsString(&message);
    m_msgCb(narrow_string(message));
    sender->PostWebMessageAsString(message);

    CoTaskMemFree(message);
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE Invoke(
      ICoreWebView2 *sender, ICoreWebView2PermissionRequestedEventArgs *args) {
    COREWEBVIEW2_PERMISSION_KIND kind;
    args->get_PermissionKind(&kind);
    if (kind == COREWEBVIEW2_PERMISSION_KIND_CLIPBOARD_READ) {
      args->put_State(COREWEBVIEW2_PERMISSION_STATE_ALLOW);
    }
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2 *sender, ICoreWebView2NavigationCompletedEventArgs *args) {
    /*
    std::string script = "console.info('hello ', new Date());";
    size_t script_lenth = script.length();
    wchar_t *buffer = new wchar_t[script_lenth + 1];
    MultiByteToWideChar(CP_ACP, 0, script.c_str(), script_lenth, buffer,
                        script_lenth * sizeof(wchar_t));
    HRESULT result = sender->ExecuteScript(buffer, nullptr);
    debug_logf("ICoreWebView2NavigationCompletedEventHandler Execute script  %s ok ? %d webview is %p\n", script.c_str(), SUCCEEDED(result) ? 1:0, sender);
    delete buffer;
    */
    this->m_window_pointer->after_window_opened();
    return S_OK;
  }
  // new windows requested handler
  HRESULT STDMETHODCALLTYPE Invoke(
      ICoreWebView2 *sender, ICoreWebView2NewWindowRequestedEventArgs *args) {
    webview_provider *window_pointer = nullptr;
    wil::com_ptr<ICoreWebView2WindowFeatures> windowFeatures;
    wil::com_ptr<ICoreWebView2Deferral> deferral;
    HRESULT hr_result;
    debug_logf("******ICoreWebView2NewWindowRequestedEventHandler Invoke started\n");
    hr_result = args->GetDeferral(&deferral);
    if (!SUCCEEDED(hr_result)) {
        debug_logf("Failed to call args->GetDeferral\n");
        return hr_result;
    }
    hr_result = args->get_WindowFeatures(&windowFeatures);
    if (!SUCCEEDED(hr_result)) {
        debug_logf("Failed to call args->get_WindowFeatures\n");
        return hr_result;
    }
    // create main instance first
    window_pointer = this->create_window_handler(this->debug_mode ? 1 : 0, nullptr, 0, 1);
    m_window_pointer = window_pointer;
    debug_logf("Got window pointer %p\n", window_pointer);
    simple_callback callback = [args, deferral, window_pointer, this]() {
      LPWSTR target_url;
      HRESULT hr_result;
      debug_logf("Trying to get uri\n");
      hr_result = args->get_Uri(&target_url);
      char logging_content[1024];
      sprintf_s((char *)&logging_content, 1024,
                "Try navigating to %ws succeed = %d\n", target_url,
                SUCCEEDED(hr_result) ? 1 : 0);
      if(!SUCCEEDED(hr_result)) {
        args->put_Handled(FALSE);
        return;
      }
      debug_logf("Navigation::%s\n", (char *)logging_content);
      // call pre init handler
      const int url_buffer_size = 1024;
      char url_buffer[url_buffer_size];
      sprintf_s(url_buffer, url_buffer_size, "%ws", target_url);
      auto window_id = webview_provider::extract_window_id_from_url(std::string((char *)&url_buffer));
      debug_logf("Window id is %d for url %ws \n", window_id, target_url);
      window_pointer->set_window_id(window_id);

      ICoreWebView2 *created_view = window_pointer->webview_instance();
      // must setup the webview, or the bindings will not work
      window_pointer->setup_webview_before_navigation();
      created_view->add_NavigationCompleted(this, nullptr);

      if (this->m_webview2_on_webview_first_inited) {
        debug_logf("Invoking m_webview2_on_webview_first_inited\n");
        this->m_webview2_on_webview_first_inited();
      }

      hr_result = created_view->Navigate(target_url);
      debug_logf("Requested navigation to %ws, succeed = %d \n", target_url,
                 SUCCEEDED(hr_result) ? 1 : 0);

      args->put_NewWindow(created_view);
      args->put_Handled(TRUE);
      deferral->Complete();
      
      webview_provider::child_window_add(window_id, window_pointer);
      webview_provider::child_window_print_all();
      
      debug_logf("Invoking this->m_after_window_opened \n");
      window_pointer->after_window_opened();
    }; 
    debug_logf("Setting up webview2_on_webview_first_inited callback handler \n");
    window_pointer->set_m_webview2_on_webview_first_inited(callback);
    debug_logf("Trying to init the webview: window_pointer->do_init \n");
    window_pointer->do_init();
    debug_logf("window_pointer->do_init called \n");
    return S_OK;
  }

  // Checks whether the specified IID equals the IID of the specified type and
  // if so casts the "this" pointer to T and returns it. Returns nullptr on
  // mismatching IIDs.
  // If ppv is specified then the pointer will also be assigned to *ppv.
  template <typename T>
  T *cast_if_equal_iid(REFIID riid, const cast_info_t<T> &info,
                       LPVOID *ppv = nullptr) noexcept {
    T *ptr = nullptr;
    if (IsEqualIID(riid, info.iid)) {
      ptr = static_cast<T *>(this);
      ptr->AddRef();
    }
    if (ppv) {
      *ppv = ptr;
    }
    return ptr;
  }

  // Set the function that will perform the initiating logic for creating
  // the WebView2 environment.
  void set_attempt_handler(std::function<HRESULT()> attempt_handler) noexcept {
    m_attempt_handler = attempt_handler;
  }

  // Retry creating a WebView2 environment.
  // The initiating logic for creating the environment is defined by the
  // caller of set_attempt_handler().
  void try_create_environment() noexcept {
    // WebView creation fails with HRESULT_FROM_WIN32(ERROR_INVALID_STATE) if
    // a running instance using the same user data folder exists, and the
    // Environment objects have different EnvironmentOptions.
    // Source: https://docs.microsoft.com/en-us/microsoft-edge/webview2/reference/win32/icorewebview2environment?view=webview2-1.0.1150.38
    if (m_attempts < m_max_attempts) {
      ++m_attempts;
      auto res = m_attempt_handler();
      if (SUCCEEDED(res)) {
        return;
      }
      // Not entirely sure if this error code only applies to
      // CreateCoreWebView2Controller so we check here as well.
      if (res == HRESULT_FROM_WIN32(ERROR_INVALID_STATE)) {
        return;
      }
      try_create_environment();
      return;
    }
    // Give up.
    m_cb(nullptr, nullptr);
  }

private:
  HWND m_window;
  msg_cb_t m_msgCb;
  webview2_com_handler_cb_t m_cb;
  std::atomic<ULONG> m_ref_count{1};
  std::function<HRESULT()> m_attempt_handler;
  unsigned int m_max_attempts = 5;
  unsigned int m_attempts = 0;
  bool debug_mode;
  create_webview_window_handler create_window_handler;
  simple_callback m_webview2_on_webview_first_inited;
  webview_provider *m_window_pointer = nullptr;
};

class win32_edge_engine: public webview_provider {
public:
  win32_edge_engine(bool debug, void *window, int is_main = 1, int lazy = 0)
      : main_window_flag(is_main), debug_flag(debug ? 1 : 0) {
      debug_logf("win32_edge_engine constructor entry\n");
    if (!is_webview2_available()) {
      return;
    }
    if (!m_com_init.is_initialized()) {
      return;
    
    }
    debug_logf("win32_edge_engine::win32_edge_engine is main window = input arg = "
            "%d, saved value = %d \n", is_main, this->main_window_flag);
    enable_dpi_awareness();
    if (window == nullptr) {
      HINSTANCE hInstance = GetModuleHandle(nullptr);
      HICON icon = (HICON)LoadImage(
          hInstance, IDI_APPLICATION, IMAGE_ICON, GetSystemMetrics(SM_CXICON),
          GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR);

      WNDCLASSEXW wc;
      ZeroMemory(&wc, sizeof(WNDCLASSEX));
      wc.cbSize = sizeof(WNDCLASSEX);
      wc.hInstance = hInstance;
      wc.lpszClassName = L"webview";
      wc.hIcon = icon;
      wc.lpfnWndProc =
          (WNDPROC)(+[](HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) -> LRESULT {
            auto w = (win32_edge_engine *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
            switch (msg) {
            case WM_SIZE:
              w->resize(hwnd);
              break;
            case WM_CLOSE:
              DestroyWindow(hwnd);
              break;
            case WM_DESTROY:
              w->terminate();
              break;
            case WM_GETMINMAXINFO: {
              auto lpmmi = (LPMINMAXINFO)lp;
              if (w == nullptr) {
                return 0;
              }
              if (w->m_maxsz.x > 0 && w->m_maxsz.y > 0) {
                lpmmi->ptMaxSize = w->m_maxsz;
                lpmmi->ptMaxTrackSize = w->m_maxsz;
              }
              if (w->m_minsz.x > 0 && w->m_minsz.y > 0) {
                lpmmi->ptMinTrackSize = w->m_minsz;
              }
            } break;
            default:
              return DefWindowProcW(hwnd, msg, wp, lp);
            }
            return 0;
          });
      RegisterClassExW(&wc);
      m_window = CreateWindowW(L"webview", L"", WS_OVERLAPPEDWINDOW,
                               CW_USEDEFAULT, CW_USEDEFAULT, 640, 480, nullptr,
                               nullptr, hInstance, nullptr);
      debug_logf("Created new window %d lazy is %d \n", m_window, lazy);
      if (m_window == nullptr) {
        return;
      }
      SetWindowLongPtr(m_window, GWLP_USERDATA, (LONG_PTR)this);
    } else {
      m_window = *(static_cast<HWND *>(window));
    }
    // main window will not do lazy
    if (!lazy) {
        this->do_init();
    }
  }

  virtual ~win32_edge_engine() {
    keep_running_event_thread = 0;
    if (m_com_handler) {
      m_com_handler->Release();
      m_com_handler = nullptr;
    }
    if (m_webview) {
      m_webview->Release();
      m_webview = nullptr;
    }
    if (m_controller) {
      m_controller->Release();
      m_controller = nullptr;
    }
  }

  win32_edge_engine(const win32_edge_engine &other) = delete;
  win32_edge_engine &operator=(const win32_edge_engine &other) = delete;
  win32_edge_engine(win32_edge_engine &&other) = delete;
  win32_edge_engine &operator=(win32_edge_engine &&other) = delete;

  void run() {
    MSG msg;
    BOOL res;
    while ((res = GetMessage(&msg, nullptr, 0, 0)) != -1) {
      if (msg.hwnd) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        continue;
      }
      if (msg.message == WM_APP) {
        auto f = (dispatch_fn_t *)(msg.lParam);
        (*f)();
        delete f;
      } else if (msg.message == WM_QUIT && this->need_quit()) {
        printf("Need to quit\n");
        return;
      }
    }
  }
  void *window() { return (void *)m_window; }

  void terminate() { 
    debug_logf("Sending quit message ? %d \n", this->main_window_flag);
    if (this->main_window_flag) {
      //close all sub windows
      int sub_window_count = webview_provider::child_window_count();
      if (sub_window_count) {
        int *window_id_array = (int *)malloc(sizeof(int) * sub_window_count);
        int actual_window_count = webview_provider::child_window_all_ids(sub_window_count, window_id_array);
        debug_logf("There're %d child windows, %d retrieve %d actually\n", sub_window_count, actual_window_count);
        for (int i = 0; i < sub_window_count; i++) {
            int wid = window_id_array[i];
            void *wptr = webview_provider::child_window_get(wid);
            if (wptr == nullptr) {
            debug_logf("Bad child window %d, no window pointer\n", wid);
                continue;
            }
            debug_logf("Terminate child window with id=%d pointer=%p\n", wid, wptr);

            ((webview_provider*) wptr) -> terminate();
        }
        free(window_id_array);
      }
      PostQuitMessage(0);
    }
    else {
      webview_provider::child_window_remove(this->get_window_id());
      webview_provider::child_window_print_all();
#ifdef WEBVIEW_CLOSE_MAIN_WINDOW_WHEN_NO_CHILD_WINDOWS
      int windows_left = webview_provider::child_window_count();
      if (!windows_left) {
        debug_logf("No child windows left, quiting...\n");
        PostQuitMessage(0);
      }
#endif // !WEBVIEW_KEEP_MAIN_WINDOW
      auto callback = webview_provider::get_cb_ext_child_window_closed();
      if(callback) {
        callback(this->get_window_id());
      }
      delete this;
    }
  }

  void dispatch(dispatch_fn_t f) {
    PostThreadMessage(m_main_thread, WM_APP, 0, (LPARAM) new dispatch_fn_t(f));
  }

  void set_title(const std::string &title) {
    SetWindowTextW(m_window, widen_string(title).c_str());
  }

  void set_size(int width, int height, int hints) {
    this->set_size(0, 0, width, height, hints);
  }

  void set_size(int x, int y, int width, int height, int hints) {
    auto style = GetWindowLong(m_window, GWL_STYLE);
    if (hints == WEBVIEW_HINT_FIXED) {
      style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
    } else {
      style |= (WS_THICKFRAME | WS_MAXIMIZEBOX);
    }
    SetWindowLong(m_window, GWL_STYLE, style);
    if (hints == WEBVIEW_HINT_MAX) {
      m_maxsz.x = width;
      m_maxsz.y = height;
    } else if (hints == WEBVIEW_HINT_MIN) {
      m_minsz.x = width;
      m_minsz.y = height;
    } else {
      RECT r;
      r.left = x;
      r.top = y;
      r.right = width;
      r.bottom = height;
      AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, 0);
      SetWindowPos(
          m_window, nullptr, r.left, r.top, r.right - r.left, r.bottom - r.top,
          SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOMOVE | SWP_FRAMECHANGED);
      resize(m_window);
    }
  }

  void navigate(const std::string &url) {
    auto wurl = widen_string(url);
    m_webview->Navigate(wurl.c_str());
  }

  void navigate_popup(const std::string &url, int popup_window_id) {
    const int buffer_size = 2048;
    const int url_buffer_size = 1024;
    char script[buffer_size];
    char url_buffer[url_buffer_size];
    webview_provider::ensure_window_id_params(
        popup_window_id, (char *)url.c_str(), (char *)&url_buffer,
        url_buffer_size);
    // todo: add window_id to the end of the url string
    sprintf_s((char *const)&script, buffer_size, "window.open('%s', '_blank');", url_buffer);
    debug_logf("Trying to open popup window %s\n", script);
    auto final_script = widen_string(script);
    // record a empty pointer first
    auto added = webview_provider::child_window_add(popup_window_id, nullptr);
    debug_logf("Added window %d with null value ok ? %d \n", popup_window_id,
               added ? 1 : 0);
    webview_provider::child_window_print_all();
    m_webview->ExecuteScript(final_script.c_str(), nullptr);
  }

  void init(const std::string &js) {
    auto wjs = widen_string(js);
    HRESULT result = m_webview->AddScriptToExecuteOnDocumentCreated(wjs.c_str(), nullptr);
    bool ok = SUCCEEDED(result);
    debug_logf("ok = %d for calling init [webview=%p] %s\n", ok ? 1 : 0, this->m_webview, js.c_str());
  }

  void eval(const std::string &js) {
    auto wjs = widen_string(js);
    HRESULT result = m_webview->ExecuteScript(wjs.c_str(), nullptr);
    bool ok = SUCCEEDED(result);
    debug_logf("ok = %d for calling eval [webview=%p] %s\n", ok ? 1 : 0, this->m_webview, js.c_str());
  }

  void set_html(const std::string &html) {
    m_webview->NavigateToString(widen_string(html).c_str());
  }
  ICoreWebView2* webview_instance() { return this->m_webview;}
  void set_m_webview2_on_webview_first_inited(simple_callback webview_first_inited) {
    this->m_webview2_on_webview_first_inited = webview_first_inited;
  }
  void after_window_opened() { 
    auto ext_callback = webview_provider::get_cb_ext_child_window_created();
    debug_logf("child window opened, trying to invoke callback %p webview=%p \n", ext_callback, this->m_webview);
    if (ext_callback) {
      ext_callback(this->get_window_id(), this);
    }
  }
  /*
  * Let external code to do some setup
  */
  int do_init() {
    debug_logf("do_init ShowWindow\n");
    ShowWindow(m_window, SW_SHOW);
    debug_logf("do_init UpdateWindow\n");
    UpdateWindow(m_window);
    debug_logf("do_init SetFocus\n");
    SetFocus(m_window);
    
    debug_logf("do_init std::bind\n");
    auto cb =
        std::bind(&win32_edge_engine::on_message, this, std::placeholders::_1);

    debug_logf("do_init embed\n");
    embed(m_window, debug_flag, cb);
    debug_logf("do_init resize\n");
    resize(m_window);
    if (this->main_window_flag) {
      debug_logf("do_init m_controller->MoveFocus\n");
      m_controller->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
    }
    debug_logf("do_init  end\n");
    return 0;
  }
  void event_thread_method(std::atomic_flag *flag, unsigned int break_for_msg = 0) {
    MSG msg = {};
    std::atomic_flag local_flag = ATOMIC_FLAG_INIT;
    if (flag == nullptr) {
        flag = &local_flag;
    }
    while (flag->test_and_set() && GetMessage(&msg, nullptr, 0, 0) && this->keep_running_event_thread) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
      unsigned int msg_id = (unsigned int) msg.message;
      if (break_for_msg > 0 && break_for_msg == msg_id) {
          debug_logf("break for msg %d\n", break_for_msg);
          break;
      }
    }
    debug_logf("event_thread_method stoped for %p \n", this);
  }
  void setup_webview_before_navigation() {
    debug_logf("int('window.external={invoke:s=>window.chrome.webview."
               "postMessage(s)}');\n");
    init("window.external={invoke:s=>window.chrome.webview.postMessage(s)}");
    this->embed_latter_job_called = true;
  }
 private:
  bool embed(HWND wnd, bool debug, msg_cb_t cb) {
    std::atomic_flag flag = ATOMIC_FLAG_INIT;
    flag.test_and_set();

    wchar_t currentExePath[MAX_PATH];
    GetModuleFileNameW(nullptr, currentExePath, MAX_PATH);
    wchar_t *currentExeName = PathFindFileNameW(currentExePath);

    wchar_t dataPath[MAX_PATH];
    if (!SUCCEEDED(
            SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, dataPath))) {
      return false;
    }
    wchar_t userDataFolder[MAX_PATH];
    PathCombineW(userDataFolder, dataPath, currentExeName);
    m_com_handler = new webview2_com_handler(
        wnd, cb,
        [&](ICoreWebView2Controller *controller, ICoreWebView2 *webview) {
          if (!controller || !webview) {
            debug_logf("webview2_com_handler callback controller pointer is %p, webivew pointer is %p\n", controller, webview);
            flag.clear();
            return;
          }
          controller->AddRef();
          webview->AddRef();
          m_controller = controller;
          m_webview = webview;
          debug_logf("webview2_com_handler callback got webview %p for window %p\n", webview, m_window);
          flag.clear();
          embed_latter_job(debug);
        },
        create_new_webview, 
            debug, 
            this->main_window_flag ? nullptr : this->m_webview2_on_webview_first_inited
        );
    debug_logf("webview2_com_handler created, now trying to call m_com_handler->set_attempt_handler\n");
    m_com_handler->set_attempt_handler([&] {
      return m_webview2_loader.create_environment_with_options(
          nullptr, userDataFolder, nullptr, m_com_handler);
    });
    debug_logf("trying to create environment\n");
    m_com_handler->try_create_environment();

    if (this->main_window_flag) {
      debug_logf("Trying to read messages in SYNC mode \n");
      this->event_thread_method(&flag);
    } 
    else {
      std::thread event_thread(&win32_edge_engine::event_thread_method, this, nullptr, 0);
      event_thread.detach();
    }
    debug_logf("Checking if the m_webivew and m_controller ok m_controller=%p "
               "m_webview=%p\n",
               m_controller, m_webview);
    if (!m_controller || !m_webview) {
      this->embed_latter_job_needed = true;
      return false;
    }
    this->embed_latter_job(debug, true);
    return true;
  }

  bool embed_latter_job(bool debug, bool force = false) {
    debug_logf("embed_latter_job is calling, embed_latter_job_needed = %d, "
               "embed_latter_job_called = %d, force=%d m_controller=%p m_webview=%p\n ",
               embed_latter_job_needed ? 1 : 0, embed_latter_job_called ? 1 : 0,
               force ? 1 : 0, m_controller, m_webview);
    if(!this->embed_latter_job_needed && !force) {
      return true;
    }
    if (this->embed_latter_job_called && !force) {
        return true;
    }
    debug_logf("m_webview->add_NewWindowRequested\n");
    m_webview->add_NewWindowRequested(m_com_handler, nullptr);
    ICoreWebView2Settings *settings = nullptr;
    debug_logf("m_webview->get_Settings\n");
    auto res = m_webview->get_Settings(&settings);
    if (res != S_OK) {
      return false;
    }
    debug_logf("settings->put_AreDevToolsEnabled\n");
    res = settings->put_AreDevToolsEnabled(debug ? TRUE : FALSE);
    if (res != S_OK) {
      return false;
    }
    this->setup_webview_before_navigation();
    return true;
  }

  int need_quit() {
      debug_logf("need quit? is main window = %d \n", this->main_window_flag);
      return this->main_window_flag;
  }
  void resize(HWND wnd) {
    if (m_controller == nullptr) {
      return;
    }
    RECT bounds;
    GetClientRect(wnd, &bounds);
    m_controller->put_Bounds(bounds);
  }

  bool is_webview2_available() const noexcept {
    LPWSTR version_info = nullptr;
    auto res = m_webview2_loader.get_available_browser_version_string(
        nullptr, &version_info);
    // The result will be equal to HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)
    // if the WebView2 runtime is not installed.
    auto ok = SUCCEEDED(res) && version_info;
    if (version_info) {
      CoTaskMemFree(version_info);
    }
    return ok;
  }
  virtual void on_message(const std::string &msg) = 0;

  // The app is expected to call CoInitializeEx before
  // CreateCoreWebView2EnvironmentWithOptions.
  // Source: https://docs.microsoft.com/en-us/microsoft-edge/webview2/reference/win32/webview2-idl#createcorewebview2environmentwithoptions
  com_init_wrapper m_com_init{COINIT_APARTMENTTHREADED};
  HWND m_window = nullptr;
  POINT m_minsz = POINT{0, 0};
  POINT m_maxsz = POINT{0, 0};
  DWORD m_main_thread = GetCurrentThreadId();
  ICoreWebView2 *m_webview = nullptr;
  ICoreWebView2Controller *m_controller = nullptr;
  webview2_com_handler *m_com_handler = nullptr;
  mswebview2::loader m_webview2_loader;
  ICoreWebView2NewWindowRequestedEventHandler *new_window_handler = nullptr;
  simple_callback m_webview2_on_webview_first_inited = nullptr;
  int main_window_flag = 0;
  int debug_flag = 0;
  int keep_running_event_thread = 1;
  bool embed_latter_job_called = 0;
  bool embed_latter_job_needed = 0;
};

} // namespace detail

using browser_engine = detail::win32_edge_engine;

} // namespace webview

#endif /* WEBVIEW_GTK, WEBVIEW_COCOA, WEBVIEW_EDGE */

namespace webview {
class webview : public browser_engine {
public:
  webview(bool debug = false, void *wnd = nullptr, int is_main = 1,
          int lazy = 0)
      : browser_engine(debug, wnd, is_main, lazy) {
    debug_logf("webview inititalization ismain=%d\n", is_main);
  }

  void navigate(const std::string &url) {
    if (url.empty()) {
      browser_engine::navigate("about:blank");
      return;
    }
    browser_engine::navigate(url);
  }

  using binding_t = std::function<void(std::string, std::string, void *)>;
  class binding_ctx_t {
  public:
    binding_ctx_t(binding_t callback, void *arg)
        : callback(callback), arg(arg) {}
    // This function is called upon execution of the bound JS function
    binding_t callback;
    // This user-supplied argument is passed to the callback
    void *arg;
  };

  using sync_binding_t = std::function<std::string(std::string)>;

  // Synchronous bind
  void bind(const std::string &name, sync_binding_t fn) {
    auto wrapper = [this, fn](const std::string &seq, const std::string &req,
                              void * /*arg*/) { resolve(seq, 0, fn(req)); };
    bind(name, wrapper, nullptr);
  }

  // Asynchronous bind
  void bind(const std::string &name, binding_t fn, void *arg) {
    if (bindings.count(name) > 0) {
      return;
    }
    bindings.emplace(name, binding_ctx_t(fn, arg));
    auto js = "(function() { var name = '" + name + "';" + R""(
      var RPC = window._rpc = (window._rpc || {nextSeq: 1});
      window[name] = function() {
        var seq = RPC.nextSeq++;
        var promise = new Promise(function(resolve, reject) {
          RPC[seq] = {
            resolve: resolve,
            reject: reject,
          };
        });
        window.external.invoke(JSON.stringify({
          id: seq,
          method: name,
          params: Array.prototype.slice.call(arguments),
        }));
        return promise;
      }
    })())"";
    init(js);
    eval(js);
  }

  void unbind(const std::string &name) {
    auto found = bindings.find(name);
    if (found != bindings.end()) {
      auto js = "delete window['" + name + "'];";
      init(js);
      eval(js);
      bindings.erase(found);
    }
  }

  void resolve(const std::string &seq, int status, const std::string &result) {
    dispatch([seq, status, result, this]() {
      if (status == 0) {
        eval("window._rpc[" + seq + "].resolve(" + result +
             "); delete window._rpc[" + seq + "]");
      } else {
        eval("window._rpc[" + seq + "].reject(" + result +
             "); delete window._rpc[" + seq + "]");
      }
    });
  }

private:
  void on_message(const std::string &msg) {
    auto seq = detail::json_parse(msg, "id", 0);
    auto name = detail::json_parse(msg, "method", 0);
    auto args = detail::json_parse(msg, "params", 0);
    auto found = bindings.find(name);
    if (found == bindings.end()) {
      return;
    }
    const auto &context = found->second;
    context.callback(seq, args, context.arg);
  }

  std::map<std::string, binding_ctx_t> bindings;
};

webview_window_t create_new_webview(bool debug, void* window, 
    int is_main=0, int lazy = 0) {
  debug_logf("create_new_webview is calling \n");
  return new webview(debug, window, is_main, lazy);
}
} // namespace webview

#include <stdio.h>

WEBVIEW_API webview_t webview_create(int debug, void *wnd) {
  int main_flag = 1;
  debug_logf("try calling webview::webview with is_main=%d\n", main_flag);
  debug_logf("main flag =  %d\n", main_flag);
  auto w = new webview::webview(debug, wnd, main_flag);
  if (!w->window()) {
    delete w;
    return nullptr;
  }
  return w;
}

WEBVIEW_API void webview_destroy(webview_t w) {
  delete static_cast<webview::webview *>(w);
}

WEBVIEW_API void webview_run(webview_t w) {
  static_cast<webview::webview *>(w)->run();
}

WEBVIEW_API void webview_terminate(webview_t w) {
  static_cast<webview::webview *>(w)->terminate();
}

WEBVIEW_API void webview_dispatch(webview_t w, void (*fn)(webview_t, void *),
                                  void *arg) {
  static_cast<webview::webview *>(w)->dispatch([=]() { fn(w, arg); });
}

WEBVIEW_API void *webview_get_window(webview_t w) {
  return static_cast<webview::webview *>(w)->window();
}

WEBVIEW_API void webview_set_title(webview_t w, const char *title) {
  static_cast<webview::webview *>(w)->set_title(title);
}

WEBVIEW_API void webview_set_size(webview_t w, int width, int height,
                                  int hints) {
  static_cast<webview::webview *>(w)->set_size(width, height, hints);
}

WEBVIEW_API void webview_set_position_n_size(webview_t w, int x, int y,
                                             int width, int height, int hints) {
  static_cast<webview::webview *>(w)->set_size(x, y, width, height, hints);
}

WEBVIEW_API void webview_navigate(webview_t w, const char *url) {
  static_cast<webview::webview *>(w)->navigate(url);
}

void webview_navigate_popup(webview_t w, const char* url, int window_id) {
  static_cast<webview::webview *>(w)->navigate_popup(url, window_id);
}

WEBVIEW_API void webview_set_html(webview_t w, const char *html) {
  static_cast<webview::webview *>(w)->set_html(html);
}

WEBVIEW_API void webview_init(webview_t w, const char *js) {
  static_cast<webview::webview *>(w)->init(js);
}

WEBVIEW_API void webview_eval(webview_t w, const char *js) {
  static_cast<webview::webview *>(w)->eval(js);
}

WEBVIEW_API void webview_bind(webview_t w, const char *name,
                              void (*fn)(const char *seq, const char *req,
                                         void *arg),
                              void *arg) {
  static_cast<webview::webview *>(w)->bind(
      name,
      [=](const std::string &seq, const std::string &req, void *arg) {
        fn(seq.c_str(), req.c_str(), arg);
      },
      arg);
}

WEBVIEW_API void webview_unbind(webview_t w, const char *name) {
  static_cast<webview::webview *>(w)->unbind(name);
}

WEBVIEW_API void webview_return(webview_t w, const char *seq, int status,
                                const char *result) {
  static_cast<webview::webview *>(w)->resolve(seq, status, result);
}

WEBVIEW_API const webview_version_info_t *webview_version() {
  return &webview::detail::library_version_info;
}

WEBVIEW_API void webview_set_child_window_opened_callback(cb_ext_child_window_created callback) {
    webview::webview_provider::set_cb_ext_child_window_created(callback);
}
WEBVIEW_API void webview_set_child_window_closed_callback(cb_ext_child_window_closed callback) {
    webview::webview_provider::set_cb_ext_child_window_closed(callback);
}

#endif /* WEBVIEW_HEADER */
#endif /* __cplusplus */
#endif /* WEBVIEW_H */
