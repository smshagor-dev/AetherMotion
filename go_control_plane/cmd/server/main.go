package main

import (
	"context"
	"net"
	"net/http"
	"net/url"
	"os"
	"os/signal"
	"strings"
	"syscall"
	"time"

	"github.com/arx-platform/control-plane/internal/api"
	"github.com/arx-platform/control-plane/internal/metrics"
	"github.com/arx-platform/control-plane/internal/telemetry"
	"github.com/arx-platform/control-plane/internal/ws"

	"github.com/gin-gonic/gin"
	"go.uber.org/zap"
)

func main() {
	log, _ := zap.NewProduction()
	defer log.Sync() //nolint:errcheck
	sugar := log.Sugar()

	sugar.Info("ARX Control Plane v2.0 starting")

	cppEndpoint := getenvDefault("ARX_CPP_ZMQ", "tcp://127.0.0.1:5556")
	aiEndpoint := getenvDefault("ARX_AI_ZMQ", "tcp://127.0.0.1:5557")
	httpAddr := getenvDefault("ARX_HTTP_ADDR", "127.0.0.1:8080")

	telemetryBus := telemetry.NewBus(256)
	gestureBus := telemetry.NewBus(64)
	metricsStore := metrics.NewStore()
	wsHub := ws.NewHub(sugar)

	cppSub := telemetry.NewZMQSubscriber(
		cppEndpoint,
		telemetryBus,
		gestureBus,
		metricsStore,
		sugar,
	)
	aiSub := telemetry.NewZMQSubscriber(
		aiEndpoint,
		telemetryBus,
		gestureBus,
		metricsStore,
		sugar,
	)

	go wsHub.Run()
	telemetryBus.Subscribe(func(pkt telemetry.Packet) {
		sugar.Infof("[Go] frame forwarded type=%s source=%s", pkt.Type, pkt.Source)
		wsHub.Broadcast(pkt.Raw)
	})
	gestureBus.Subscribe(func(pkt telemetry.Packet) {
		sugar.Infof("[Go] frame forwarded type=%s source=%s", pkt.Type, pkt.Source)
		wsHub.Broadcast(pkt.Raw)
	})

	gin.SetMode(gin.ReleaseMode)
	r := gin.New()
	r.Use(gin.Recovery())
	r.Use(corsMiddleware())

	apiGroup := r.Group("/api")
	api.RegisterTelemetry(apiGroup, telemetryBus, metricsStore)
	api.RegisterGestures(apiGroup, gestureBus)
	api.RegisterHealth(apiGroup, metricsStore)

	r.GET("/ws", func(c *gin.Context) {
		wsHub.ServeWS(c.Writer, c.Request)
	})

	srv := &http.Server{
		Addr:              httpAddr,
		Handler:           r,
		ReadTimeout:       10 * time.Second,
		ReadHeaderTimeout: 5 * time.Second,
		WriteTimeout:      10 * time.Second,
		IdleTimeout:       60 * time.Second,
	}

	go func() {
		sugar.Infof("HTTP server listening on %s", srv.Addr)
		if err := srv.ListenAndServe(); err != nil && err != http.ErrServerClosed {
			sugar.Fatalf("HTTP server error: %v", err)
		}
	}()

	ctx, cancel := context.WithCancel(context.Background())
	go cppSub.Run(ctx)
	go aiSub.Run(ctx)
	go metricsStore.CollectSystem(ctx)

	sugar.Info("Control plane running. Ctrl-C to stop.")

	quit := make(chan os.Signal, 1)
	signal.Notify(quit, syscall.SIGINT, syscall.SIGTERM)
	<-quit

	sugar.Info("Shutting down...")
	cancel()

	shutCtx, shutCancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer shutCancel()
	if err := srv.Shutdown(shutCtx); err != nil {
		sugar.Errorf("Server shutdown error: %v", err)
	}
	sugar.Info("ARX Control Plane stopped cleanly.")
}

func getenvDefault(key, fallback string) string {
	if value := os.Getenv(key); value != "" {
		return value
	}
	return fallback
}

func corsMiddleware() gin.HandlerFunc {
	return func(c *gin.Context) {
		origin := strings.TrimSpace(c.GetHeader("Origin"))
		if origin != "" && corsOriginAllowed(origin, c.Request.Host) {
			c.Header("Access-Control-Allow-Origin", origin)
			c.Header("Vary", "Origin")
		}
		c.Header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
		c.Header("Access-Control-Allow-Headers", "Content-Type, Authorization")
		if c.Request.Method == http.MethodOptions {
			if origin != "" && !corsOriginAllowed(origin, c.Request.Host) {
				c.AbortWithStatus(http.StatusForbidden)
				return
			}
			c.AbortWithStatus(http.StatusNoContent)
			return
		}
		c.Next()
	}
}

func corsOriginAllowed(origin, requestHost string) bool {
	parsed, err := url.Parse(origin)
	if err != nil || parsed.Scheme == "" || parsed.Host == "" {
		return false
	}

	if strings.EqualFold(parsed.Host, requestHost) {
		return true
	}
	if isLoopbackHost(parsed.Hostname()) && isLoopbackRequestHost(requestHost) {
		return true
	}

	for _, allowed := range strings.Split(os.Getenv("ARX_CORS_ALLOWED_ORIGINS"), ",") {
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
