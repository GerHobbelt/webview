# Added support for open child window in MS Windows ONLY

## 1. Why?

The original version does not support open the second windows  without location bar, blah blah blah...

And the you will get unexpected new window with location bar if you decide to use window.open inside html of main window.

### Why not just using thread for new window?

Well, it is not supported by MS webview2. [Check this link for more details](https://learn.microsoft.com/en-us/microsoft-edge/webview2/concepts/threading-model).

## 2. How?

You JUST need to implement ICoreWebView2NewWindowRequestedEventHandler. That's the short answer.  But it is really a LONG story to make it works.

## 3. How can i use it?

### Try it quickly if you are hurry

You may check ```examples/open_window.go```. And I also added ```build_open_window.bat``` so you can try it directly.

#### How can I reference the code?

```bash
go get -u github.com/ColinZou/webview@mwindow-0.1.2
```



### NOTE

#### a. register a child window open callback

Call ```webview.AddChildWindowCallback``` before create any window first, so you can manage the child windows.

#### b. always resize opened child so it can be rendered correctly

#### c. debug logging

You can add define of ```NDEBUG``` for stop verbose logging.

#### d. auto close main window and quit application if there's no more child window running

You need to define ```WEBVIEW_CLOSE_MAIN_WINDOW_WHEN_NO_CHILD_WINDOWS``` in order to do that.

#### e. you MUST do bind for all opened child windows

#### f. close main window will close all child windows automatically

You can change it inside terminate method of ```native/webview.h``` #2064:

  ```if (this->main_window_flag) { xxxx```

#### g. it is linked dynamically for go in the example

Well, I have no time to resolve the compiling issue for static link right now. And I have to split the headers in order to do dynamic linking, sorry.

#### i. golang binding is broken in child window.

You need to call native method in following way:

```javascript
window.chrome.webview.postMessage('{"id":1,"method":"hello","params":[]}')
```


















































