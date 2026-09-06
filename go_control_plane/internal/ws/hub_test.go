package ws

import (
	"net/http/httptest"
	"testing"
)

func TestWebsocketOriginAllowed(t *testing.T) {
	t.Setenv("ARX_WS_ALLOWED_ORIGINS", "https://ops.example.com")

	tests := []struct {
		name   string
		host   string
		origin string
		want   bool
	}{
		{name: "native client without origin", host: "127.0.0.1:8080", origin: "", want: true},
		{name: "same origin", host: "aether.local:8080", origin: "http://aether.local:8080", want: true},
		{name: "loopback aliases", host: "127.0.0.1:8080", origin: "http://localhost:5173", want: true},
		{name: "configured remote origin", host: "0.0.0.0:8080", origin: "https://ops.example.com", want: true},
		{name: "untrusted remote origin", host: "0.0.0.0:8080", origin: "https://evil.example.com", want: false},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			req := httptest.NewRequest("GET", "http://"+tt.host+"/ws", nil)
			req.Host = tt.host
			if tt.origin != "" {
				req.Header.Set("Origin", tt.origin)
			}
			if got := websocketOriginAllowed(req); got != tt.want {
				t.Fatalf("websocketOriginAllowed() = %v, want %v", got, tt.want)
			}
		})
	}
}
