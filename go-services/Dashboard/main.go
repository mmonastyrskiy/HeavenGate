/*
 * Filename: d:\HeavenGate\go-services\Dashboard\main.go
 * Path: d:\HeavenGate\go-services\Dashboard
 * Created Date: Saturday, November 8th 2025, 11:02:28 am
 * Author: mmonastyrskiy
 * 
 */

package main

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
	http.Handle("/", http.FileServer(http.Dir("../../static")))

	// Запуск сервера
	log.Fatal(http.ListenAndServe(":8081", nil))
}
