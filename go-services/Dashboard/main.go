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

type SSEClient struct {
	ID      string
	Channel chan []byte
}

var (
	requests   []BalancerRequest
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
	
	// Ограничиваем историю (последние 1000 запросов)
	if len(requests) > 1000 {
		requests = requests[1:]
	}
	
	// Подготавливаем данные для статистики
	total := len(requests)
	legit := countLegitRequests()
	malicious := countMaliciousRequests()
	mu.Unlock()

	// Отправляем новое сообщение всем SSE клиентам
	broadcastToSSEClients("new_request", map[string]interface{}{
		"request": balancerReq,
		"stats": map[string]interface{}{
			"total":     total,
			"legit":     legit,
			"malicious": malicious,
		},
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

func getRequestsHistory(w http.ResponseWriter, r *http.Request) {
	if r.Method != "GET" {
		http.Error(w, "Method not allowed", http.StatusMethodNotAllowed)
		return
	}

	mu.Lock()
	defer mu.Unlock()

	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(map[string]interface{}{
		"requests":  requests,
		"total":     len(requests),
		"legit":     countLegitRequests(),
		"malicious": countMaliciousRequests(),
	})
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
	sseMutex.Unlock()

	log.Printf("SSE client connected: %s, total clients: %d", client.ID, len(sseClients))

	// Уведомляем о подключении
	fmt.Fprintf(w, "event: connected\ndata: {\"clientId\": \"%s\"}\n\n", client.ID)
	flusher.Flush()

	// Отправляем начальные данные
	mu.Lock()
	initialData := map[string]interface{}{
		"type": "initial",
		"data": map[string]interface{}{
			"requests":  requests,
			"total":     len(requests),
			"legit":     countLegitRequests(),
			"malicious": countMaliciousRequests(),
		},
	}
	mu.Unlock()

	initialDataJSON, _ := json.Marshal(initialData)
	fmt.Fprintf(w, "data: %s\n\n", string(initialDataJSON))
	flusher.Flush()

	// Обрабатываем сообщения для этого клиента
	for {
		select {
		case message := <-client.Channel:
			// Отправляем сообщение клиенту
			_, err := fmt.Fprintf(w, "data: %s\n\n", string(message))
			if err != nil {
				// Клиент отключился
				break
			}
			flusher.Flush()

		case <-r.Context().Done():
			// Клиент отключился
			sseMutex.Lock()
			delete(sseClients, client)
			sseMutex.Unlock()
			close(client.Channel)
			log.Printf("SSE client disconnected: %s, total clients: %d", client.ID, len(sseClients))
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
		log.Printf("Error marshaling SSE message: %v", err)
		return
	}

	sseMutex.Lock()
	defer sseMutex.Unlock()

	for client := range sseClients {
		select {
		case client.Channel <- messageJSON:
			// Сообщение отправлено
		default:
			// Канал заполнен, пропускаем этого клиента
			log.Printf("SSE client channel full, skipping: %s", client.ID)
		}
	}
}

func countLegitRequests() int {
	count := 0
	for _, req := range requests {
		if !req.IsMalicious {
			count++
		}
	}
	return count
}

func countMaliciousRequests() int {
	count := 0
	for _, req := range requests {
		if req.IsMalicious {
			count++
		}
	}
	return count
}