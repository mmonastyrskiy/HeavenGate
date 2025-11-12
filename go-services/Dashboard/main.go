package main

import (
	"encoding/json"
	"fmt"
	"log"
	"net/http"
	"sync"
	"time"
)

type BalancerRequest struct {
	ClientIP    string    `json:"clientIP"`
	Path        string    `json:"path"`
	IsMalicious bool      `json:"isMalicious"`
	Timestamp   string    `json:"timestamp"`
	ReceivedAt  time.Time `json:"receivedAt"`
}

type ClientInfo struct {
	IP          string    `json:"ip"`
	IsMalicious bool      `json:"isMalicious"`
	FirstSeen   time.Time `json:"firstSeen"`
	LastSeen    time.Time `json:"lastSeen"`
	RequestCount int      `json:"requestCount"`
}

type AgentsInfo struct {
	RealServers int `json:"realServers"`
	Honeypots   int `json:"honeypots"`
}

type SSEClient struct {
	ID      string
	Channel chan []byte
}

var (
	requests   []BalancerRequest
	clients    = make(map[string]*ClientInfo)
	agents     = AgentsInfo{RealServers: 3, Honeypots: 5}
	mu         sync.Mutex
	sseClients = make(map[*SSEClient]bool)
	sseMutex   sync.Mutex
)

func main() {
	fmt.Println("🚀 HeavenGate Dashboard started!")
	fmt.Println("📡 Listening for balancer requests on :8081")
	fmt.Println("🌐 Open http://localhost:8081 to view balancer requests")
	fmt.Println("--------------------------------------------------------")

	// Обслуживание статических файлов
	http.Handle("/", http.FileServer(http.Dir("./static")))

	// SSE endpoint для real-time обновлений
	http.HandleFunc("/events", handleSSE)

	// API для приема запросов от балансировщика
	http.HandleFunc("/api/user_registered", handleBalancerRequest)

	// API для получения истории запросов
	http.HandleFunc("/user_registered", getRequestsHistory)

	// API для обновления информации об агентах
	http.HandleFunc("/api/agents", handleAgentsUpdate)

	// Запуск сервера
	log.Fatal(http.ListenAndServe(":8081", nil))
}

func handleBalancerRequest(w http.ResponseWriter, r *http.Request) {
	if r.Method != "POST" {
		http.Error(w, "Method not allowed", http.StatusMethodNotAllowed)
		return
	}

	var balancerReq BalancerRequest
	if err := json.NewDecoder(r.Body).Decode(&balancerReq); err != nil {
		http.Error(w, "Invalid JSON", http.StatusBadRequest)
		return
	}

	balancerReq.ReceivedAt = time.Now()

	mu.Lock()
	// Добавляем запрос в историю
	requests = append(requests, balancerReq)
	
	// Ограничиваем историю
	if len(requests) > 1000 {
		requests = requests[1:]
	}
	
	// Обновляем информацию о клиенте
	updateClientInfo(balancerReq)
	
	// Подготавливаем данные для статистики
	stats := prepareStats()
	mu.Unlock()

	// Отправляем новое сообщение всем SSE клиентам
	broadcastToSSEClients("new_request", map[string]interface{}{
		"request": balancerReq,
		"stats":   stats,
	})

	// Логируем в консоль
	status := "✅ LEGIT"
	if balancerReq.IsMalicious {
		status = "🚨 MALICIOUS"
	}
	fmt.Printf("%s | %s | %s | %s\n", 
		status, 
		balancerReq.ClientIP, 
		balancerReq.Path, 
		balancerReq.ReceivedAt.Format("15:04:05"))

	w.WriteHeader(http.StatusOK)
	json.NewEncoder(w).Encode(map[string]string{"status": "received"})
}

func updateClientInfo(request BalancerRequest) {
	clientIP := request.ClientIP
	now := time.Now()
	
	if client, exists := clients[clientIP]; exists {
		client.LastSeen = now
		client.RequestCount++
		// Если запрос вредоносный, помечаем клиента как вредоносного
		if request.IsMalicious {
			client.IsMalicious = true
		}
	} else {
		clients[clientIP] = &ClientInfo{
			IP:          clientIP,
			IsMalicious: request.IsMalicious,
			FirstSeen:   now,
			LastSeen:    now,
			RequestCount: 1,
		}
	}
}

func prepareStats() map[string]interface{} {
	legitClients := 0
	maliciousClients := 0
	
	for _, client := range clients {
		if client.IsMalicious {
			maliciousClients++
		} else {
			legitClients++
		}
	}
	
	return map[string]interface{}{
		"totalRequests":    len(requests),
		"legitClients":     legitClients,
		"maliciousClients": maliciousClients,
		"agents":           agents,
	}
}

func handleAgentsUpdate(w http.ResponseWriter, r *http.Request) {
	if r.Method != "POST" {
		http.Error(w, "Method not allowed", http.StatusMethodNotAllowed)
		return
	}

	var newAgents AgentsInfo
	if err := json.NewDecoder(r.Body).Decode(&newAgents); err != nil {
		http.Error(w, "Invalid JSON", http.StatusBadRequest)
		return
	}

	mu.Lock()
	agents = newAgents
	mu.Unlock()

	// Отправляем обновление всем SSE клиентам
	broadcastToSSEClients("agents_update", map[string]interface{}{
		"agents": agents,
	})

	w.WriteHeader(http.StatusOK)
	json.NewEncoder(w).Encode(map[string]string{"status": "updated"})
}

// Остальные функции (getRequestsHistory, handleSSE, broadcastToSSEClients) 
// остаются аналогичными предыдущей реализации, но обновляются для работы 
// с новой структурой данных