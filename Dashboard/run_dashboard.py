#!/usr/bin/env python3
"""
HeavenGate Dashboard Launcher
This script ensures proper setup and launches the dashboard
"""
import sys
import os
from pathlib import Path

# Add Dashboard directory to path for imports
dashboard_dir = Path(__file__).parent
sys.path.insert(0, str(dashboard_dir))

# Change to dashboard directory for static files
os.chdir(dashboard_dir)

# Import and run dashboard
from dashbord import HeavenGateDashboard, AppConfig

def main():
    # Read configuration from environment or use defaults
    import os
    
    config = AppConfig(
        host=os.getenv("DASHBOARD_HOST", "127.0.0.1"),
        port=int(os.getenv("DASHBOARD_PORT", "8081")),
        log_level=os.getenv("DASHBOARD_LOG_LEVEL", "INFO"),
        log_file=os.getenv("DASHBOARD_LOG_FILE", "heavengate_dashboard.log"),
        enable_logging=os.getenv("DASHBOARD_ENABLE_LOGGING", "true").lower() == "true"
    )
    
    dashboard = HeavenGateDashboard(config)
    dashboard.run()

if __name__ == "__main__":
    main()
