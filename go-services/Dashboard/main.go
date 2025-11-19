package main

import (
	"encoding/json"
	"fmt"
	"log"
	"math/rand"
	"net/http"
	"sync"
	"time"

	"github.com/gorilla/mux"
	"github.com/rs/cors"
)

type Request struct {
	ClientIP    string    `json:"clientIP"`
	Path        string    `json:"path"`
	IsMalicious bool      `json:"isMalicious"`
	ReceivedAt  time.Time `json:"receivedAt"`
}

type Client struct {
	IP          string    `json:"ip"`
	Country     string    `json:"country"`
	IsMalicious bool      `json:"isMalicious"`
	ServerID    string    `json:"serverId"`
	FirstSeen   time.Time `json:"firstSeen"`
	LastSeen    time.Time `json:"lastSeen"`
	RequestCount int      `json:"requestCount"`
}

type AgentsInfo struct {
	RealServers int `json:"realServers"`
	Honeypots   int `json:"honeypots"`
}

type Stats struct {
	TotalRequests    int `json:"totalRequests"`
	LegitClients     int `json:"legitClients"`
	MaliciousClients int `json:"maliciousClients"`
}

type SSEClient struct {
	ID      string
	Channel chan []byte
}

type SSEMessage struct {
	Type string      `json:"type"`
	Data interface{} `json:"data"`
}

type App struct {
	mu        sync.RWMutex
	requests  []Request
	clients   map[string]*Client
	agents    AgentsInfo
	stats     Stats
	sseClients map[*SSEClient]bool
	sseMutex  sync.RWMutex
}

func NewApp() *App {
	app := &App{
		requests:   make([]Request, 0),
		clients:    make(map[string]*Client),
		sseClients: make(map[*SSEClient]bool),
		agents: AgentsInfo{
			RealServers: 3,
			Honeypots:   5,
		},
	}

	// Initialize with demo data
	app.initializeDemoData()
	
	// Start background task to generate demo requests
	go app.generateDemoRequests()

	return app
}

func (app *App) initializeDemoData() {
	countries := []string{"us", "ru", "cn", "de", "fr", "uk", "jp", "br"}
	serverTypes := []string{"srv-web", "srv-api", "srv-db", "srv-honeypot"}

	for i := 0; i < 10; i++ {
		country := countries[i%len(countries)]
		serverType := serverTypes[i%len(serverTypes)]
		isMalicious := i%3 == 0

		client := &Client{
			IP:          fmt.Sprintf("192.168.1.%d", 100+i),
			Country:     country,
			IsMalicious: isMalicious,
			ServerID:    fmt.Sprintf("%s-%02d", serverType, i+1),
			FirstSeen:   time.Now().Add(-time.Duration(rand.Intn(24)) * time.Hour),
			LastSeen:    time.Now().Add(-time.Duration(rand.Intn(60)) * time.Minute),
			RequestCount: rand.Intn(50) + 1,
		}

		app.clients[client.IP] = client

		if isMalicious {
			app.stats.MaliciousClients++
		} else {
			app.stats.LegitClients++
		}
	}

	// Add some initial requests
	for i := 0; i < 5; i++ {
		request := Request{
			ClientIP:    fmt.Sprintf("192.168.1.%d", 100+rand.Intn(10)),
			Path:        "/api/test",
			IsMalicious: rand.Float32() < 0.3,
			ReceivedAt:  time.Now().Add(-time.Duration(i*10) * time.Minute),
		}
		app.requests = append(app.requests, request)
		app.stats.TotalRequests++
	}
}

func (app *App) generateDemoRequests() {
	ticker := time.NewTicker(5 * time.Second)
	defer ticker.Stop()

	paths := []string{
		"/api/login",
		"/admin/panel",
		"/api/data",
		"/user/profile",
		"/download/file",
		"/upload",
		"/config",
		"/backup",
	}

	countries := []string{"us", "ru", "cn", "de", "fr", "uk", "jp", "br"}
	serverTypes := []string{"srv-web", "srv-api", "srv-db", "srv-honeypot"}

	for range ticker.C {
		// Generate random IP
		ip := fmt.Sprintf("%d.%d.%d.%d",
			rand.Intn(255), rand.Intn(255), rand.Intn(255), rand.Intn(255))

		// 20% chance of being malicious
		isMalicious := rand.Float32() < 0.2

		request := Request{
			ClientIP:    ip,
			Path:        paths[rand.Intn(len(paths))],
			IsMalicious: isMalicious,
			ReceivedAt:  time.Now(),
		}

		app.mu.Lock()
		app.requests = append(app.requests, request)
		app.stats.TotalRequests++

		// Update or create client
		if client, exists := app.clients[ip]; exists {
			client.LastSeen = time.Now()
			client.RequestCount++
			client.IsMalicious = client.IsMalicious || isMalicious
		} else {
			country := countries[rand.Intn(len(countries))]
			serverType := serverTypes[rand.Intn(len(serverTypes))]

			client := &Client{
				IP:          ip,
				Country:     country,
				IsMalicious: isMalicious,
				ServerID:    fmt.Sprintf("%s-%02d", serverType, len(app.clients)+1),
				FirstSeen:   time.Now(),
				LastSeen:    time.Now(),
				RequestCount: 1,
			}
			app.clients[ip] = client

			if isMalicious {
				app.stats.MaliciousClients++
			} else {
				app.stats.LegitClients++
			}
		}
		app.mu.Unlock()

		// Send SSE update
		app.sendSSEUpdate(request)
	}
}

func (app *App) sendSSEUpdate(request Request) {
	app.mu.RLock()
	stats := map[string]interface{}{
		"totalRequests":    app.stats.TotalRequests,
		"legitClients":     app.stats.LegitClients,
		"maliciousClients": app.stats.MaliciousClients,
		"agents": map[string]int{
			"realServers": app.agents.RealServers,
			"honeypots":   app.agents.Honeypots,
		},
	}
	app.mu.RUnlock()

	message := SSEMessage{
		Type: "new_request",
		Data: map[string]interface{}{
			"request": request,
			"stats":   stats,
		},
	}

	app.broadcastSSE(message)
}

// API Handlers
func (app *App) handleStats(w http.ResponseWriter, r *http.Request) {
	app.mu.RLock()
	defer app.mu.RUnlock()

	stats := map[string]interface{}{
		"totalRequests":    app.stats.TotalRequests,
		"legitClients":     app.stats.LegitClients,
		"maliciousClients": app.stats.MaliciousClients,
		"realServers":      app.agents.RealServers,
		"honeypots":        app.agents.Honeypots,
	}

	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(stats)
}

func (app *App) handleRequests(w http.ResponseWriter, r *http.Request) {
	app.mu.RLock()
	defer app.mu.RUnlock()

	// Get last 20 requests (most recent first)
	limit := 20
	start := len(app.requests) - limit
	if start < 0 {
		start = 0
	}
	recentRequests := app.requests[start:]
	
	// Reverse to show most recent first
	for i, j := 0, len(recentRequests)-1; i < j; i, j = i+1, j-1 {
		recentRequests[i], recentRequests[j] = recentRequests[j], recentRequests[i]
	}

	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(recentRequests)
}

func (app *App) handleClients(w http.ResponseWriter, r *http.Request) {
	app.mu.RLock()
	defer app.mu.RUnlock()

	clients := make([]*Client, 0, len(app.clients))
	for _, client := range app.clients {
		clients = append(clients, client)
	}

	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(clients)
}

func (app *App) handleAgents(w http.ResponseWriter, r *http.Request) {
	app.mu.RLock()
	defer app.mu.RUnlock()

	agents := map[string]int{
		"realServers": app.agents.RealServers,
		"honeypots":   app.agents.Honeypots,
	}

	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(agents)
}

// Balancer request handler (for receiving requests from load balancer)
func (app *App) handleBalancerRequest(w http.ResponseWriter, r *http.Request) {
	if r.Method != "POST" {
		http.Error(w, "Method not allowed", http.StatusMethodNotAllowed)
		return
	}

	var request Request
	if err := json.NewDecoder(r.Body).Decode(&request); err != nil {
		http.Error(w, "Invalid JSON", http.StatusBadRequest)
		return
	}

	request.ReceivedAt = time.Now()

	app.mu.Lock()
	app.requests = append(app.requests, request)
	app.stats.TotalRequests++

	// Update client info
	if client, exists := app.clients[request.ClientIP]; exists {
		client.LastSeen = time.Now()
		client.RequestCount++
		if request.IsMalicious {
			client.IsMalicious = true
		}
	} else {
		// Create new client with random country and server
		countries := []string{"us", "ru", "cn", "de", "fr", "uk", "jp", "br"}
		serverTypes := []string{"srv-web", "srv-api", "srv-db", "srv-honeypot"}
		
		client := &Client{
			IP:          request.ClientIP,
			Country:     countries[rand.Intn(len(countries))],
			IsMalicious: request.IsMalicious,
			ServerID:    fmt.Sprintf("%s-%02d", serverTypes[rand.Intn(len(serverTypes))], len(app.clients)+1),
			FirstSeen:   time.Now(),
			LastSeen:    time.Now(),
			RequestCount: 1,
		}
		app.clients[request.ClientIP] = client

		if request.IsMalicious {
			app.stats.MaliciousClients++
		} else {
			app.stats.LegitClients++
		}
	}
	app.mu.Unlock()

	// Log the request
	status := "✅ LEGIT"
	if request.IsMalicious {
		status = "🚨 MALICIOUS"
	}
	fmt.Printf("%s | %s | %s | %s\n", 
		status, 
		request.ClientIP, 
		request.Path, 
		request.ReceivedAt.Format("15:04:05"))

	// Send SSE update
	app.sendSSEUpdate(request)

	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(map[string]string{"status": "received"})
}

// SSE Handler
func (app *App) handleSSE(w http.ResponseWriter, r *http.Request) {
	w.Header().Set("Content-Type", "text/event-stream")
	w.Header().Set("Cache-Control", "no-cache")
	w.Header().Set("Connection", "keep-alive")
	w.Header().Set("Access-Control-Allow-Origin", "*")
	w.Header().Set("Access-Control-Allow-Headers", "Cache-Control")

	flusher, ok := w.(http.Flusher)
	if !ok {
		http.Error(w, "SSE not supported", http.StatusInternalServerError)
		return
	}

	// Create new SSE client
	client := &SSEClient{
		ID:      fmt.Sprintf("%d", time.Now().UnixNano()),
		Channel: make(chan []byte, 10),
	}

	// Register client
	app.sseMutex.Lock()
	app.sseClients[client] = true
	app.sseMutex.Unlock()

	log.Printf("SSE client connected: %s, total clients: %d", client.ID, len(app.sseClients))

	// Send connection confirmation
	connectMsg := SSEMessage{
		Type: "connected",
		Data: map[string]string{
			"clientId": client.ID,
			"message":  "Connected to HeavenGate SSE",
		},
	}
	connectData, _ := json.Marshal(connectMsg)
	fmt.Fprintf(w, "data: %s\n\n", string(connectData))
	flusher.Flush()

	// Send initial data
	app.sendInitialData(client.ID, w, flusher)

	// Handle client messages
	for {
		select {
		case message := <-client.Channel:
			_, err := fmt.Fprintf(w, "data: %s\n\n", string(message))
			if err != nil {
				break
			}
			flusher.Flush()

		case <-r.Context().Done():
			app.sseMutex.Lock()
			delete(app.sseClients, client)
			app.sseMutex.Unlock()
			close(client.Channel)
			log.Printf("SSE client disconnected: %s, total clients: %d", client.ID, len(app.sseClients))
			return

		case <-time.After(30 * time.Second):
			// Send ping to keep connection alive
			pingMsg := SSEMessage{
				Type: "ping",
				Data: map[string]string{"message": "keep-alive"},
			}
			pingData, _ := json.Marshal(pingMsg)
			fmt.Fprintf(w, "data: %s\n\n", string(pingData))
			flusher.Flush()
		}
	}
}

func (app *App) sendInitialData(clientID string, w http.ResponseWriter, flusher http.Flusher) {
	app.mu.RLock()
	defer app.mu.RUnlock()

	// Prepare initial data
	initialData := SSEMessage{
		Type: "initial",
		Data: map[string]interface{}{
			"requests": app.requests,
			"agents":   app.agents,
			"stats": map[string]interface{}{
				"totalRequests":    app.stats.TotalRequests,
				"legitClients":     app.stats.LegitClients,
				"maliciousClients": app.stats.MaliciousClients,
			},
		},
	}

	initialJSON, _ := json.Marshal(initialData)
	fmt.Fprintf(w, "data: %s\n\n", string(initialJSON))
	flusher.Flush()
}

func (app *App) broadcastSSE(message SSEMessage) {
	messageJSON, err := json.Marshal(message)
	if err != nil {
		log.Printf("Error marshaling SSE message: %v", err)
		return
	}

	app.sseMutex.RLock()
	defer app.sseMutex.RUnlock()

	for client := range app.sseClients {
		select {
		case client.Channel <- messageJSON:
			// Message sent successfully
		default:
			// Channel is full, remove client
			log.Printf("SSE client channel full, removing: %s", client.ID)
			app.sseMutex.Lock()
			delete(app.sseClients, client)
			close(client.Channel)
			app.sseMutex.Unlock()
		}
	}
}

func main() {
	app := NewApp()

	router := mux.NewRouter()

	// API routes
	api := router.PathPrefix("/api").Subrouter()
	api.HandleFunc("/stats", app.handleStats).Methods("GET")
	api.HandleFunc("/requests", app.handleRequests).Methods("GET")
	api.HandleFunc("/clients", app.handleClients).Methods("GET")
	api.HandleFunc("/agents", app.handleAgents).Methods("GET")
	api.HandleFunc("/events", app.handleSSE).Methods("GET")
	
	// Balancer endpoint (for receiving requests from load balancer)
	api.HandleFunc("/balancer/request", app.handleBalancerRequest).Methods("POST")

	// Serve static files for production
	router.PathPrefix("/").Handler(http.FileServer(http.Dir("../frontend/dist/")))

	// CORS configuration
	c := cors.New(cors.Options{
		AllowedOrigins:   []string{"http://localhost:5173", "http://localhost:8080"},
		AllowedMethods:   []string{"GET", "POST", "PUT", "DELETE", "OPTIONS"},
		AllowedHeaders:   []string{"*"},
		AllowCredentials: true,
	})

	handler := c.Handler(router)

	port := ":8080"
	log.Printf("🚀 HeavenGate Dashboard server starting on port %s", port)
	log.Printf("📡 Listening for balancer requests on /api/balancer/request")
	log.Printf("🌐 Dashboard available at: http://localhost%s", port)
	log.Printf("--------------------------------------------------------")
	
	log.Fatal(http.ListenAndServe(port, handler))
}