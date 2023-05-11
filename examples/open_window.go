package main

import (
	"fmt"

	"github.com/ColinZou/webview"
)

var globalWindow webview.WebView

func openPopup() {
	fmt.Println("openPopup called")
	globalWindow.OpenChildWindow(100, "http://localhost:3030/child.html")
}
func onChildWindowOpen(windowId int, w webview.WebView) {
	fmt.Printf("onChildWindowOpen window id is %d, view is %v \n", windowId, w)
	w.SetSize(1024, 768, webview.HintNone)
}
func hello() {
	fmt.Println("Hello from child window")
}
func main() {
	webview.AddChildWindowCallback(onChildWindowOpen)
	w := webview.New(true)
	defer w.Destroy()
	w.SetTitle("Basic Example")
	w.SetSize(480, 320, webview.HintNone)
	w.Bind("openPopup", openPopup)
	w.Bind("hello", hello)
	w.Navigate("http://localhost:3030/webview.html")
	globalWindow = w
	w.Run()
}
