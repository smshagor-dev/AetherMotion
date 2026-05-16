package api

import (
	"encoding/json"
	"net/http"
	"sync"
	"time"

	"github.com/arx-platform/control-plane/internal/metrics"
	"github.com/arx-platform/control-plane/internal/telemetry"
	"github.com/gin-gonic/gin"
)

type packetRing struct {
	mu  sync.RWMutex
	buf []telemetry.Packet
	pos int
	cap int
}

func newPacketRing(capacity int) *packetRing {
	return &packetRing{
		buf: make([]telemetry.Packet, capacity),
		cap: capacity,
	}
}

func (r *packetRing) push(p telemetry.Packet) {
	r.mu.Lock()
	r.buf[r.pos] = p
	r.pos = (r.pos + 1) % r.cap
	r.mu.Unlock()
}

func (r *packetRing) last(n int) []telemetry.Packet {
	r.mu.RLock()
	defer r.mu.RUnlock()

	if n > r.cap {
		n = r.cap
	}

	out := make([]telemetry.Packet, 0, n)
	start := (r.pos - n + r.cap) % r.cap
	for i := 0; i < n; i++ {
		out = append(out, r.buf[(start+i)%r.cap])
	}
	return out
}

func RegisterTelemetry(g *gin.RouterGroup, bus *telemetry.Bus, store *metrics.Store) {
	ring := newPacketRing(500)
	bus.Subscribe(func(p telemetry.Packet) { ring.push(p) })

	g.GET("/telemetry", func(c *gin.Context) {
		packets := ring.last(50)
		c.JSON(http.StatusOK, gin.H{
			"count":   len(packets),
			"packets": packets,
		})
	})

	g.GET("/telemetry/metrics", func(c *gin.Context) {
		c.JSON(http.StatusOK, store.Snapshot())
	})
}

func RegisterGestures(g *gin.RouterGroup, bus *telemetry.Bus) {
	type gestureRecord struct {
		Gesture    string  `json:"gesture"`
		Confidence float64 `json:"confidence"`
		EventKind  string  `json:"event_kind"`
		Timestamp  float64 `json:"timestamp"`
	}

	var mu sync.RWMutex
	var log []gestureRecord

	bus.Subscribe(func(p telemetry.Packet) {
		if p.Type != "gesture_event" {
			return
		}

		record := gestureRecord{
			EventKind: "unknown",
			Timestamp: p.Timestamp,
		}

		var raw map[string]interface{}
		if err := unmarshalRaw(p.Raw, &raw); err == nil {
			if v, ok := raw["gesture"].(string); ok {
				record.Gesture = v
			}
			if v, ok := raw["confidence"].(float64); ok {
				record.Confidence = v
			}
			if v, ok := raw["event_kind"].(string); ok {
				record.EventKind = v
			}
		}

		mu.Lock()
		log = append(log, record)
		if len(log) > 1000 {
			log = log[len(log)-1000:]
		}
		mu.Unlock()
	})

	g.GET("/gestures", func(c *gin.Context) {
		mu.RLock()
		out := make([]gestureRecord, len(log))
		copy(out, log)
		mu.RUnlock()

		c.JSON(http.StatusOK, gin.H{"events": out, "total": len(out)})
	})

	g.GET("/gestures/latest", func(c *gin.Context) {
		mu.RLock()
		var latest *gestureRecord
		if len(log) > 0 {
			item := log[len(log)-1]
			latest = &item
		}
		mu.RUnlock()

		c.JSON(http.StatusOK, gin.H{"event": latest})
	})
}

func RegisterHealth(g *gin.RouterGroup, store *metrics.Store) {
	start := time.Now()

	g.GET("/system/health", func(c *gin.Context) {
		c.JSON(http.StatusOK, gin.H{
			"status":   "ok",
			"version":  "2.0.0",
			"uptime_s": time.Since(start).Seconds(),
			"metrics":  store.Snapshot(),
		})
	})

	g.GET("/system/ping", func(c *gin.Context) {
		c.JSON(http.StatusOK, gin.H{
			"pong": true,
			"ts":   time.Now().UnixMilli(),
		})
	})
}

func unmarshalRaw(raw []byte, v interface{}) error {
	return json.Unmarshal(raw, v)
}
