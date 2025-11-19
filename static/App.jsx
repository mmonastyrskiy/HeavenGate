import React, { useState, useEffect, useRef } from 'react';
import { Chart as ChartJS, CategoryScale, LinearScale, PointElement, LineElement, Title, Tooltip, Legend, Filler } from 'chart.js';
import { Line } from 'react-chartjs-2';
import './App.css';

// Register ChartJS components
ChartJS.register(CategoryScale, LinearScale, PointElement, LineElement, Title, Tooltip, Legend, Filler);

const API_BASE = 'http://localhost:8080/api';

const App = () => {
  const [activePage, setActivePage] = useState('home');
  const [requests, setRequests] = useState([]);
  const [clients, setClients] = useState([]);
  const [agents, setAgents] = useState({ realServers: 0, honeypots: 0 });
  const [stats, setStats] = useState({
    totalRequests: 0,
    legitClients: 0,
    maliciousClients: 0
  });
  const [chartData, setChartData] = useState({
    labels: [],
    legit: Array(24).fill(0),
    malicious: Array(24).fill(0)
  });
  const [searchQuery, setSearchQuery] = useState('');
  const [showNotificationMenu, setShowNotificationMenu] = useState(false);
  const [showUserMenu, setShowUserMenu] = useState(false);
  const [isConnected, setIsConnected] = useState(false);
  const eventSourceRef = useRef(null);

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

  // Chart configuration
  const chartOptions = {
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
        suggestedMax: 10,
        grid: {
          display: true
        },
        ticks: {
          stepSize: 1
        }
      }
    },
    animation: {
      duration: 0
    },
    elements: {
      point: {
        radius: 0
      }
    }
  };

  const chartConfig = {
    labels: chartData.labels,
    datasets: [
      {
        label: 'Легитимные клиенты',
        data: chartData.legit,
        borderColor: '#2ecc71',
        backgroundColor: 'rgba(46, 204, 113, 0.1)',
        tension: 0.4,
        fill: true,
        borderWidth: 2
      },
      {
        label: 'Вредоносные клиенты',
        data: chartData.malicious,
        borderColor: '#e74c3c',
        backgroundColor: 'rgba(231, 76, 60, 0.1)',
        tension: 0.4,
        fill: true,
        borderWidth: 2
      }
    ]
  };

  // Initialize chart labels
  useEffect(() => {
    const labels = [];
    const now = new Date();
    
    for (let i = 23; i >= 0; i--) {
      const time = new Date(now);
      time.setHours(now.getHours() - i);
      labels.push(time.toLocaleTimeString([], {hour: '2-digit', minute: '2-digit'}));
    }
    
    setChartData(prev => ({ ...prev, labels }));
  }, []);

  // Fetch initial data
  useEffect(() => {
    fetchInitialData();
    connectSSE();
    
    return () => {
      if (eventSourceRef.current) {
        eventSourceRef.current.close();
      }
    };
  }, []);

  const fetchInitialData = async () => {
    try {
      const [statsRes, requestsRes, clientsRes, agentsRes] = await Promise.all([
        fetch(`${API_BASE}/stats`),
        fetch(`${API_BASE}/requests`),
        fetch(`${API_BASE}/clients`),
        fetch(`${API_BASE}/agents`)
      ]);

      const statsData = await statsRes.json();
      const requestsData = await requestsRes.json();
      const clientsData = await clientsRes.json();
      const agentsData = await agentsRes.json();

      setStats(statsData);
      setRequests(requestsData);
      setClients(clientsData);
      setAgents(agentsData);
    } catch (error) {
      console.error('Error fetching initial data:', error);
    }
  };

  const connectSSE = () => {
    try {
      eventSourceRef.current = new EventSource(`${API_BASE}/events`);
      
      eventSourceRef.current.onopen = () => {
        console.log('SSE connection opened');
        setIsConnected(true);
      };

      eventSourceRef.current.onmessage = (event) => {
        try {
          const data = JSON.parse(event.data);
          handleSSEMessage(data);
        } catch (error) {
          console.error('Error parsing SSE message:', error);
        }
      };

      eventSourceRef.current.addEventListener('connected', (event) => {
        const data = JSON.parse(event.data);
        console.log('SSE connected with client ID:', data.clientId);
      });

      eventSourceRef.current.onerror = (event) => {
        console.error('SSE error:', event);
        setIsConnected(false);
        
        if (eventSourceRef.current) {
          eventSourceRef.current.close();
        }
        
        // Attempt reconnect after 3 seconds
        setTimeout(connectSSE, 3000);
      };

    } catch (error) {
      console.error('Error creating SSE connection:', error);
      setTimeout(connectSSE, 3000);
    }
  };

  const handleSSEMessage = (data) => {
    switch (data.type) {
      case 'new_request':
        console.log('New request received:', data.data.request);
        // Add new request to the beginning of the list
        setRequests(prev => [data.data.request, ...prev.slice(0, 19)]);
        
        // Update stats
        if (data.data.stats) {
          setStats(data.data.stats);
        }
        
        // Update chart
        updateChartWithNewRequest(data.data.request);
        break;
        
      case 'connected':
        console.log('SSE connected:', data.data);
        break;
        
      default:
        console.log('Unknown message type:', data.type);
    }
  };

  const updateChartWithNewRequest = (request) => {
    const requestTime = new Date(request.receivedAt);
    const now = new Date();
    const currentHour = now.getHours();
    const requestHour = requestTime.getHours();
    
    // Determine the index in the chart data array
    let index = (requestHour - (currentHour - 23) + 24) % 24;
    
    if (index >= 0 && index < 24) {
      setChartData(prev => {
        const newLegit = [...prev.legit];
        const newMalicious = [...prev.malicious];
        
        if (request.isMalicious) {
          newMalicious[index] += 1;
        } else {
          newLegit[index] += 1;
        }
        
        return {
          ...prev,
          legit: newLegit,
          malicious: newMalicious
        };
      });
    }
  };

  // Close dropdowns when clicking outside
  useEffect(() => {
    const handleClickOutside = () => {
      setShowNotificationMenu(false);
      setShowUserMenu(false);
    };

    document.addEventListener('click', handleClickOutside);
    return () => document.removeEventListener('click', handleClickOutside);
  }, []);

  const navigateToPage = (pageId) => {
    setActivePage(pageId);
    window.history.pushState(null, null, `#${pageId}`);
  };

  const handleNotificationClick = (e) => {
    e.stopPropagation();
    setShowNotificationMenu(!showNotificationMenu);
    setShowUserMenu(false);
  };

  const handleUserMenuClick = (e) => {
    e.stopPropagation();
    setShowUserMenu(!showUserMenu);
    setShowNotificationMenu(false);
  };

  const searchClients = (query) => {
    return clients.filter(client => {
      const searchLower = query.toLowerCase();
      return client.ip.toLowerCase().includes(searchLower) ||
             countryNames[client.country]?.toLowerCase().includes(searchLower) ||
             client.serverId.toLowerCase().includes(searchLower);
    });
  };

  const filteredClients = searchQuery ? searchClients(searchQuery) : clients;

  const renderHomePage = () => (
    <div className={`page-content ${activePage === 'home' ? 'active' : ''}`}>
      <div className="page-title">
        <h2>Обзор системы</h2>
        <div className="connection-status">
          <span style={{ color: isConnected ? '#2ecc71' : '#e74c3c', fontWeight: 'bold' }}>
            {isConnected ? '🟢 Connected' : '🔴 Disconnected'}
          </span>
        </div>
      </div>

      <div className="stats-cards">
        <div className="stat-card">
          <div className="stat-icon" style={{backgroundColor: 'var(--primary-color)'}}>
            <i className="fas fa-shield-alt"></i>
          </div>
          <div className="stat-info">
            <h3>{stats.totalRequests}</h3>
            <p>Всего запросов</p>
          </div>
        </div>
        <div className="stat-card">
          <div className="stat-icon" style={{backgroundColor: 'var(--success-color)'}}>
            <i className="fas fa-user-check"></i>
          </div>
          <div className="stat-info">
            <h3>{stats.legitClients}</h3>
            <p>Легитимные клиенты</p>
          </div>
        </div>
        <div className="stat-card">
          <div className="stat-icon" style={{backgroundColor: 'var(--danger-color)'}}>
            <i className="fas fa-user-slash"></i>
          </div>
          <div className="stat-info">
            <h3>{stats.maliciousClients}</h3>
            <p>Вредоносные клиенты</p>
          </div>
        </div>
        <div className="stat-card">
          <div className="stat-icon" style={{backgroundColor: 'var(--warning-color)'}}>
            <i className="fas fa-server"></i>
          </div>
          <div className="stat-info">
            <h3>{agents.realServers}</h3>
            <p>Реальные серверы</p>
          </div>
        </div>
        <div className="stat-card">
          <div className="stat-icon" style={{backgroundColor: 'var(--info-color)'}}>
            <i className="fas fa-bug"></i>
          </div>
          <div className="stat-info">
            <h3>{agents.honeypots}</h3>
            <p>Ловушки</p>
          </div>
        </div>
      </div>

      <div className="chart-container">
        <h3>Активность клиентов (последние 24 часа)</h3>
        <div className="chart-wrapper">
          <Line data={chartConfig} options={chartOptions} />
        </div>
      </div>

      <div className="requests-table">
        <h3 style={{padding: '20px 20px 0'}}>Последние запросы</h3>
        <table>
          <thead>
            <tr>
              <th>Статус</th>
              <th>IP-адрес</th>
              <th>Путь</th>
              <th>Время получения</th>
            </tr>
          </thead>
          <tbody>
            {requests.length === 0 ? (
              <tr>
                <td colSpan="4">
                  <div className="empty-table">
                    <i className="fas fa-inbox"></i>
                    <p>Нет данных для отображения</p>
                    <small>Запросы появятся здесь, когда система начнет их получать</small>
                  </div>
                </td>
              </tr>
            ) : (
              requests.slice(0, 20).map((request, index) => (
                <tr key={index}>
                  <td>
                    <span className={`status-badge ${request.isMalicious ? 'status-malicious' : 'status-legit'}`}>
                      {request.isMalicious ? '🚨 MALICIOUS' : '✅ LEGIT'}
                    </span>
                  </td>
                  <td>{request.clientIP}</td>
                  <td>{request.path}</td>
                  <td>{new Date(request.receivedAt).toLocaleString()}</td>
                </tr>
              ))
            )}
          </tbody>
        </table>
      </div>
    </div>
  );

  const renderUsersPage = () => (
    <div className={`page-content ${activePage === 'users' ? 'active' : ''}`}>
      <div className="page-title">
        <h2>Управление пользователями</h2>
        <div className="clients-count">
          Всего клиентов: <span id="total-clients">{filteredClients.length}</span>
        </div>
      </div>

      <div className="users-table-container">
        <div className="table-header">
          <h3>Список клиентов</h3>
          <div className="table-actions">
            <div className="search-box">
              <i className="fas fa-search"></i>
              <input 
                type="text" 
                placeholder="Поиск по IP или стране..."
                value={searchQuery}
                onChange={(e) => setSearchQuery(e.target.value)}
              />
            </div>
            <button className="btn btn-primary" onClick={() => {
              setSearchQuery('');
              fetchInitialData();
            }}>
              <i className="fas fa-sync-alt"></i> Обновить
            </button>
          </div>
        </div>

        <div className="table-wrapper">
          <table className="users-table">
            <thead>
              <tr>
                <th>IP Адрес</th>
                <th>Страна</th>
                <th>Статус</th>
                <th>Сервер</th>
                <th>Первое появление</th>
                <th width="50"></th>
              </tr>
            </thead>
            <tbody>
              {filteredClients.map((client, index) => (
                <ClientRow 
                  key={index} 
                  client={client} 
                  countryNames={countryNames} 
                />
              ))}
            </tbody>
          </table>
        </div>
      </div>
    </div>
  );

  const renderPlaceholderPage = (title, icon) => (
    <div className={`page-content ${activePage === title.toLowerCase() ? 'active' : ''}`}>
      <div className="page-title">
        <h2>{title}</h2>
      </div>
      <div style={{textAlign: 'center', padding: '60px 20px', color: '#7f8c8d'}}>
        <i className={icon} style={{fontSize: '4rem', marginBottom: '20px'}}></i>
        <h3>Страница в разработке</h3>
        <p>Раздел будет доступен в ближайшее время</p>
      </div>
    </div>
  );

  const getPageTitle = () => {
    const titles = {
      'home': 'Главная',
      'users': 'Пользователи',
      'agents': 'Агенты',
      'rules': 'Правила',
      'settings': 'Настройки'
    };
    return titles[activePage] || 'Главная';
  };

  return (
    <div className="app">
      {/* Sidebar */}
      <div className="sidebar">
        <div className="logo">
          <h1>HeavenGate</h1>
        </div>
        <ul className="nav-links">
          {[
            { id: 'home', icon: 'fas fa-home', text: 'Домашняя страница' },
            { id: 'users', icon: 'fas fa-users', text: 'Пользователи' },
            { id: 'agents', icon: 'fas fa-server', text: 'Агенты' },
            { id: 'rules', icon: 'fas fa-shield-alt', text: 'Правила' },
            { id: 'settings', icon: 'fas fa-cog', text: 'Настройки' }
          ].map(item => (
            <li key={item.id}>
              <a 
                href={`#${item.id}`} 
                className={`nav-link ${activePage === item.id ? 'active' : ''}`}
                onClick={(e) => {
                  e.preventDefault();
                  navigateToPage(item.id);
                }}
              >
                <i className={item.icon}></i>
                <span>{item.text}</span>
              </a>
            </li>
          ))}
        </ul>
      </div>

      {/* Header */}
      <div className="header">
        <div className="header-left">
          <h2>{getPageTitle()}</h2>
        </div>
        <div className="header-right">
          <div className={`notification-icon dropdown ${showNotificationMenu ? 'active' : ''}`}>
            <i className="fas fa-bell" onClick={handleNotificationClick}></i>
            <span className="notification-badge">3</span>
            {showNotificationMenu && (
              <div className="dropdown-content">
                <div className="dropdown-header">Уведомления</div>
                <a href="#" className="dropdown-item">
                  <div>Обнаружена подозрительная активность</div>
                  <small>2 минуты назад</small>
                </a>
                <a href="#" className="dropdown-item">
                  <div>Новый пользователь зарегистрирован</div>
                  <small>5 минут назад</small>
                </a>
                <a href="#" className="dropdown-item">
                  <div>Обновление системы завершено</div>
                  <small>Вчера</small>
                </a>
              </div>
            )}
          </div>
          <div className={`user-profile dropdown ${showUserMenu ? 'active' : ''}`}>
            <div className="user-avatar" onClick={handleUserMenuClick}>A</div>
            <span onClick={handleUserMenuClick}>Администратор</span>
            {showUserMenu && (
              <div className="dropdown-content">
                <a href="#" className="dropdown-item"><i className="fas fa-user"></i> Профиль</a>
                <a href="#" className="dropdown-item"><i className="fas fa-cog"></i> Настройки</a>
                <a href="#" className="dropdown-item"><i className="fas fa-sign-out-alt"></i> Выйти</a>
              </div>
            )}
          </div>
        </div>
      </div>

      {/* Main Content */}
      <div className="main-content">
        {activePage === 'home' && renderHomePage()}
        {activePage === 'users' && renderUsersPage()}
        {activePage === 'agents' && renderPlaceholderPage('Агенты', 'fas fa-server')}
        {activePage === 'rules' && renderPlaceholderPage('Правила', 'fas fa-shield-alt')}
        {activePage === 'settings' && renderPlaceholderPage('Настройки', 'fas fa-cog')}
      </div>
    </div>
  );
};

// Client Row Component with Context Menu
const ClientRow = ({ client, countryNames }) => {
  const [showContextMenu, setShowContextMenu] = useState(false);

  const toggleContextMenu = (e) => {
    e.stopPropagation();
    setShowContextMenu(!showContextMenu);
  };

  const handleAction = (action, ip) => {
    setShowContextMenu(false);
    alert(`${action} клиента ${ip} - функционал в разработке`);
  };

  useEffect(() => {
    const handleClickOutside = () => setShowContextMenu(false);
    document.addEventListener('click', handleClickOutside);
    return () => document.removeEventListener('click', handleClickOutside);
  }, []);

  return (
    <tr>
      <td>
        <a 
          href={`/client/${client.ip}`} 
          className="ip-link" 
          title="Подробная информация о клиенте"
          onClick={(e) => {
            e.preventDefault();
            handleAction('Подробности', client.ip);
          }}
        >
          {client.ip}
        </a>
      </td>
      <td>
        <span className={`flag flag-${client.country}`} title={countryNames[client.country]}></span>
        {countryNames[client.country]}
      </td>
      <td>
        <span className={`status-badge ${client.isMalicious ? 'status-malicious' : 'status-legit'}`}>
          {client.isMalicious ? 'Вредоносный' : 'Легитимный'}
        </span>
      </td>
      <td>
        <span className="server-id">{client.serverId}</span>
      </td>
      <td>
        {new Date(client.firstSeen).toLocaleString()}
      </td>
      <td>
        <div className="context-menu">
          <button className="context-menu-btn" onClick={toggleContextMenu}>
            <i className="fas fa-ellipsis-v"></i>
          </button>
          {showContextMenu && (
            <div className="context-menu-content">
              <a 
                href="#" 
                className="context-menu-item"
                onClick={(e) => {
                  e.preventDefault();
                  handleAction('Подробности', client.ip);
                }}
              >
                <i className="fas fa-info-circle"></i>Подробности
              </a>
              <a 
                href="#" 
                className="context-menu-item"
                onClick={(e) => {
                  e.preventDefault();
                  handleAction('Блокировка', client.ip);
                }}
              >
                <i className="fas fa-ban"></i>Заблокировать
              </a>
              <a 
                href="#" 
                className="context-menu-item"
                onClick={(e) => {
                  e.preventDefault();
                  handleAction('Анализ', client.ip);
                }}
              >
                <i className="fas fa-search"></i>Проанализировать
              </a>
            </div>
          )}
        </div>
      </td>
    </tr>
  );
};

export default App;