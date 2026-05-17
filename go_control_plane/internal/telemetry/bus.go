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

	"github.com/go-zeromq/zmq4"
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
	z.log.Infof("[ZMQ] Subscriber connecting to %s", z.endpoint)

	sock := zmq4.NewSub(ctx)
	if err := sock.SetOption(zmq4.OptionSubscribe, ""); err != nil {
		z.log.Errorf("[ZMQ] Subscribe option failed for %s: %v", z.endpoint, err)
		return
	}
	if err := sock.Dial(z.endpoint); err != nil {
		z.log.Errorf("[ZMQ] Dial failed for %s: %v", z.endpoint, err)
		return
	}
	defer func() {
		if err := sock.Close(); err != nil {
			z.log.Warnf("[ZMQ] Close failed for %s: %v", z.endpoint, err)
		}
	}()

	go func() {
		<-ctx.Done()
		_ = sock.Close()
	}()

	for {
		msg, err := sock.Recv()
		if err != nil {
			select {
			case <-ctx.Done():
				z.log.Infof("[ZMQ] Subscriber %s stopped", z.endpoint)
				return
			default:
				z.log.Warnf("[ZMQ] Receive failed from %s: %v", z.endpoint, err)
				time.Sleep(100 * time.Millisecond)
				continue
			}
		}
		raw := msg.Bytes()
		if len(raw) == 0 {
			continue
		}
		z.ingest(raw)
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
