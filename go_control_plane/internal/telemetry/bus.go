// ─────────────────────────────────────────────────────────────────────────────
// internal/telemetry/bus.go  –  In-process pub/sub message bus.
// internal/telemetry/zmq_subscriber.go  –  ZeroMQ → Bus ingestion.
// ─────────────────────────────────────────────────────────────────────────────

package telemetry

import (
	"context"
	"encoding/json"
	"sync"
	"time"

	"go.uber.org/zap"
)

// ─── Packet ──────────────────────────────────────────────────────────────────

// Packet is a decoded telemetry/gesture envelope received from upstream.
type Packet struct {
	Type      string          `json:"type"`
	Timestamp float64         `json:"timestamp"`
	Source    string          `json:"source"`
	Raw       []byte          `json:"-"`
	Data      json.RawMessage `json:"data,omitempty"`
}

// ─── Bus ─────────────────────────────────────────────────────────────────────

type Handler func(Packet)

type Bus struct {
	ch       chan Packet
	handlers []Handler
	mu       sync.RWMutex
}

func NewBus(bufSize int) *Bus {
	b := &Bus{ch: make(chan Packet, bufSize)}
	go b.dispatch()
	return b
}

func (b *Bus) Publish(pkt Packet) {
	select {
	case b.ch <- pkt:
	default:
		// Drop on overflow – never block the ingestion goroutine
	}
}

func (b *Bus) Subscribe(h Handler) {
	b.mu.Lock()
	defer b.mu.Unlock()
	b.handlers = append(b.handlers, h)
}

func (b *Bus) dispatch() {
	for pkt := range b.ch {
		b.mu.RLock()
		hs := b.handlers
		b.mu.RUnlock()
		for _, h := range hs {
			h(pkt)
		}
	}
}

// ─── ZMQ Subscriber ──────────────────────────────────────────────────────────
// Uses polling via net.Conn simulation (pure Go zmq4 via pebbe/zmq4).
// Falls back to a no-op implementation when ZMQ is not available.

// ZMQSubscriber ingests from a ZeroMQ PUB endpoint.
type ZMQSubscriber struct {
	endpoint     string
	telemetryBus *Bus
	gestureBus   *Bus
	metrics      MetricsUpdater
	log          *zap.SugaredLogger
}

type MetricsUpdater interface {
	RecordPacket(source string, latencyMs float64)
}

func NewZMQSubscriber(
	endpoint string,
	tBus, gBus *Bus,
	metrics MetricsUpdater,
	log *zap.SugaredLogger,
) *ZMQSubscriber {
	return &ZMQSubscriber{
		endpoint:     endpoint,
		telemetryBus: tBus,
		gestureBus:   gBus,
		metrics:      metrics,
		log:          log,
	}
}

func (z *ZMQSubscriber) Run(ctx context.Context) {
	// NOTE: In production, initialise zmq4.NewSub socket here.
	// For portability in this reference implementation we simulate
	// the receive loop with a placeholder that logs and waits.
	//
	// Real implementation:
	//   sock, _ := zmq4.NewSocket(zmq4.SUB)
	//   sock.Connect(z.endpoint)
	//   sock.SetSubscribe("")
	//   for { msg, err := sock.RecvBytes(0); ... }

	z.log.Infof("[ZMQ] Subscriber connecting to %s", z.endpoint)

	ticker := time.NewTicker(100 * time.Millisecond)
	defer ticker.Stop()

	for {
		select {
		case <-ctx.Done():
			z.log.Infof("[ZMQ] Subscriber %s stopped", z.endpoint)
			return
		case <-ticker.C:
			// Real: receive from socket and call z.ingest(msg)
		}
	}
}

func (z *ZMQSubscriber) ingest(raw []byte) {
	var pkt Packet
	if err := json.Unmarshal(raw, &pkt); err != nil {
		z.log.Warnf("[ZMQ] Bad JSON from %s: %v", z.endpoint, err)
		return
	}
	pkt.Raw = raw

	latencyMs := (float64(time.Now().UnixMicro()) - pkt.Timestamp*1e6) / 1000.0
	z.metrics.RecordPacket(pkt.Source, latencyMs)

	switch pkt.Type {
	case "gesture_event":
		z.gestureBus.Publish(pkt)
	default:
		z.telemetryBus.Publish(pkt)
	}
}
