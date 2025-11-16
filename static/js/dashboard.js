        // SSE соединение
        let eventSource;
        let reconnectTimeout;
        const maxReconnectDelay = 10000;

        // Инициализация графика с фиксированными осями
        const ctx = document.getElementById('activity-chart').getContext('2d');
        const activityChart = new Chart(ctx, {
            type: 'line',
            data: {
                labels: [],
                datasets: [
                    {
                        label: 'Легитимные клиенты',
                        data: [],
                        borderColor: '#2ecc71',
                        backgroundColor: 'rgba(46, 204, 113, 0.1)',
                        tension: 0.4,
                        fill: true,
                        borderWidth: 2
                    },
                    {
                        label: 'Вредоносные клиенты',
                        data: [],
                        borderColor: '#e74c3c',
                        backgroundColor: 'rgba(231, 76, 60, 0.1)',
                        tension: 0.4,
                        fill: true,
                        borderWidth: 2
                    }
                ]
            },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                plugins: {
                    legend: {
                        position: 'top',
                    }
                },
                scales: {
                    x: {
                        display: true,
                        title: {
                            display: true,
                            text: 'Время'
                        },
                        grid: {
                            display: true
                        }
                    },
                    y: {
                        display: true,
                        title: {
                            display: true,
                            text: 'Количество клиентов'
                        },
                        beginAtZero: true,
                        suggestedMin: 0,
                        suggestedMax: 10, // Фиксированный максимум для начала
                        grid: {
                            display: true
                        },
                        ticks: {
                            stepSize: 1 // Целые числа для количества клиентов
                        }
                    }
                },
                animation: {
                    duration: 0 // Отключаем анимацию для производительности
                },
                elements: {
                    point: {
                        radius: 0 // Убираем точки для чистоты
                    }
                }
            }
        });

        // Массив для хранения запросов и клиентов
        let requests = [];
        let clients = new Map(); // IP -> {isMalicious, lastSeen, requestCount}
        let agents = {
            realServers: 3, // Пример начальных данных
            honeypots: 5
        };

        // Данные для графика (фиксированные интервалы)
        let chartData = {
            labels: generateTimeLabels(),
            legit: Array(24).fill(0),
            malicious: Array(24).fill(0)
        };

        // Генерация временных меток для последних 24 часов
        function generateTimeLabels() {
            const labels = [];
            const now = new Date();
            
            for (let i = 23; i >= 0; i--) {
                const time = new Date(now);
                time.setHours(now.getHours() - i);
                labels.push(time.toLocaleTimeString([], {hour: '2-digit', minute: '2-digit'}));
            }
            
            return labels;
        }

        // Функция подключения SSE
        function connectSSE() {
            try {
                eventSource = new EventSource('/events');
                
                eventSource.onopen = function(event) {
                    console.log('SSE connection opened');
                    updateConnectionStatus(true);
                    clearTimeout(reconnectTimeout);
                };

                eventSource.onmessage = function(event) {
                    try {
                        const data = JSON.parse(event.data);
                        handleSSEMessage(data);
                    } catch (error) {
                        console.error('Error parsing SSE message:', error);
                    }
                };

                eventSource.addEventListener('connected', function(event) {
                    const data = JSON.parse(event.data);
                    console.log('SSE connected with client ID:', data.clientId);
                });

                eventSource.onerror = function(event) {
                    console.error('SSE error:', event);
                    updateConnectionStatus(false);
                    
                    if (eventSource) {
                        eventSource.close();
                    }
                    
                    attemptReconnect();
                };

            } catch (error) {
                console.error('Error creating SSE connection:', error);
                attemptReconnect();
            }
        }

        // Попытка переподключения
        function attemptReconnect(delay = 1000) {
            console.log(`Attempting to reconnect in ${delay}ms...`);
            
            clearTimeout(reconnectTimeout);
            reconnectTimeout = setTimeout(() => {
                connectSSE();
            }, Math.min(delay * 2, maxReconnectDelay));
        }


        function handleClientsUpdate(clientsData) {
    console.log("Clients update received:", clientsData);
    
    // Обновляем статистику клиентов из полученных данных
    updateClientsStats(clientsData);
            }

        // Обработчик сообщений SSE
        function handleSSEMessage(data) {
            switch (data.type) {
                case 'initial':
    console.log('Received initial data');
    requests = data.data.requests || [];
    agents = data.data.agents || agents;
    
    // Восстанавливаем локальные данные клиентов из запросов
    updateClientsFromRequests();
    
    // Но статистику берем из серверных данных, а не вычисляем локально
    if (data.data.legitClients !== undefined && data.data.maliciousClients !== undefined) {
        updateClientsStats({
            legitClients: data.data.legitClients,
            maliciousClients: data.data.maliciousClients
        });
    } else {
        // На всякий случай, если в initial данных нет статистики
        updateClientsStats();
    }
    
    updateRequestsStats();
    updateAgentsStats();
    updateChartData();
    updateRequestsTable();
    break;
                    
                case 'agents_update':
                    console.log('Agents update received:', data.data);
                    agents = data.data;
                    updateAgentsStats();
                    break;
                    
                case 'ping':
                    break;
                case 'clients_update':
                    console.log("Clients update recieved:", data.data);
                    clients = data;
                    updateClientsStats()
                    break;
                    
                default:
                    console.log('Unknown message type:', data.type);
            }
        }

        // Обновление информации о клиентах из всех запросов
        function updateClientsFromRequests() {
            clients.clear();
            requests.forEach(request => {
                updateClientInfo(request);
            });
        }

        // Обновление информации о конкретном клиенте
        function updateClientInfo(request) {
            const clientIP = request.clientIP;
            const now = new Date();
            
            if (!clients.has(clientIP)) {
                clients.set(clientIP, {
                    isMalicious: request.isMalicious,
                    firstSeen: now,
                    lastSeen: now,
                    requestCount: 1
                });
            } else {
                const client = clients.get(clientIP);
                client.lastSeen = now;
                client.requestCount++;
                // Если клиент стал вредоносным, помечаем его как вредоносного
                if (request.isMalicious) {
                    client.isMalicious = true;
                }
            }
        }

        // Обновление всей статистики
        function updateAllStats() {
            updateRequestsStats();
            updateClientsStats();
            updateAgentsStats();
        }

        // Обновление статистики запросов
        function updateRequestsStats() {
            document.getElementById('total-requests').textContent = requests.length;
        }

        // Обновление статистики клиентов
    function updateClientsStats(clientsData) {
    // Просто устанавливаем значения из полученных данных
    document.getElementById('legit-clients').textContent = clientsData.legitClients || 0;
    document.getElementById('malicious-clients').textContent = clientsData.maliciousClients || 0;
}  

        // Обновление статистики агентов
        function updateAgentsStats() {
            document.getElementById('real-servers').textContent = agents.realServers || 0;
            document.getElementById('honeypots').textContent = agents.honeypots || 0;
        }

        // Функция обновления статуса соединения
        function updateConnectionStatus(connected) {
            const statusElement = document.getElementById('connection-status') || createConnectionStatusElement();
            statusElement.textContent = connected ? '🟢 Connected' : '🔴 Disconnected';
            statusElement.style.color = connected ? '#2ecc71' : '#e74c3c';
        }

        // Создание элемента статуса соединения
        function createConnectionStatusElement() {
            const statusElement = document.createElement('div');
            statusElement.id = 'connection-status';
            statusElement.style.marginLeft = '20px';
            statusElement.style.fontSize = '0.9rem';
            statusElement.style.fontWeight = 'bold';
            document.querySelector('.header-left').appendChild(statusElement);
            return statusElement;
        }

        // Обновление данных графика
        function updateChartData() {
            // Сбрасываем данные графика
            chartData.legit = Array(24).fill(0);
            chartData.malicious = Array(24).fill(0);
            
            // Обновляем график
            activityChart.data.labels = chartData.labels;
            activityChart.data.datasets[0].data = chartData.legit;
            activityChart.data.datasets[1].data = chartData.malicious;
            activityChart.update('none');
        }

        // Обновление графика при новом запросе
        function updateChartWithNewRequest(request) {
            const requestTime = new Date(request.receivedAt);
            const now = new Date();
            
            // Находим соответствующий временной интервал (текущий час)
            const currentHour = now.getHours();
            const requestHour = requestTime.getHours();
            
            // Определяем индекс в массиве данных (0-23)
            let index = (requestHour - (currentHour - 23) + 24) % 24;
            
            if (index >= 0 && index < 24) {
                // Обновляем счетчик для соответствующего типа клиента
                const client = clients.get(request.clientIP);
                if (client) {
                    if (client.isMalicious) {
                        chartData.malicious[index] = Math.max(chartData.malicious[index], 
                            Array.from(clients.values()).filter(c => c.isMalicious).length);
                    } else {
                        chartData.legit[index] = Math.max(chartData.legit[index],
                            Array.from(clients.values()).filter(c => !c.isMalicious).length);
                    }
                }
                
                // Автоматически подстраиваем максимальное значение оси Y
                const maxValue = Math.max(...chartData.legit, ...chartData.malicious);
                activityChart.options.scales.y.suggestedMax = Math.max(10, maxValue + 2);
                
                // Обновляем график
                activityChart.data.datasets[0].data = chartData.legit;
                activityChart.data.datasets[1].data = chartData.malicious;
                activityChart.update('none');
            }
        }

        // Обновление таблицы запросов
        function updateRequestsTable() {
            const tableBody = document.getElementById('requests-table-body');
            tableBody.innerHTML = '';
            
            // Показываем последние 20 запросов
            const recentRequests = requests.slice(-20).reverse();
            
            recentRequests.forEach(request => {
                addRequestToTable(request, tableBody);
            });
        }

        // Добавление нового запроса в таблицу
        function addNewRequestToTable(request) {
            const tableBody = document.getElementById('requests-table-body');
            
            // Добавляем новую строку в начало таблицы
            addRequestToTable(request, tableBody);
            
            // Удаляем старые строки если больше 20
            const rows = tableBody.getElementsByTagName('tr');
            if (rows.length > 20) {
                tableBody.removeChild(rows[rows.length - 1]);
            }
            
            // Добавляем анимацию подсветки новой строки
            const newRow = tableBody.firstChild;
            newRow.style.backgroundColor = 'rgba(52, 152, 219, 0.1)';
            setTimeout(() => {
                newRow.style.backgroundColor = '';
            }, 2000);
        }

        // Добавление запроса в таблицу
        function addRequestToTable(request, tableBody) {
            const row = document.createElement('tr');
            const statusClass = request.isMalicious ? 'status-malicious' : 'status-legit';
            const statusText = request.isMalicious ? '🚨 MALICIOUS' : '✅ LEGIT';
            const receivedAt = new Date(request.receivedAt).toLocaleString();
            
            row.innerHTML = `
                <td><span class="status-badge ${statusClass}">${statusText}</span></td>
                <td>${request.clientIP}</td>
                <td>${request.path}</td>
                <td>${receivedAt}</td>
            `;
            
            // Добавляем анимацию появления
            row.style.opacity = '0';
            row.style.transform = 'translateY(-10px)';
            row.style.transition = 'all 0.3s ease';
            
            tableBody.insertBefore(row, tableBody.firstChild);
            
            // Запускаем анимацию
            setTimeout(() => {
                row.style.opacity = '1';
                row.style.transform = 'translateY(0)';
            }, 10);
        }

        // Обработчики для выпадающих меню
        document.getElementById('notification-dropdown').addEventListener('click', function(e) {
            e.stopPropagation();
            document.getElementById('notification-menu').classList.toggle('show');
        });

        document.getElementById('user-dropdown').addEventListener('click', function(e) {
            e.stopPropagation();
            document.getElementById('user-menu').classList.toggle('show');
        });

        // Закрытие выпадающих меню при клике вне их
        document.addEventListener('click', function() {
            document.querySelectorAll('.dropdown-content').forEach(menu => {
                menu.classList.remove('show');
            });
        });

        // Обработчики для навигации
        document.querySelectorAll('.nav-links a').forEach(link => {
            link.addEventListener('click', function(e) {
                e.preventDefault();
                
                document.querySelectorAll('.nav-links a').forEach(item => {
                    item.classList.remove('active');
                });
                
                this.classList.add('active');
                
                const pageTitle = this.querySelector('span').textContent;
                document.getElementById('page-title').textContent = pageTitle;
            });
        });

        // Инициализация при загрузке страницы
        document.addEventListener('DOMContentLoaded', function() {
            // Инициализируем график
            updateChartData();
            
            // Подключаемся к SSE
            connectSSE();
        });

        // Обработка перед закрытием страницы
        window.addEventListener('beforeunload', function() {
            if (eventSource) {
                eventSource.close();
            }
            clearTimeout(reconnectTimeout);
        });