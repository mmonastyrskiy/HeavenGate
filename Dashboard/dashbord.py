import json
import logging
import time
import threading
import asyncio
import websockets
from datetime import datetime
from typing import Dict, List, Set, Any, Optional
from dataclasses import dataclass, asdict, field
from collections import defaultdict, deque
from contextlib import asynccontextmanager
from enum import Enum

from fastapi import FastAPI, WebSocket, WebSocketDisconnect, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import HTMLResponse, FileResponse
from fastapi.staticfiles import StaticFiles
from pydantic import BaseModel
import uvicorn

# Модели данных
class RequestType(str, Enum):
    BALANCER = "balancer"
    USER = "user"
    AGENTS = "agents"

class BalancerRequest(BaseModel):
    clientIP: str
    path: str
    isMalicious: bool
    timestamp: str
    receivedAt: datetime = field(default_factory=datetime.now)

class ClientInfo(BaseModel):
    ip: str
    isMalicious: bool
    firstSeen: datetime
    lastSeen: datetime
    requestCount: int = 1
    country: str

class AgentsInfo(BaseModel):
    realServers: int = 0
    honeypots: int = 0

class Stats(BaseModel):
    totalRequests: int = 0
    legitClients: int = 0
    maliciousClients: int = 0
    agents: AgentsInfo = field(default_factory=AgentsInfo)

class SSEEvent(BaseModel):
    type: str
    data: Dict[str, Any]

# Менеджер WebSocket соединений
class ConnectionManager:
    def __init__(self):
        self.active_connections: List[WebSocket] = []
        self.lock = threading.Lock()
        
    async def connect(self, websocket: WebSocket):
        await websocket.accept()
        with self.lock:
            self.active_connections.append(websocket)
        logging.info(f"WebSocket connected: {websocket.client}. Total: {len(self.active_connections)}")
        
    def disconnect(self, websocket: WebSocket):
        with self.lock:
            if websocket in self.active_connections:
                self.active_connections.remove(websocket)
        logging.info(f"WebSocket disconnected: {websocket.client}. Total: {len(self.active_connections)}")
        
    async def broadcast(self, message: dict):
        disconnected = []
        with self.lock:
            connections = self.active_connections.copy()
        
        for connection in connections:
            try:
                await connection.send_json(message)
            except Exception:
                disconnected.append(connection)
                
        # Удаляем отключенные соединения
        if disconnected:
            with self.lock:
                for conn in disconnected:
                    if conn in self.active_connections:
                        self.active_connections.remove(conn)
            logging.info(f"Removed {len(disconnected)} disconnected WebSockets")

# Менеджер данных
class DataManager:
    def __init__(self, max_history: int = 1000):
        self.max_history = max_history
        self.requests: deque = deque(maxlen=max_history)
        self.clients: Dict[str, ClientInfo] = {}
        self.agents = AgentsInfo()
        self.stats = Stats()
        self.lock = threading.RLock()
        self._update_stats()
        
    def add_request(self, request: BalancerRequest):
        with self.lock:
            # Добавляем запрос
            self.requests.append(request)
            
            # Обновляем информацию о клиенте
            self._update_client_info(request)
            
            # Обновляем статистику
            self._update_stats()
            
        # Логируем в консоль
        status = "🚨 MALICIOUS" if request.isMalicious else "✅ LEGIT"
        console_msg = f"{status} | {request.clientIP} | {request.path} | {request.receivedAt.strftime('%H:%M:%S')}"
        print(console_msg)
        
        # Логируем в файл
        log_level = logging.WARNING if request.isMalicious else logging.INFO
        logging.log(log_level, f"Request: {status} IP: {request.clientIP} Path: {request.path}")
        
    def _update_client_info(self, request: BalancerRequest):
        client_ip = request.clientIP
        now = datetime.now()
        
        if client_ip in self.clients:
            client = self.clients[client_ip]
            client.lastSeen = now
            client.requestCount += 1
            
            if request.isMalicious and not client.isMalicious:
                client.isMalicious = True
                logging.warning(f"Client {client_ip} marked as MALICIOUS")
        else:
            self.clients[client_ip] = ClientInfo(
                ip=client_ip,
                isMalicious=request.isMalicious,
                firstSeen=now,
                lastSeen=now,
                requestCount=1
            )
            logging.info(f"New client registered: {client_ip} (Malicious: {request.isMalicious})")
    
    def _update_stats(self):
        legit_clients = 0
        malicious_clients = 0
        
        for client in self.clients.values():
            if client.isMalicious:
                malicious_clients += 1
            else:
                legit_clients += 1
                
        self.stats = Stats(
            totalRequests=len(self.requests),
            legitClients=legit_clients,
            maliciousClients=malicious_clients,
            agents=self.agents
        )
    
    def update_agents(self, agents: AgentsInfo):
        with self.lock:
            old_agents = self.agents.copy()
            self.agents = agents
            self._update_stats()
            
        logging.info(f"Agents updated: real_servers={old_agents.realServers}->{agents.realServers}, "
                    f"honeypots={old_agents.honeypots}->{agents.honeypots}")
    
    def get_all_clients(self) -> List[ClientInfo]:
        with self.lock:
            return list(self.clients.values())
    
    def get_all_requests(self) -> List[BalancerRequest]:
        with self.lock:
            return list(self.requests)
    
    def get_stats(self) -> Stats:
        with self.lock:
            return self.stats
    
    def get_initial_data(self) -> Dict[str, Any]:
        with self.lock:
            return {
                "requests": [asdict(req) for req in self.requests],
                "total": len(self.requests),
                "legitClients": len([c for c in self.clients.values() if not c.isMalicious]),
                "maliciousClients": len([c for c in self.clients.values() if c.isMalicious]),
                "agents": asdict(self.agents)
            }

# Инициализация приложения
@dataclass
class AppConfig:
    host: str = "0.0.0.0"
    port: int = 8081
    log_level: str = "INFO"
    log_file: str = "heavengate_dashboard.log"
    enable_logging: bool = True

class HeavenGateDashboard:
    def __init__(self, config: Optional[AppConfig] = None):
        self.config = config or AppConfig()
        self._setup_logging()
        
        self.data_manager = DataManager()
        self.connection_manager = ConnectionManager()
        
        self.app = FastAPI(
            title="HeavenGate Dashboard",
            description="Real-time monitoring dashboard for HeavenGate Load Balancer",
            version="1.0.0"
        )
        
        self._setup_middleware()
        self._setup_routes()
        
    def _setup_logging(self):
        if self.config.enable_logging:
            logging.basicConfig(
                level=getattr(logging, self.config.log_level),
                format='%(asctime)s - %(name)s - %(levelname)s - %(message)s',
                handlers=[
                    logging.FileHandler(self.config.log_file),
                    logging.StreamHandler()
                ]
            )
        else:
            logging.disable(logging.CRITICAL)
            
        self.logger = logging.getLogger(__name__)
        
    def _setup_middleware(self):
        # Настройка CORS
        self.app.add_middleware(
            CORSMiddleware,
            allow_origins=["*"],
            allow_credentials=True,
            allow_methods=["*"],
            allow_headers=["*"],
        )
        
        # Монтирование статических файлов
        try:
            self.app.mount("/static", StaticFiles(directory="static"), name="static")
        except:
            self.logger.warning("Static directory not found, serving from memory")
            
    def _setup_routes(self):
        # Основной маршрут
        @self.app.get("/")
        async def index():
            try:
                return FileResponse("static/index.html")
            except:
                # Если нет статического файла, возвращаем базовый HTML
                html_content = """
                <!DOCTYPE html>
                <html>
                <head>
                    <title>HeavenGate Dashboard</title>
                    <style>
                        body { font-family: Arial, sans-serif; margin: 40px; }
                        .container { max-width: 1200px; margin: 0 auto; }
                        .header { text-align: center; margin-bottom: 30px; }
                        .status { padding: 20px; background: #f5f5f5; border-radius: 5px; }
                        .stats { display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 20px; margin: 20px 0; }
                        .stat-box { background: white; padding: 20px; border-radius: 5px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); text-align: center; }
                        .malicious { color: #dc3545; font-weight: bold; }
                        .legit { color: #28a745; font-weight: bold; }
                        #requests { margin-top: 20px; }
                        .request { padding: 10px; border-bottom: 1px solid #eee; }
                        .request.malicious { background: #ffe6e6; }
                    </style>
                </head>
                <body>
                    <div class="container">
                        <div class="header">
                            <h1>🚀 HeavenGate Dashboard</h1>
                            <p>Real-time monitoring system</p>
                        </div>
                        <div class="status" id="status">Connecting...</div>
                        <div class="stats" id="stats"></div>
                        <div id="requests"></div>
                    </div>
                    <script>
                        const ws = new WebSocket(`ws://${window.location.host}/ws`);
                        ws.onopen = () => {
                            document.getElementById('status').innerHTML = '✅ Connected';
                        };
                        ws.onmessage = (event) => {
                            const data = JSON.parse(event.data);
                            updateDashboard(data);
                        };
                        ws.onerror = () => {
                            document.getElementById('status').innerHTML = '❌ Connection error';
                        };
                        ws.onclose = () => {
                            document.getElementById('status').innerHTML = '🔌 Disconnected';
                        };
                        
                        function updateDashboard(data) {
                            // Обновление статистики
                            if (data.stats) {
                                const stats = data.stats;
                                document.getElementById('stats').innerHTML = `
                                    <div class="stat-box">
                                        <h3>Total Requests</h3>
                                        <h2>${stats.totalRequests}</h2>
                                    </div>
                                    <div class="stat-box">
                                        <h3>Legit Clients</h3>
                                        <h2 class="legit">${stats.legitClients}</h2>
                                    </div>
                                    <div class="stat-box">
                                        <h3>Malicious Clients</h3>
                                        <h2 class="malicious">${stats.maliciousClients}</h2>
                                    </div>
                                    <div class="stat-box">
                                        <h3>Agents</h3>
                                        <p>Real Servers: ${stats.agents.realServers}</p>
                                        <p>Honeypots: ${stats.agents.honeypots}</p>
                                    </div>
                                `;
                            }
                            
                            // Обновление последних запросов
                            if (data.request) {
                                const request = data.request;
                                const requestDiv = document.createElement('div');
                                requestDiv.className = `request ${request.isMalicious ? 'malicious' : ''}`;
                                requestDiv.innerHTML = `
                                    <strong>${request.isMalicious ? '🚨 MALICIOUS' : '✅ LEGIT'}</strong>
                                    IP: ${request.clientIP} | Path: ${request.path}
                                    <small>${new Date(request.receivedAt).toLocaleTimeString()}</small>
                                `;
                                document.getElementById('requests').prepend(requestDiv);
                            }
                        }
                    </script>
                </body>
                </html>
                """
                return HTMLResponse(content=html_content)
        
        # API для приема запросов от балансировщика
        @self.app.post("/api/req_registered")
        async def handle_balancer_request(request: BalancerRequest):
            """Прием запросов от балансировщика"""
            self.data_manager.add_request(request)
            
            # Подготавливаем данные для рассылки
            broadcast_data = {
                "type": "new_request",
                "data": {
                    "request": asdict(request),
                    "stats": asdict(self.data_manager.get_stats())
                }
            }
            
            # Рассылаем всем подключенным клиентам
            await self.connection_manager.broadcast(broadcast_data)
            
            return {"status": "received"}
        
        # API для обновления информации об агентах
        @self.app.post("/api/agents")
        async def update_agents(agents: AgentsInfo):
            """Обновление информации об агентах"""
            self.data_manager.update_agents(agents)
            
            broadcast_data = {
                "type": "agents_update",
                "data": {
                    "agents": asdict(agents)
                }
            }
            
            await self.connection_manager.broadcast(broadcast_data)
            
            return {"status": "updated"}
        
        # API для обновления информации о пользователях
        @self.app.post("/api/user_registered")
        async def update_users(data: dict):
            """Обновление информации о пользователях"""
            legit_clients = data.get("legitClients", 0)
            malicious_clients = data.get("maliciousClients", 0)
            
            self.logger.info(f"User update received: legit={legit_clients}, malicious={malicious_clients}")
            
            broadcast_data = {
                "type": "agents_update",
                "data": {
                    "agents": {
                        "legitClients": legit_clients,
                        "maliciousClients": malicious_clients
                    }
                }
            }
            
            await self.connection_manager.broadcast(broadcast_data)
            
            return {
                "status": "success",
                "message": "Data received and broadcasted",
                "data": {
                    "legitClients": legit_clients,
                    "maliciousClients": malicious_clients
                }
            }
        
        # API для получения списка клиентов
        @self.app.get("/api/clients")
        async def get_clients():
            """Получение списка всех клиентов"""
            clients = self.data_manager.get_all_clients()
            return [asdict(client) for client in clients]
        
        # API для получения истории запросов
        @self.app.get("/api/requests")
        async def get_requests(limit: int = 100):
            """Получение истории запросов"""
            requests = self.data_manager.get_all_requests()
            return [asdict(req) for req in list(requests)[-limit:]]
        
        # API для получения статистики
        @self.app.get("/api/stats")
        async def get_stats():
            """Получение текущей статистики"""
            return asdict(self.data_manager.get_stats())
        
        # WebSocket endpoint для real-time обновлений
        @self.app.websocket("/ws")
        async def websocket_endpoint(websocket: WebSocket):
            await self.connection_manager.connect(websocket)
            
            try:
                # Отправляем начальные данные
                initial_data = {
                    "type": "initial",
                    "data": self.data_manager.get_initial_data()
                }
                await websocket.send_json(initial_data)
                
                # Периодические ping-сообщения
                async def send_ping():
                    while True:
                        await asyncio.sleep(30)
                        try:
                            await websocket.send_json({"type": "ping", "message": "keep-alive"})
                        except:
                            break
                
                # Запускаем ping в фоне
                ping_task = asyncio.create_task(send_ping())
                
                # Ожидаем сообщения от клиента (или отключения)
                while True:
                    try:
                        data = await websocket.receive_text()
                        # Можно обрабатывать входящие сообщения от клиента
                        self.logger.debug(f"Received from WebSocket: {data}")
                    except WebSocketDisconnect:
                        break
                        
            except Exception as e:
                self.logger.error(f"WebSocket error: {e}")
            finally:
                ping_task.cancel()
                self.connection_manager.disconnect(websocket)
        
        # Health check endpoint
        @self.app.get("/health")
        async def health_check():
            """Health check endpoint"""
            return {
                "status": "healthy",
                "timestamp": datetime.now().isoformat(),
                "connections": len(self.connection_manager.active_connections),
                "requests": len(self.data_manager.requests),
                "clients": len(self.data_manager.clients)
            }
    
    def run(self):
        """Запуск сервера"""
        print(f"""
        🚀 HeavenGate Dashboard started!
        📡 Listening for balancer requests on {self.config.host}:{self.config.port}
        🔄 WebSocket endpoint: ws://{self.config.host}:{self.config.port}/ws
        🌐 Open http://{self.config.host}:{self.config.port} to view balancer requests
        """)
        
        if self.config.enable_logging:
            print("📝 Logging enabled: logs are being written to", self.config.log_file)
        else:
            print("📝 Logging disabled")
            
        print("--------------------------------------------------------")
        
        uvicorn.run(
            self.app,
            host=self.config.host,
            port=self.config.port,
            log_level=self.config.log_level.lower()
        )

# Пример использования
if __name__ == "__main__":
    # Конфигурация
    config = AppConfig(
        host="0.0.0.0",
        port=8081,
        log_level="INFO",
        enable_logging=True
    )
    
    # Создание и запуск дашборда
    dashboard = HeavenGateDashboard(config)
    
    # Можно запускать в отдельном потоке для интеграции с другими компонентами
    # import threading
    # thread = threading.Thread(target=dashboard.run)
    # thread.daemon = True
    # thread.start()
    
    # Или просто запустить
    dashboard.run()