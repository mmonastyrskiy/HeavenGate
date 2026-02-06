# Dashboard Migration: Go → Python

This document describes the migration from Go dashboard to Python dashboard.

## Overview

The dashboard has been migrated from Go (`Dashboard/main.go`) to Python (`Dashboard/dashbord.py` using FastAPI).

## Changes Made

### 1. AppComponent Configuration
**File:** `src/AppManager/AppComponent.h`

- **Before:** Pointed to `go-apps/dashboard` (Go binary)
- **After:** Points to `../Dashboard/run_dashboard.py` (Python script)

The `run()` method now properly executes Python with the dashboard script.

### 2. Dashboard Entry Point
**File:** `Dashboard/run_dashboard.py` (NEW)

Created a wrapper script that:
- Ensures proper path resolution
- Sets working directory for static files
- Reads configuration from environment variables
- Launches the dashboard

### 3. Python Dashboard Features

The Python dashboard (`Dashboard/dashbord.py`) provides:

✅ **All Required Endpoints:**
- `POST /api/req_registered` - Receive requests from LoadBalancer
- `POST /api/agents` - Update agent counts
- `POST /api/user_registered` - Update client counts
- `GET /api/clients` - Get client list
- `GET /api/requests` - Get request history
- `GET /api/stats` - Get statistics
- `GET /health` - Health check

✅ **Real-time Updates:**
- WebSocket endpoint at `/ws` for live updates
- Broadcasts new requests, agent updates, and client updates

✅ **Better Features:**
- FastAPI framework (modern, async)
- WebSocket support (vs SSE in Go version)
- Better error handling
- Structured logging
- Static file serving

## API Compatibility

The Python dashboard maintains **100% API compatibility** with the Go version. The C++ `DashboardAPI` class will work without any changes.

### Endpoints Used by C++ Code:

1. **callRequestRegistered()** → `POST /api/req_registered`
   ```json
   {
     "ClientIP": "127.0.0.1",
     "Path": "server-id",
     "IsMalicious": false,
     "Timestamp": "2026-02-06T12:00:00.000Z"
   }
   ```

2. **callAgentChange()** → `POST /api/agents`
   ```json
   {
     "realServers": 3,
     "honeypots": 2
   }
   ```

3. **callClientChange()** → `POST /api/user_registered`
   ```json
   {
     "legitClients": 10,
     "maliciousClients": 2
   }
   ```

## Running the Dashboard

### Standalone (Development)
```bash
cd Dashboard
python3 dashbord.py
# or
python3 run_dashboard.py
```

### Via AppManager (Production)
The dashboard is automatically started by `AppManager` when `main()` calls `manager.start_all()`.

### Configuration

The dashboard reads configuration from:
1. Environment variables (priority)
2. Default values in `AppConfig`

**Environment Variables:**
- `DASHBOARD_HOST` - Host to bind (default: "127.0.0.1")
- `DASHBOARD_PORT` - Port to bind (default: 8081)
- `DASHBOARD_LOG_LEVEL` - Logging level (default: "INFO")
- `DASHBOARD_LOG_FILE` - Log file path (default: "heavengate_dashboard.log")
- `DASHBOARD_ENABLE_LOGGING` - Enable file logging (default: "true")

**Note:** The C++ code uses `DASHBOARD_HOST` and `DASHBOARD_PORT` from `config/default.ini` via `Confparcer`. Make sure these match!

## Dependencies

Install Python dependencies:
```bash
cd Dashboard
pip install -r requirements.txt
```

**Required packages:**
- fastapi==0.104.1
- uvicorn[standard]==0.24.0
- websockets==12.0
- pydantic==2.5.0
- python-multipart==0.0.6

## Migration Checklist

- [x] Update AppComponent to run Python script
- [x] Create run_dashboard.py wrapper
- [x] Verify API endpoint compatibility
- [x] Test WebSocket functionality
- [x] Update documentation
- [ ] Remove Go dashboard code (optional - can keep for reference)
- [ ] Update build scripts if needed
- [ ] Test in production environment

## Troubleshooting

### Dashboard won't start
- Check Python is installed: `python3 --version`
- Check dependencies: `pip list | grep fastapi`
- Check file permissions: `chmod +x run_dashboard.py`

### Static files not loading
- Ensure `Dashboard/static/` directory exists
- Check working directory is set to Dashboard folder
- Verify `index.html` exists in `static/`

### Port already in use
- Change `DASHBOARD_PORT` in config or environment
- Kill existing process: `lsof -ti:8081 | xargs kill`

### C++ can't connect to dashboard
- Verify dashboard is running: `curl http://127.0.0.1:8081/health`
- Check `DASHBOARD_HOST` and `DASHBOARD_PORT` in `config/default.ini` match dashboard config
- Check firewall rules

## Benefits of Python Dashboard

1. **Better async support** - FastAPI is built on async/await
2. **WebSocket native** - Better real-time updates than SSE
3. **Type safety** - Pydantic models for request/response validation
4. **Easier development** - Python is more accessible than Go for many developers
5. **Rich ecosystem** - Access to Python ML libraries if needed for future classification features
6. **Better error handling** - More detailed error messages and stack traces

## Future Enhancements

Potential improvements now that we're on Python:
- Integration with Python ML libraries for request classification
- Better data visualization with Plotly/Bokeh
- Database integration (PostgreSQL client for direct queries)
- Authentication/authorization improvements
- API documentation with Swagger/OpenAPI (FastAPI auto-generates this)
