
        // Тестовые данные клиентов
        const testClients = [
            { ip: "192.168.1.100", country: "us", isMalicious: false, serverId: "srv-web-01" },
            { ip: "10.0.0.50", country: "ru", isMalicious: true, serverId: "srv-honeypot-02" },
            { ip: "172.16.0.25", country: "cn", isMalicious: false, serverId: "srv-api-03" },
            { ip: "203.0.113.45", country: "de", isMalicious: true, serverId: "srv-honeypot-01" },
            { ip: "198.51.100.123", country: "fr", isMalicious: false, serverId: "srv-web-02" },
            { ip: "203.0.113.67", country: "uk", isMalicious: false, serverId: "srv-db-01" },
            { ip: "192.0.2.189", country: "jp", isMalicious: true, serverId: "srv-honeypot-03" },
            { ip: "198.51.100.204", country: "br", isMalicious: false, serverId: "srv-web-03" },
            { ip: "203.0.113.12", country: "us", isMalicious: true, serverId: "srv-honeypot-04" },
            { ip: "192.0.2.55", country: "ru", isMalicious: false, serverId: "srv-api-02" }
        ];

        // Карта стран для отображения названий
        const countryNames = {
            us: "США",
            ru: "Россия",
            cn: "Китай",
            de: "Германия",
            fr: "Франция",
            uk: "Великобритания",
            jp: "Япония",
            br: "Бразилия"
        };

        // Функция для отображения клиентов в таблице
        function renderClientsTable(clients) {
            const tableBody = document.getElementById('users-table-body');
            tableBody.innerHTML = '';

            clients.forEach(client => {
                const row = document.createElement('tr');
                const statusClass = client.isMalicious ? 'status-malicious' : 'status-legit';
                const statusText = client.isMalicious ? 'Вредоносный' : 'Легитимный';
                const countryName = countryNames[client.country] || client.country;

                row.innerHTML = `
                    <td>
                        <a href="/client/${client.ip}" class="ip-link" title="Подробная информация о клиенте">
                            ${client.ip}
                        </a>
                    </td>
                    <td>
                        <span class="flag flag-${client.country}" title="${countryName}"></span>
                        ${countryName}
                    </td>
                    <td>
                        <span class="status-badge ${statusClass}">${statusText}</span>
                    </td>
                    <td>
                        <span class="server-id">${client.serverId}</span>
                    </td>
                    <td>
                        <div class="context-menu">
                            <button class="context-menu-btn" onclick="toggleContextMenu(this)">
                                <i class="fas fa-ellipsis-v"></i>
                            </button>
                            <div class="context-menu-content">
                                <a href="/client/${client.ip}" class="context-menu-item">
                                    <i class="fas fa-info-circle"></i>Подробности
                                </a>
                                <a href="#" class="context-menu-item" onclick="blockClient('${client.ip}')">
                                    <i class="fas fa-ban"></i>Заблокировать
                                </a>
                                <a href="#" class="context-menu-item" onclick="analyzeClient('${client.ip}')">
                                    <i class="fas fa-search"></i>Проанализировать
                                </a>
                            </div>
                        </div>
                    </td>
                `;

                tableBody.appendChild(row);
            });

            // Обновляем счетчик клиентов
            document.getElementById('total-clients').textContent = clients.length;
        }

        // Функция для переключения контекстного меню
        function toggleContextMenu(button) {
            // Закрываем все открытые меню
            document.querySelectorAll('.context-menu-content').forEach(menu => {
                menu.classList.remove('show');
            });

            // Открываем текущее меню
            const menu = button.nextElementSibling;
            menu.classList.toggle('show');

            // Закрываем меню при клике вне его
            setTimeout(() => {
                const closeMenu = (e) => {
                    if (!menu.contains(e.target) && e.target !== button) {
                        menu.classList.remove('show');
                        document.removeEventListener('click', closeMenu);
                    }
                };
                document.addEventListener('click', closeMenu);
            });
        }

        // Функции для действий контекстного меню (заглушки)
        function blockClient(ip) {
            alert(`Блокировка клиента ${ip} - функционал в разработке`);
            // Закрываем меню после действия
            document.querySelectorAll('.context-menu-content').forEach(menu => {
                menu.classList.remove('show');
            });
        }

        function analyzeClient(ip) {
            alert(`Анализ клиента ${ip} - функционал в разработке`);
            // Закрываем меню после действия
            document.querySelectorAll('.context-menu-content').forEach(menu => {
                menu.classList.remove('show');
            });
        }

        // Функция поиска клиентов
        function searchClients(query) {
            const filteredClients = testClients.filter(client => {
                const searchLower = query.toLowerCase();
                return client.ip.toLowerCase().includes(searchLower) ||
                       countryNames[client.country].toLowerCase().includes(searchLower) ||
                       client.serverId.toLowerCase().includes(searchLower);
            });
            renderClientsTable(filteredClients);
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
                
                // Здесь можно добавить логику загрузки контента для разных страниц
                console.log(`Переход на страницу: ${this.getAttribute('data-page')}`);
            });
        });

        // Обработчик поиска
        document.getElementById('search-input').addEventListener('input', function(e) {
            searchClients(e.target.value);
        });

        // Обработчик кнопки обновления
        document.getElementById('refresh-btn').addEventListener('click', function() {
            // В реальном приложении здесь будет запрос к API
            renderClientsTable(testClients);
            document.getElementById('search-input').value = '';
            alert('Данные обновлены');
        });

        // Инициализация при загрузке страницы
        document.addEventListener('DOMContentLoaded', function() {
            renderClientsTable(testClients);
        });

        // Обработка ссылок на детальную информацию о клиенте
        document.addEventListener('click', function(e) {
            if (e.target.classList.contains('ip-link')) {
                e.preventDefault();
                const ip = e.target.textContent;
                alert(`Подробная информация о клиенте ${ip} - страница в разработке`);
                // В реальном приложении: window.location.href = `/client/${ip}`;
            }
        });