@echo off
rem Created by Claude Opus 5.5 | Owner: Julian Martus | Review priority: Low | Reviewed by: None
rem
rem Serves this folder on http://localhost:8765 and opens it in the default browser.
rem localhost is a secure context, so Web Bluetooth works. Keep the port fixed: the
rem downloaded logs are stored per origin (http://localhost:8765), another port starts
rem with an empty cache. Close this window to stop the server.

setlocal
set PORT=8765
set PY=C:\Program Files\KiCad\10.0\bin\python.exe
if not exist "%PY%" (
  where python >nul 2>nul && (set PY=python) || (
    echo Python not found. Install KiCad 10 or Python 3, or open KalliFunghi.html directly.
    pause
    exit /b 1
  )
)
cd /d "%~dp0"
start "" "http://localhost:%PORT%/"
echo KalliFunghi Sync on http://localhost:%PORT%/  -  close this window to stop.
"%PY%" -m http.server %PORT% --bind 127.0.0.1
