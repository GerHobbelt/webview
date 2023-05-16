package main

import (
	"fmt"
    "time"
	"github.com/ColinZou/webview"
)

var globalWindow webview.WebView
var childWindow webview.WebView

func openPopup() {
	fmt.Println("openPopup called")
	globalWindow.OpenChildWindow(100, "http://localhost:3030/child.html")
}
func onChildWindowOpen(windowId int, w webview.WebView) {
	if windowId <= 0 {
		fmt.Println("Got main window creation callback")
		return
	}
	childWindow = w
	w.EnableNativeMethodInvoke()
	fmt.Printf("onChildWindowOpen window id is %d, view is %v \n", windowId, w)
	w.SetSize(1024, 768, webview.HintNone)
	go func() {
		fmt.Println("hide window latter")
		time.Sleep(time.Second * 2)
		w.HideWindow()
		fmt.Println("show window latter")
		time.Sleep(time.Second * 2)
		w.ShowWindow()
	}()
}
func onChildWindowClosed(windowId int) {
	fmt.Printf("Window id=%d just closed\n", windowId)
}
func hello() {
	fmt.Println("Hello from golang")
}
func nativeCall(windowId int, args string) (bool, string) {
	fmt.Printf("invoking nativeCall with windowId=%d args=%v\n", windowId, args)
	if windowId == 100 {
		fmt.Printf("It is child window\n")
		childWindow.Eval("window.callByNative();")
	}
	return true, fmt.Sprintf("processed %s", args)
}
func main() {
	webview.AddChildWindowCallback(onChildWindowOpen)
	webview.AddChildWindowClosedCallback(onChildWindowClosed)
	webview.AddNativeMethodInvoke(nativeCall)
	w := webview.New(true)
	globalWindow = w
	defer w.Destroy()
	w.SetTitle("Basic Example")
	w.SetSize(480, 320, webview.HintNone)
	w.Bind("openPopup", openPopup)
	w.Navigate("http://localhost:3030/webview.html")
	w.Run()
}
