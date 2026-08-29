@echo off
setlocal
chcp 65001 >nul

call "D:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" x64 -vcvars_ver=14.29.30133
if errorlevel 1 goto :err

:: ===== 加速镜像 =====
set OPENCV_IPPICV_URL=https://ghfast.top/https://raw.githubusercontent.com/opencv/opencv_3rdparty/1224f78da6684df04397ac0f40c961ed37f79ccb/ippicv/
set OPENCV_FFMPEG_URL=https://ghfast.top/https://raw.githubusercontent.com/opencv/opencv_3rdparty/7da61f0695eabf8972a2c302bf1632a3d99fb0d5/ffmpeg/
set OPENCV_ADE_URL=https://ghfast.top/https://github.com/opencv/ade/archive/
set OPENCV_TENGINE_URL=https://ghfast.top/https://github.com/OAID/Tengine/archive/

:: ===== 路径 =====
set CMAKE="D:\Program Files\CMake\bin\cmake.exe"
set NINJA="D:\Program Files\Ninja\ninja.exe"
set SRC=D:\MyCode\opencv-4.8.1

:: ============ Debug ============
echo ============ Debug ============
%CMAKE% -S %SRC% -B %SRC%\build-debug -G Ninja ^
  -DCMAKE_BUILD_TYPE=Debug ^
  -DCMAKE_MAKE_PROGRAM=%NINJA% ^
  -DCMAKE_INSTALL_PREFIX=%SRC%\install-debug ^
  -DCMAKE_PREFIX_PATH=%QT_DIR% ^
  -DBUILD_SHARED_LIBS=ON ^
  -DBUILD_opencv_world=OFF ^
  -DWITH_OPENMP=ON ^
  -DWITH_QT=OFF ^
  -DWITH_IPP=ON ^
  -DBUILD_opencv_python3=OFF ^
  -DBUILD_opencv_java=OFF ^
  -DBUILD_EXAMPLES=OFF ^
  -DBUILD_TESTS=OFF ^
  -DBUILD_PERF_TESTS=OFF ^
  -DOPENCV_GENERATE_PKGCONFIG=ON
if errorlevel 1 goto :err

%CMAKE% --build %SRC%\build-debug -j 8
if errorlevel 1 goto :err

%CMAKE% --install %SRC%\build-debug
if errorlevel 1 goto :err

:: ============ Release ============
echo ============ Release ============
%CMAKE% -S %SRC% -B %SRC%\build-release -G Ninja ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_MAKE_PROGRAM=%NINJA% ^
  -DCMAKE_INSTALL_PREFIX=%SRC%\install-release ^
  -DCMAKE_PREFIX_PATH=%QT_DIR% ^
  -DBUILD_SHARED_LIBS=ON ^
  -DBUILD_opencv_world=OFF ^
  -DWITH_OPENMP=ON ^
  -DWITH_QT=OFF ^
  -DWITH_IPP=ON ^
  -DBUILD_opencv_python3=OFF ^
  -DBUILD_opencv_java=OFF ^
  -DBUILD_EXAMPLES=OFF ^
  -DBUILD_TESTS=OFF ^
  -DBUILD_PERF_TESTS=OFF ^
  -DOPENCV_GENERATE_PKGCONFIG=ON
if errorlevel 1 goto :err

%CMAKE% --build %SRC%\build-release -j 8
if errorlevel 1 goto :err

%CMAKE% --install %SRC%\build-release
if errorlevel 1 goto :err

echo ============ 全部完成 ============
exit /b 0

:err
echo.
echo ============ Build FAILED, check log above ============
exit /b 1

