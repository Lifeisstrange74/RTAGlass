@echo off
rem Builds RTAGlass from your own copy of Road Trip (PS2, USA). Usage:
rem     build.bat "PATH\TO\Road Trip (USA).cue"     (or a .bin or .iso; --help for the options)
rem Needs Python 3.11 or later; everything else is checked by scripts\build\build.py.
setlocal
cd /d "%~dp0"

set "PY="
where py >nul 2>nul && set "PY=py -3"
if not defined PY (
    where python >nul 2>nul && set "PY=python"
)
if not defined PY goto nopython
%PY% -c "import sys; sys.exit(0 if sys.version_info >= (3, 11) else 1)" >nul 2>nul || goto nopython

rem A private Python environment for the build's packages (requirements.txt), remade when that file changes.
if not exist ".venv\Scripts\python.exe" (
    echo Creating .venv for the build's Python packages...
    %PY% -m venv .venv || goto venvfailed
)
fc /b requirements.txt .venv\requirements.txt >nul 2>nul
if errorlevel 1 (
    echo Installing the Python packages in requirements.txt...
    ".venv\Scripts\python.exe" -m pip install --disable-pip-version-check -q -r requirements.txt || goto venvfailed
    copy /y requirements.txt .venv\requirements.txt >nul
)

".venv\Scripts\python.exe" -I scripts\build\build.py %*
set "RESULT=%errorlevel%"
rem With no arguments the build asked its questions, likely in a window of its own: keep it open to read the end.
if "%~1"=="" pause
exit /b %RESULT%

:nopython
echo Python 3.11 or later is needed: https://www.python.org/downloads/windows/ (tick "Add python.exe to PATH",
echo or keep the py launcher it installs by default).
exit /b 1

:venvfailed
echo Couldn't set up the Python packages in .venv. Delete the .venv folder and try again; if pip can't reach the
echo internet, install the packages in requirements.txt yourself.
exit /b 1
