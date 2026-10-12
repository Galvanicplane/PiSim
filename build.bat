@echo off
echo ===================================================
echo [PiSim] Unreal Engine 5.7 Derleme Baslatiliyor...
echo ===================================================

set ENGINE_PATH=C:\UE_5.7
set UPROJECT_PATH=%~dp0PiSim.uproject

if not exist "%ENGINE_PATH%\Engine\Build\BatchFiles\Build.bat" (
    echo [HATA] Unreal Engine 5.7 yolu bulunamadi: %ENGINE_PATH%
    pause
    exit /b 1
)

call "%ENGINE_PATH%\Engine\Build\BatchFiles\Build.bat" PiSimEditor Win64 Development "%UPROJECT_PATH%" -WaitMutex

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ===================================================
    echo [BASARILI] PiSimEditor derlemesi basariyla tamamlandi!
    echo ===================================================
) else (
    echo.
    echo ===================================================
    echo [HATA] Derleme basarisiz oldu! Yukaridaki hata loglarini inceleyin.
    echo ===================================================
)

echo.
pause
