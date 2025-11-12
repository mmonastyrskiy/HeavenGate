
        // Инициализация графика
        const ctx = document.getElementById('activity-chart').getContext('2d');
        const activityChart = new Chart(ctx, {
            type: 'line',
            data: {
                labels: [], // Временные метки
                datasets: [
                    {
                        label: 'Легитимные запросы',
                        data: [],
                        borderColor: '#2ecc71',
                        backgroundColor: 'rgba(46, 204, 113, 0.1)',
                        tension: 0.4,
                        fill: true
                    },
                    {
                        label: 'Вредоносные запросы',
                        data: [],
                        borderColor: '#e74c3c',
                        backgroundColor: 'rgba(231, 76, 60, 0.1)',
                        tension: 0.4,
                        fill: true
                    }
                ]
            },
            options: {
                responsive: true,
                plugins: {
                    legend: {
                        position: 'top',
                    },
                    title: {
                        display: false
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

        // Функция для обновления статистики
        function updateStats() {
            const totalRequests = requests.length;
            const legitRequests = requests.filter(req => !req.IsMalicious).length;
            const maliciousRequests = requests.filter(req => req.IsMalicious).length;
            
            document.getElementById('total-requests').textContent = totalRequests;
            document.getElementById('legit-requests').textContent = legitRequests;
            document.getElementById('malicious-requests').textContent = maliciousRequests;
            
            // Обновление таблицы запросов
            const tableBody = document.getElementById('requests-table-body');
            tableBody.innerHTML = '';
            
            // Показываем последние 10 запросов
            const recentRequests = requests.slice(-10).reverse();
            
            recentRequests.forEach(request => {
                const row = document.createElement('tr');
                const statusClass = request.IsMalicious ? 'status-malicious' : 'status-legit';
                const statusText = request.IsMalicious ? '🚨 MALICIOUS' : '✅ LEGIT';
                
                row.innerHTML = `
                    <td><span class="status-badge ${statusClass}">${statusText}</span></td>
                    <td>${request.ClientIP}</td>
                    <td>${request.Path}</td>
                    <td>${request.ReceivedAt}</td>
                `;
                
                tableBody.appendChild(row);
            });
            
            // Обновление графика
            updateChart();
        }

        // Функция для обновления графика
        function updateChart() {
            // Группируем запросы по времени (последние 24 часа)
            const now = new Date();
            const timeLabels = [];
            const legitData = [];
            const maliciousData = [];
            
            // Создаем временные интервалы (последние 12 часов с интервалом в 1 час)
            for (let i = 11; i >= 0; i--) {
                const time = new Date(now);
                time.setHours(now.getHours() - i);
                timeLabels.push(time.toLocaleTimeString([], {hour: '2-digit', minute: '2-digit'}));
                
                const hourStart = new Date(time);
                hourStart.setMinutes(0, 0, 0);
                
                const hourEnd = new Date(time);
                hourEnd.setMinutes(59, 59, 999);
                
                const hourRequests = requests.filter(req => {
                    const reqTime = new Date(req.ReceivedAt);
                    return reqTime >= hourStart && reqTime <= hourEnd;
                });
                
                legitData.push(hourRequests.filter(req => !req.IsMalicious).length);
                maliciousData.push(hourRequests.filter(req => req.IsMalicious).length);
            }
            
            // Обновляем данные графика
            activityChart.data.labels = timeLabels;
            activityChart.data.datasets[0].data = legitData;
            activityChart.data.datasets[1].data = maliciousData;
            activityChart.update();
        }

        // Функция для получения данных с сервера
        async function fetchRequests() {
            try {
                const response = await fetch('/user_registered');
                if (response.ok) {
                    const data = await response.json();
                    requests = data.requests || [];
                    updateStats();
                }
            } catch (error) {
                console.error('Ошибка при получении данных:', error);
            }
        }

        // Имитация получения данных (для демонстрации)
        function simulateData() {
            // Генерируем тестовые данные
            const mockRequests = [
                {
                    ClientIP: '192.168.1.100',
                    Path: '/login',
                    IsMalicious: false,
                    Timestamp: new Date().toISOString(),
                    ReceivedAt: new Date().toLocaleString()
                },
                {
                    ClientIP: '10.0.0.50',
                    Path: '/admin',
                    IsMalicious: true,
                    Timestamp: new Date(Date.now() - 300000).toISOString(),
                    ReceivedAt: new Date(Date.now() - 300000).toLocaleString()
                },
                {
                    ClientIP: '172.16.0.25',
                    Path: '/api/data',
                    IsMalicious: false,
                    Timestamp: new Date(Date.now() - 600000).toISOString(),
                    ReceivedAt: new Date(Date.now() - 600000).toLocaleString()
                }
            ];
            
            requests = mockRequests;
            updateStats();
            
            // В реальном приложении здесь будет вызов fetchRequests()
            // setInterval(fetchRequests, 5000); // Обновление каждые 5 секунд
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
                
                // Убираем активный класс у всех ссылок
                document.querySelectorAll('.nav-links a').forEach(item => {
                    item.classList.remove('active');
                });
                
                // Добавляем активный класс к текущей ссылке
                this.classList.add('active');
                
                // Обновляем заголовок страницы
                const pageTitle = this.querySelector('span').textContent;
                document.getElementById('page-title').textContent = pageTitle;
                
                // Здесь можно добавить логику загрузки контента для разных страниц
                const page = this.getAttribute('data-page');
                console.log(`Переход на страницу: ${page}`);
            });
        });

        // Инициализация при загрузке страницы
        document.addEventListener('DOMContentLoaded', function() {
            simulateData(); // Для демонстрации
        });
