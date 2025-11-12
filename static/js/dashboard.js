
    // SSE соединение
    let eventSource;
    let reconnectTimeout;
    const maxReconnectDelay = 10000; // 10 секунд

    // Инициализация графика
    const ctx = document.getElementById('activity-chart').getContext('2d');
    const activityChart = new Chart(ctx, {
        type: 'line',
        data: {
            labels: [],
            datasets: [
                {
                    label: 'Легитимные запросы',
                    data: [],
                    borderColor: '#2ecc71',
                    backgroundColor: 'rgba(46, 204, 113, 0.1)',
                    tension: 0.4,
                    fill: true,
                    borderWidth: 2
                },
                {
                    label: 'Вредоносные запросы',
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
                    }
                },
                y: {
                    display: true,
                    title: {
                        display: true,
                        text: 'Количество запросов'
                    },
                    beginAtZero: true
                }
            }
        }
    });

    // Массив для хранения запросов
    let requests = [];
    let chartData = {
        labels: [],
        legit: [],
        malicious: []
    };

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
                
                // Закрываем соединение и пытаемся переподключиться
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

    // Попытка переподключения с экспоненциальной задержкой
    function attemptReconnect(delay = 1000) {
        console.log(`Attempting to reconnect in ${delay}ms...`);
        
        clearTimeout(reconnectTimeout);
        reconnectTimeout = setTimeout(() => {
            connectSSE();
        }, Math.min(delay * 2, maxReconnectDelay));
    }

    // Обработчик сообщений SSE
    function handleSSEMessage(data) {
        switch (data.type) {
            case 'initial':
                console.log('Received initial data:', data.data.requests.length, 'requests');
                requests = data.data.requests || [];
                updateStats(data.data);
                updateChartData();
                updateRequestsTable();
                break;
                
            case 'new_request':
                console.log('New request received:', data.data.request);
                // Добавляем новый запрос
                requests.push(data.data.request);
                
                // Обновляем статистику
                updateStats(data.data.stats);
                
                // Обновляем таблицу
                addNewRequestToTable(data.data.request);
                
                // Обновляем график
                updateChartWithNewRequest(data.data.request);
                break;
                
            case 'ping':
                // Игнорируем ping сообщения
                break;
                
            default:
                console.log('Unknown message type:', data.type);
        }
    }

    // Функция обновления статистики
    function updateStats(stats) {
        document.getElementById('total-requests').textContent = stats.total || requests.length;
        document.getElementById('legit-requests').textContent = stats.legit || countLegitRequests();
        document.getElementById('malicious-requests').textContent = stats.malicious || countMaliciousRequests();
    }

    // Функция обновления статуса соединения
    function updateConnectionStatus(connected) {
        const statusElement = document.getElementById('connection-status') || createConnectionStatusElement();
        statusElement.textContent = connected ? '🟢 SSE Connected' : '🔴 SSE Disconnected';
        statusElement.style.color = connected ? '#2ecc71' : '#e74c3c';
        
        // Добавляем индикатор последнего обновления
        if (connected) {
            const updateElement = document.getElementById('last-update') || createLastUpdateElement();
            updateElement.textContent = 'Last update: ' + new Date().toLocaleTimeString();
        }
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

    // Создание элемента последнего обновления
    function createLastUpdateElement() {
        const updateElement = document.createElement('div');
        updateElement.id = 'last-update';
        updateElement.style.marginLeft = '20px';
        updateElement.style.fontSize = '0.8rem';
        updateElement.style.color = '#7f8c8d';
        document.querySelector('.header-left').appendChild(updateElement);
        return updateElement;
    }

    // Подсчет легитимных запросов
    function countLegitRequests() {
        return requests.filter(req => !req.isMalicious).length;
    }

    // Подсчет вредоносных запросов
    function countMaliciousRequests() {
        return requests.filter(req => req.isMalicious).length;
    }

    // Обновление данных графика
    function updateChartData() {
        // Группируем запросы по времени (последние 12 часов)
        const now = new Date();
        chartData = {
            labels: [],
            legit: [],
            malicious: []
        };
        
        // Создаем временные интервалы (последние 12 часов)
        for (let i = 11; i >= 0; i--) {
            const time = new Date(now);
            time.setHours(now.getHours() - i);
            chartData.labels.push(time.toLocaleTimeString([], {hour: '2-digit', minute: '2-digit'}));
            
            const hourStart = new Date(time);
            hourStart.setMinutes(0, 0, 0);
            
            const hourEnd = new Date(time);
            hourEnd.setMinutes(59, 59, 999);
            
            const hourRequests = requests.filter(req => {
                const reqTime = new Date(req.receivedAt);
                return reqTime >= hourStart && reqTime <= hourEnd;
            });
            
            chartData.legit.push(hourRequests.filter(req => !req.isMalicious).length);
            chartData.malicious.push(hourRequests.filter(req => req.isMalicious).length);
        }
        
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
        
        // Находим соответствующий временной интервал
        const hourIndex = Math.floor((now - requestTime) / (60 * 60 * 1000));
        
        if (hourIndex >= 0 && hourIndex < chartData.legit.length) {
            const index = chartData.legit.length - 1 - hourIndex;
            
            if (request.isMalicious) {
                chartData.malicious[index]++;
            } else {
                chartData.legit[index]++;
            }
            
            // Обновляем график
            activityChart.data.datasets[0].data = chartData.legit;
            activityChart.data.datasets[1].data = chartData.malicious;
            activityChart.update('none');
        }
        
        // Обновляем время последнего обновления
        const updateElement = document.getElementById('last-update');
        if (updateElement) {
            updateElement.textContent = 'Last update: ' + new Date().toLocaleTimeString();
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

    // Функция для ручной отправки тестового запроса
    function sendTestRequest() {
        const testRequest = {
            clientIP: '192.168.1.' + Math.floor(Math.random() * 255),
            path: '/test/' + Math.random().toString(36).substring(7),
            isMalicious: Math.random() > 0.7,
            timestamp: new Date().toISOString()
        };
        
        fetch('/api/user_registered', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify(testRequest)
        })
        .then(response => response.json())
        .then(data => console.log('Test request sent:', data))
        .catch(error => console.error('Error sending test request:', error));
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
        connectSSE();
        
        // Добавляем кнопку для тестирования (только для разработки)
        const testButton = document.createElement('button');
        testButton.textContent = 'Send Test Request';
        testButton.style.position = 'fixed';
        testButton.style.bottom = '20px';
        testButton.style.right = '20px';
        testButton.style.padding = '10px';
        testButton.style.backgroundColor = '#3498db';
        testButton.style.color = 'white';
        testButton.style.border = 'none';
        testButton.style.borderRadius = '5px';
        testButton.style.cursor = 'pointer';
        testButton.style.zIndex = '1000';
        testButton.addEventListener('click', sendTestRequest);
        document.body.appendChild(testButton);
    });

    // Обработка перед закрытием страницы
    window.addEventListener('beforeunload', function() {
        if (eventSource) {
            eventSource.close();
        }
        clearTimeout(reconnectTimeout);
    });
