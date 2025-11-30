package main

import (
	"encoding/json"
	"fmt"
	"log"
	"net/http"
	"os"
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
	agents     = AgentsInfo{}
	mu         sync.Mutex
	sseClients = make(map[*SSEClient]bool)
	sseMutex   sync.Mutex
	
	// Настройки логирования
	loggingEnabled = true // Переключите на false чтобы отключить логирование
	logFile        *os.File
	logger         *log.Logger
)

// Инициализация логирования
func initLogging() error {
	if !loggingEnabled {
		return nil
	}
	
	var err error
	logFile, err = os.OpenFile("heavengate_dashboard.log", os.O_CREATE|os.O_WRONLY|os.O_APPEND, 0666)
	if err != nil {
		return fmt.Errorf("failed to open log file: %v", err)
	}
	
	logger = log.New(logFile, "", log.LstdFlags)
	logger.Println("=== HeavenGate Dashboard Log Started ===")
	return nil
}

// Функция логирования
func logMessage(level, message string) {
	if !loggingEnabled || logger == nil {
		return
	}
	
	timestamp := time.Now().Format("2006-01-02 15:04:05.000")
	logger.Printf("[%s] %s: %s\n", timestamp, level, message)
}

// Логирование с форматированием
func logf(level, format string, args ...interface{}) {
	if !loggingEnabled || logger == nil {
		return
	}
	
	message := fmt.Sprintf(format, args...)
	logMessage(level, message)
}

func main() {
	// Инициализация логирования
	if err := initLogging(); err != nil {
		log.Printf("Warning: Logging initialization failed: %v", err)
	} else if loggingEnabled {
		defer logFile.Close()
	}

	fmt.Println("🚀 HeavenGate Dashboard started!")
	fmt.Println("📡 Listening for balancer requests on :8081")
	fmt.Println("🌐 Open http://localhost:8081 to view balancer requests")
	if loggingEnabled {
		fmt.Println("📝 Logging enabled: logs are being written to heavengate_dashboard.log")
	} else {
		fmt.Println("📝 Logging disabled")
	}
	fmt.Println("--------------------------------------------------------")

	logf("INFO", "HeavenGate Dashboard server starting on port 8081")

	// Обслуживание статических файлов
	http.Handle("/", http.FileServer(http.Dir("../../static")))

	// SSE endpoint для real-time обновлений
	http.HandleFunc("/events", handleSSE)

	// API для приема запросов от балансировщика
	http.HandleFunc("/api/req_registered", handleBalancerRequest)

	// API для получения истории запросов
	http.HandleFunc("/api/user_registered", getUserUpdate)

	// API для обновления информации об агентах
	http.HandleFunc("/api/agents", handleAgentsUpdate)

	// Запуск сервера
	logf("INFO", "Starting HTTP server on :8081")
	log.Fatal(http.ListenAndServe(":8081", nil))
}

func handleBalancerRequest(w http.ResponseWriter, r *http.Request) {
	if r.Method != "POST" {
		http.Error(w, "Method not allowed", http.StatusMethodNotAllowed)
		logf("WARNING", "Invalid method %s for /api/req_registered from %s", r.Method, r.RemoteAddr)
		return
	}

	var balancerReq BalancerRequest
	if err := json.NewDecoder(r.Body).Decode(&balancerReq); err != nil {
		http.Error(w, "Invalid JSON", http.StatusBadRequest)
		logf("ERROR", "Failed to decode JSON from %s: %v", r.RemoteAddr, err)
		return
	}

	balancerReq.ReceivedAt = time.Now()

	mu.Lock()
	// Добавляем запрос в историю
	requests = append(requests, balancerReq)
	
	// Ограничиваем историю
	if len(requests) > 1000 {
		removed := requests[0]
		requests = requests[1:]
		logf("DEBUG", "Removed oldest request from history: %s", removed.ClientIP)
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

	// Логируем в консоль и файл
	status := "✅ LEGIT"
	if balancerReq.IsMalicious {
		status = "🚨 MALICIOUS"
	}
	
	consoleMessage := fmt.Sprintf("%s | %s | %s | %s", 
		status, 
		balancerReq.ClientIP, 
		balancerReq.Path, 
		balancerReq.ReceivedAt.Format("15:04:05"))
	
	fmt.Println(consoleMessage)
	
	// Логируем в файл с дополнительной информацией
	logLevel := "INFO"
	if balancerReq.IsMalicious {
		logLevel = "WARNING"
	}
	logf(logLevel, "Request: %s IP: %s Path: %s Remote: %s", 
		status, balancerReq.ClientIP, balancerReq.Path, r.RemoteAddr)

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
			if !client.IsMalicious {
				logf("SECURITY", "Client %s marked as MALICIOUS", clientIP)
			}
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
		logf("INFO", "New client registered: %s (Malicious: %v)", clientIP, request.IsMalicious)
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

func getUserUpdate(w http.ResponseWriter, r *http.Request) {
	if r.Method != "POST" {
		http.Error(w, "Method not allowed", http.StatusMethodNotAllowed)
		logf("WARNING", "Invalid method %s for /api/user_registered from %s", r.Method, r.RemoteAddr)
		return
	}

	// Декодируем JSON из тела запроса
	var requestData struct {
		LegitClients     int `json:"legitClients"`
		MaliciousClients int `json:"maliciousClients"`
	}

	if err := json.NewDecoder(r.Body).Decode(&requestData); err != nil {
		http.Error(w, "Invalid JSON", http.StatusBadRequest)
		logf("ERROR", "Failed to decode user update JSON from %s: %v", r.RemoteAddr, err)
		return
	}
	defer r.Body.Close()

	logf("INFO", "User update received: legit=%d, malicious=%d", 
		requestData.LegitClients, requestData.MaliciousClients)

	// Отправляем данные через SSE
	broadcastToSSEClients("agents_update", map[string]interface{}{
		"agents": map[string]interface{}{
			"legitClients":     requestData.LegitClients,
			"maliciousClients": requestData.MaliciousClients,
		},
	})

	// Возвращаем успешный ответ
	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(http.StatusOK)
	json.NewEncoder(w).Encode(map[string]interface{}{
		"status":  "success",
		"message": "Data received and broadcasted",
		"data": map[string]interface{}{
			"legitClients":     requestData.LegitClients,
			"maliciousClients": requestData.MaliciousClients,
		},
	})
}

func handleAgentsUpdate(w http.ResponseWriter, r *http.Request) {
	if r.Method != "POST" {
		http.Error(w, "Method not allowed", http.StatusMethodNotAllowed)
		logf("WARNING", "Invalid method %s for /api/agents from %s", r.Method, r.RemoteAddr)
		return
	}

	var newAgents AgentsInfo
	if err := json.NewDecoder(r.Body).Decode(&newAgents); err != nil {
		http.Error(w, "Invalid JSON", http.StatusBadRequest)
		logf("ERROR", "Failed to decode agents update JSON from %s: %v", r.RemoteAddr, err)
		return
	}

	mu.Lock()
	oldAgents := agents
	agents = newAgents
	mu.Unlock()

	logf("INFO", "Agents updated: real_servers=%d->%d, honeypots=%d->%d", 
		oldAgents.RealServers, agents.RealServers, 
		oldAgents.Honeypots, agents.Honeypots)

	// Отправляем обновление всем SSE клиентам
	broadcastToSSEClients("agents_update", map[string]interface{}{
		"agents": agents,
	})

	w.WriteHeader(http.StatusOK)
	json.NewEncoder(w).Encode(map[string]string{"status": "updated"})
}

func handleSSE(w http.ResponseWriter, r *http.Request) {
	// Устанавливаем заголовки для SSE
	w.Header().Set("Content-Type", "text/event-stream")
	w.Header().Set("Cache-Control", "no-cache")
	w.Header().Set("Connection", "keep-alive")
	w.Header().Set("Access-Control-Allow-Origin", "*")
	w.Header().Set("Access-Control-Allow-Headers", "Cache-Control")

	// Создаем флашер для принудительной отправки данных
	flusher, ok := w.(http.Flusher)
	if !ok {
		http.Error(w, "SSE not supported", http.StatusInternalServerError)
		logf("ERROR", "SSE not supported for client %s", r.RemoteAddr)
		return
	}

	// Создаем нового клиента
	client := &SSEClient{
		ID:      fmt.Sprintf("%d", time.Now().UnixNano()),
		Channel: make(chan []byte, 10), // Буферизованный канал
	}

	// Регистрируем клиента
	sseMutex.Lock()
	sseClients[client] = true
	currentClients := len(sseClients)
	sseMutex.Unlock()

	logf("INFO", "SSE client connected: %s from %s, total clients: %d", 
		client.ID, r.RemoteAddr, currentClients)

	// Уведомляем о подключении
	fmt.Fprintf(w, "event: connected\ndata: {\"clientId\": \"%s\"}\n\n", client.ID)
	flusher.Flush()

	// Отправляем начальные данные
	mu.Lock()
	
	legitClients := 0
	maliciousClients := 0
	for _, client := range clients {
		if client.IsMalicious {
			maliciousClients++
		} else {
			legitClients++
		}
	}
	
	initialData := map[string]interface{}{
		"type": "initial",
		"data": map[string]interface{}{
			"requests":        requests,
			"total":           len(requests),
			"legitClients":    legitClients,
			"maliciousClients": maliciousClients,
			"agents":          agents,
		},
	}
	mu.Unlock()

	initialDataJSON, _ := json.Marshal(initialData)
	fmt.Fprintf(w, "data: %s\n\n", string(initialDataJSON))
	flusher.Flush()

	logf("DEBUG", "Sent initial data to SSE client %s", client.ID)

	// Обрабатываем сообщения для этого клиента
	for {
		select {
		case message := <-client.Channel:
			// Отправляем сообщение клиенту
			_, err := fmt.Fprintf(w, "data: %s\n\n", string(message))
			if err != nil {
				// Клиент отключился
				logf("DEBUG", "Error sending to client %s: %v", client.ID, err)
				break
			}
			flusher.Flush()

		case <-r.Context().Done():
			// Клиент отключился
			sseMutex.Lock()
			delete(sseClients, client)
			remainingClients := len(sseClients)
			sseMutex.Unlock()
			close(client.Channel)
			logf("INFO", "SSE client disconnected: %s, remaining clients: %d", 
				client.ID, remainingClients)
			return

		case <-time.After(30 * time.Second):
			// Отправляем ping для поддержания соединения
			pingData := map[string]interface{}{
				"type":    "ping",
				"message": "keep-alive",
			}
			pingJSON, _ := json.Marshal(pingData)
			fmt.Fprintf(w, "data: %s\n\n", string(pingJSON))
			flusher.Flush()
			logf("DEBUG", "Sent ping to SSE client %s", client.ID)
		}
	}
}

func broadcastToSSEClients(eventType string, data interface{}) {
	message := map[string]interface{}{
		"type": eventType,
		"data": data,
	}

	messageJSON, err := json.Marshal(message)
	if err != nil {
		logf("ERROR", "Error marshaling SSE message: %v", err)
		return
	}

	sseMutex.Lock()
	defer sseMutex.Unlock()

	clientsCount := len(sseClients)
	skippedClients := 0

	for client := range sseClients {
		select {
		case client.Channel <- messageJSON:
			// Сообщение отправлено
		default:
			// Канал заполнен, пропускаем этого клиента
			skippedClients++
			logf("WARNING", "SSE client channel full, skipping: %s", client.ID)
		}
	}
	
	if clientsCount > 0 {
		logf("DEBUG", "Broadcasted %s to %d clients (%d skipped)", 
			eventType, clientsCount-skippedClients, skippedClients)
	}
}