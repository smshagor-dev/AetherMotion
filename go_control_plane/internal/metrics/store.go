// ─────────────────────────────────────────────────────────────────────────────
// internal/metrics/store.go  –  In-memory metrics aggregation.
// ─────────────────────────────────────────────────────────────────────────────

package metrics

import (
	"context"
	"runtime"
	"sync"
	"sync/atomic"
	"time"
)

// ─── Rolling average ──────────────────────────────────────────────────────────

type RollingAvg struct {
	mu     sync.Mutex
	values []float64
	pos    int
	n      int
	cap    int
}

func NewRollingAvg(cap int) *RollingAvg {
	return &RollingAvg{values: make([]float64, cap), cap: cap}
}

func (r *RollingAvg) Add(v float64) {
	r.mu.Lock()
	defer r.mu.Unlock()
	r.values[r.pos] = v
	r.pos = (r.pos + 1) % r.cap
	if r.n < r.cap {
		r.n++
	}
}

func (r *RollingAvg) Avg() float64 {
	r.mu.Lock()
	defer r.mu.Unlock()
	if r.n == 0 {
		return 0
	}
	var sum float64
	for i := 0; i < r.n; i++ {
		sum += r.values[i]
	}
	return sum / float64(r.n)
}

// ─── Store ────────────────────────────────────────────────────────────────────

type SourceMetrics struct {
	PacketCount atomic.Int64
	LatencyAvg  *RollingAvg
	LastSeen    atomic.Int64 // Unix ms
}

type Store struct {
	mu      sync.RWMutex
	sources map[string]*SourceMetrics
	// System stats
	HeapAllocMB atomic.Value // float64
	NumGoroutine atomic.Int64
	Uptime       time.Time
}

func NewStore() *Store {
	return &Store{
		sources: make(map[string]*SourceMetrics),
		Uptime:  time.Now(),
	}
}

func (s *Store) getOrCreate(source string) *SourceMetrics {
	s.mu.RLock()
	m, ok := s.sources[source]
	s.mu.RUnlock()
	if ok {
		return m
	}
	s.mu.Lock()
	defer s.mu.Unlock()
	if m, ok = s.sources[source]; ok {
		return m
	}
	m = &SourceMetrics{LatencyAvg: NewRollingAvg(60)}
	s.sources[source] = m
	return m
}

// RecordPacket implements telemetry.MetricsUpdater.
func (s *Store) RecordPacket(source string, latencyMs float64) {
	m := s.getOrCreate(source)
	m.PacketCount.Add(1)
	m.LatencyAvg.Add(latencyMs)
	m.LastSeen.Store(time.Now().UnixMilli())
}

func (s *Store) Snapshot() map[string]interface{} {
	s.mu.RLock()
	defer s.mu.RUnlock()

	sources := make(map[string]interface{})
	for name, m := range s.sources {
		sources[name] = map[string]interface{}{
			"packets":    m.PacketCount.Load(),
			"latency_ms": m.LatencyAvg.Avg(),
			"last_seen":  m.LastSeen.Load(),
		}
	}

	var heapMB float64
	if v := s.HeapAllocMB.Load(); v != nil {
		heapMB = v.(float64)
	}

	return map[string]interface{}{
		"sources":    sources,
		"heap_mb":    heapMB,
		"goroutines": s.NumGoroutine.Load(),
		"uptime_s":   time.Since(s.Uptime).Seconds(),
	}
}

func (s *Store) CollectSystem(ctx context.Context) {
	ticker := time.NewTicker(2 * time.Second)
	defer ticker.Stop()
	for {
		select {
		case <-ctx.Done():
			return
		case <-ticker.C:
			var ms runtime.MemStats
			runtime.ReadMemStats(&ms)
			s.HeapAllocMB.Store(float64(ms.HeapAlloc) / 1e6)
			s.NumGoroutine.Store(int64(runtime.NumGoroutine()))
		}
	}
}
