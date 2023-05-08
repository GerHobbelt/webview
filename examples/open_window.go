package main

import (
	"fmt"
	"time"

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
	go func() {
		var total = 10
		for {
			if total == 0 {
				break
			}
			err := w.Bind("hello", hello)
			if err != nil {
				fmt.Printf("Failed to bind: %v\n", err)
			}
			w.Eval("console.info('hello from golang', new Date());")
			time.Sleep(time.Second * 1)
			total = total - 1
		}
	}()

}
func hello() {
	fmt.Println("Hello from golang")
}
func main() {
	webview.AddChildWindowCallback(onChildWindowOpen)
	w := webview.New(true)
	defer w.Destroy()
	w.SetTitle("Basic Example")
	w.SetSize(480, 320, webview.HintNone)
	w.Bind("openPopup", openPopup)
	w.Navigate("http://localhost:3030/webview.html")
	globalWindow = w
	w.Run()
}
