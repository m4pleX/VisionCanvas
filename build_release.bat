@echo off
rem ============================================================
rem  VisionPlatform 编译脚本（Release, MSVC2019 + Ninja）
rem  用法: 双击运行 或 命令行执行
rem  首次运行会自动执行 CMake 配置；之后增量构建。
rem ============================================================

setlocal

set "CMAKE=D:\Program Files\CMake\bin\cmake.exe"
set "NINJA=D:\Program Files\Ninja\ninja.exe"
set "BUILD_DIR=D:\MyCode\VisionPlatform\cmake-build-release-msvc2019"
set "SRC_DIR=D:\MyCode\VisionPlatform"

rem ---- 1. 加载 MSVC 2019 (v142) 编译环境 ----
call "%SRC_DIR%\my_vcvars1429.bat"
if errorlevel 1 (
    echo [ERROR] 加载 MSVC 环境失败
    exit /b 1
)

rem ---- 2. CMake 配置（首次或 CMakeLists 变更时） ----
if not exist "%BUILD_DIR%\CMakeCache.txt" (
    echo [CMake] 首次配置中...
    "%CMAKE%" -S "%SRC_DIR%" -B "%BUILD_DIR%" -G "Ninja" ^
        -DCMAKE_BUILD_TYPE=Release ^
        -DCMAKE_MAKE_PROGRAM="%NINJA%"
    if errorlevel 1 (
        echo [ERROR] CMake 配置失败
        exit /b 1
    )
) else (
    echo [CMake] 已存在构建缓存，跳过配置（如需重新配置请删除 CMakeCache.txt）
)

rem ---- 3. 构建 ----
echo [Build] 开始构建...
"%CMAKE%" --build "%BUILD_DIR%" --config Release
if errorlevel 1 (
    echo [ERROR] 构建失败
    exit /b 1
)

echo.
echo [OK] 构建成功！产物位于 %BUILD_DIR%
endlocal
