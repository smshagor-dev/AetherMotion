// ─────────────────────────────────────────────────────────────────────────────
// internal/ws/hub.go  –  WebSocket fan-out hub.
//
// Every message pushed to Hub.Broadcast() is sent to all connected clients.
// Client connections are managed in a goroutine-safe registry.
// Auto-ping keeps connections alive; dead clients are evicted.
// ─────────────────────────────────────────────────────────────────────────────

package ws

import (
	"net"
	"net/http"
	"net/url"
	"os"
	"strings"
	"sync"
	"sync/atomic"
	"time"

	"github.com/gorilla/websocket"
	"go.uber.org/zap"
)

const (
	writeTimeout   = 5 * time.Second
	pingInterval   = 20 * time.Second
	maxMessageSize = 1 << 16 // 64 KB
)

var upgrader = websocket.Upgrader{
	ReadBufferSize:  1024,
	WriteBufferSize: 4096,
	CheckOrigin:     websocketOriginAllowed,
}

func websocketOriginAllowed(r *http.Request) bool {
	origin := strings.TrimSpace(r.Header.Get("Origin"))
	if origin == "" {
		// Non-browser native clients commonly omit Origin.
		return true
	}

	parsed, err := url.Parse(origin)
	if err != nil || parsed.Scheme == "" || parsed.Host == "" {
		return false
	}
	if strings.EqualFold(parsed.Host, r.Host) {
		return true
	}
	if isLoopbackHost(parsed.Hostname()) && isLoopbackRequestHost(r.Host) {
		return true
	}

	for _, allowed := range strings.Split(os.Getenv("ARX_WS_ALLOWED_ORIGINS"), ",") {
		allowed = strings.TrimSpace(strings.TrimRight(allowed, "/"))
		if allowed != "" && strings.EqualFold(strings.TrimRight(origin, "/"), allowed) {
			return true
		}
	}
	return false
}

func isLoopbackRequestHost(hostport string) bool {
	host := hostport
	if parsedHost, _, err := net.SplitHostPort(hostport); err == nil {
		host = parsedHost
	}
	return isLoopbackHost(strings.Trim(host, "[]"))
}

func isLoopbackHost(host string) bool {
	if strings.EqualFold(host, "localhost") {
		return true
	}
	ip := net.ParseIP(host)
	return ip != nil && ip.IsLoopback()
}

// ─────────────────────────────────────────────────────────────────────────────

type client struct {
	hub  *Hub
	conn *websocket.Conn
	send chan []byte
	id   uint64
}

func (c *client) writePump() {
	ticker := time.NewTicker(pingInterval)
	defer func() {
		ticker.Stop()
		c.conn.Close()
		c.hub.unregister <- c
	}()

	for {
		select {
		case msg, ok := <-c.send:
			c.conn.SetWriteDeadline(time.Now().Add(writeTimeout))
			if !ok {
				c.conn.WriteMessage(websocket.CloseMessage, []byte{})
				return
			}
			w, err := c.conn.NextWriter(websocket.TextMessage)
			if err != nil {
				return
			}
			w.Write(msg)
			// Drain any queued messages in the same write frame.
			n := len(c.send)
			for i := 0; i < n; i++ {
				w.Write([]byte{'\n'})
				w.Write(<-c.send)
			}
			if err := w.Close(); err != nil {
				return
			}

		case <-ticker.C:
			c.conn.SetWriteDeadline(time.Now().Add(writeTimeout))
			if err := c.conn.WriteMessage(websocket.PingMessage, nil); err != nil {
				return
			}
		}
	}
}

func (c *client) readPump() {
	defer func() { c.hub.unregister <- c }()
	c.conn.SetReadLimit(maxMessageSize)
	c.conn.SetReadDeadline(time.Now().Add(60 * time.Second))
	c.conn.SetPongHandler(func(string) error {
		c.conn.SetReadDeadline(time.Now().Add(60 * time.Second))
		return nil
	})
	for {
		if _, _, err := c.conn.ReadMessage(); err != nil {
			break
		}
	}
}

// ─────────────────────────────────────────────────────────────────────────────

type Hub struct {
	clients    map[*client]struct{}
	broadcast  chan []byte
	register   chan *client
	unregister chan *client
	mu         sync.RWMutex
	nextID     atomic.Uint64
	log        *zap.SugaredLogger
}

func NewHub(log *zap.SugaredLogger) *Hub {
	return &Hub{
		clients:    make(map[*client]struct{}),
		broadcast:  make(chan []byte, 256),
		register:   make(chan *client, 32),
		unregister: make(chan *client, 32),
		log:        log,
	}
}

func (h *Hub) Run() {
	for {
		select {
		case c := <-h.register:
			h.mu.Lock()
			h.clients[c] = struct{}{}
			total := len(h.clients)
			h.mu.Unlock()
			h.log.Infof("[WS] Client %d connected (total=%d)", c.id, total)

		case c := <-h.unregister:
			h.mu.Lock()
			if _, ok := h.clients[c]; ok {
				delete(h.clients, c)
				close(c.send)
				total := len(h.clients)
				h.mu.Unlock()
				h.log.Infof("[WS] Client %d disconnected (total=%d)", c.id, total)
			} else {
				h.mu.Unlock()
			}

		case msg := <-h.broadcast:
			h.mu.RLock()
			for c := range h.clients {
				select {
				case c.send <- msg:
				default:
					// Slow client: drop message (never block).
				}
			}
			h.mu.RUnlock()
		}
	}
}

func (h *Hub) Broadcast(msg []byte) {
	select {
	case h.broadcast <- msg:
	default:
		// Hub queue full: drop message rather than blocking producers.
	}
}

func (h *Hub) ClientCount() int {
	h.mu.RLock()
	defer h.mu.RUnlock()
	return len(h.clients)
}

func (h *Hub) ServeWS(w http.ResponseWriter, r *http.Request) {
	conn, err := upgrader.Upgrade(w, r, nil)
	if err != nil {
		h.log.Warnf("[WS] Upgrade error: %v", err)
		return
	}
	c := &client{
		hub:  h,
		conn: conn,
		send: make(chan []byte, 256),
		id:   h.nextID.Add(1),
	}
	h.register <- c
	go c.writePump()
	go c.readPump()
}
