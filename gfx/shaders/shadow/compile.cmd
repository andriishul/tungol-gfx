@echo off
setlocal

if not exist "bin" mkdir bin

for %%f in (*.vert) do (
    echo Compiling: %%f
    glslangValidator -V --target-env vulkan1.3 -I. -I..\common "%%f" -o "bin\%%~nf.vertex.spv"
    if errorlevel 1 goto error
)

for %%f in (*.frag) do (
    echo Compiling: %%f
    glslangValidator -V --target-env vulkan1.3 -I. -I..\common "%%f" -o "bin\%%~nf.frag.spv"
    if errorlevel 1 goto error
)

for %%f in (*.comp) do (
    echo Compiling: %%f
    glslangValidator -V --target-env vulkan1.3 -I. -I..\common "%%f" -o "bin\%%~nf.comp.spv"
    if errorlevel 1 goto error
)

echo Done.
pause
exit /b 0

:error
echo.
echo [CRITICAL] Shader compilation failed. Check errors above.
pause
exit /b 1