/*
 * Filename: d:\HeavenGate\go-services\Dashboard\main.go
 * Path: d:\HeavenGate\go-services\Dashboard
 * Created Date: Saturday, November 8th 2025, 11:02:28 am
 * Author: mmonastyrskiy
 * 
 */

package main
.
import (
	"encoding/json"
	"fmt"
	"log"
	"net/http"
	"time"
)

type BalancerRequest struct {
	ClientIP    string `json:"clientIP"`
	Path        string `json:"path"`
	IsMalicious bool   `json:"IsMalicious"`
	Timestamp   string `json:"timestamp"`
	ReceivedAt  time.Time
}

var requests []BalancerRequest

func main() {
	fmt.Println("🚀 HeavenGate Dashboard started!")
	fmt.Println("📡 Listening for balancer requests on :8081")
	fmt.Println("🌐 Open http://localhost:8081 to view balancer requests")
	fmt.Println("--------------------------------------------------------")

	// API для приема запросов от балансировщика
	http.HandleFunc("/api/user_registered", func(w http.ResponseWriter, r *http.Request) {
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

		// Добавляем запрос в историю
		requests = append(requests, balancerReq)
		
		// Ограничиваем историю (последние 100 запросов)
		if len(requests) > 100 {
			requests = requests[1:]
		}

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
	})

	// Веб-интерфейс для просмотра запросов
	http.HandleFunc("/", func(w http.ResponseWriter, r *http.Request) {
		html := `
		<!DOCTYPE html>
		<html>
		<head>
		<meta charset="UTF-8">
			<title>HeavenGate Balancer Monitor</title>
			<style>
				body { font-family: Arial, sans-serif; margin: 20px; background: #f5f5f5; }
				.container { max-width: 1200px; margin: 0 auto; background: white; padding: 20px; border-radius: 10px; }
				.request { padding: 10px; margin: 5px 0; border-radius: 5px; }
				.legit { background: #d4edda; border-left: 4px solid #28a745; }
				.malicious { background: #f8d7da; border-left: 4px solid #dc3545; }
				.stats { background: #e2e3e5; padding: 15px; border-radius: 5px; margin-bottom: 20px; }
				.method { font-weight: bold; }
				.timestamp { color: #6c757d; font-size: 0.9em; }
			</style>
		</head>
		<body>
			<div class="container">
				<h1>HeavenGate Balancer Monitor</h1>
				
				<div class="stats">
					<strong>Statistics:</strong><br>
					Total Requests: <span id="total">0</span> | 
					Legit: <span id="legit">0</span> | 
					Malicious: <span id="malicious">0</span>
				</div>

				<button onclick="location.reload()">Refresh</button>
				<button onclick="clearRequests()">Clear</button>
				<hr>
				
				<div id="requests">
		`

		// Считаем статистику
		total := len(requests)
		legit := 0
		malicious := 0

		// Показываем запросы (новые сверху)
		for i := len(requests) - 1; i >= 0; i-- {
			req := requests[i]
			
			if req.IsMalicious {
				malicious++
				html += fmt.Sprintf(`
					<div class="request malicious">
						<div>🚨 <strong>MALICIOUS</strong></div>
						<div>🌐 <strong>IP:</strong> %s</div>
						<div>📁 <strong>Path:</strong> %s</div>
						<div class="timestamp">⏰ %s</div>
					</div>
				`, req.ClientIP, req.Path, req.ReceivedAt.Format("2006-01-02 15:04:05"))
			} else {
				legit++
				html += fmt.Sprintf(`
					<div class="request legit">
						<div>✅ <strong>LEGIT</strong></div>
						<div>🌐 <strong>IP:</strong> %s</div>
						<div>📁 <strong>Path:</strong> %s</div>
						<div class="timestamp">⏰ %s</div>
					</div>
				`, req.ClientIP, req.Path, req.ReceivedAt.Format("2006-01-02 15:04:05"))
			}
		}

		html += fmt.Sprintf(`
				</div>
			</div>

			<script>
				// Обновляем статистику
				document.getElementById('total').textContent = '%d';
				document.getElementById('legit').textContent = '%d';
				document.getElementById('malicious').textContent = '%d';

				function clearRequests() {
					fetch('/clear', { method: 'POST' })
						.then(() => location.reload());
				}

				// Авто-обновление каждые 3 секунды
				setInterval(() => location.reload(), 3000);
			</script>
		</body>
		</html>
		`, total, legit, malicious)

		w.Header().Set("Content-Type", "text/html")
		w.Write([]byte(html))
	})

	// Очистка запросов
	http.HandleFunc("/clear", func(w http.ResponseWriter, r *http.Request) {
		if r.Method != "POST" {
			http.Error(w, "Method not allowed", http.StatusMethodNotAllowed)
			return
		}
		requests = []BalancerRequest{}
		w.Write([]byte("Cleared"))
	})

	// Запуск сервера
	log.Fatal(http.ListenAndServe(":8081", nil))
}
