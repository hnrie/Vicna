@echo off
for /f "tokens=2" %%a in ('tasklist ^| findstr /i "RobloxPlayerBeta.exe"') do (
    Loader.exe %%a
)
exit /b