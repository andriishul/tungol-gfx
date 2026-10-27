@echo off
if not exist "bin" mkdir bin

for %%f in (*.vert) do (
    if not exist "%%~nf.frag" (
        echo [CRITICAL] Missing matching fragment shader for: %%f
        goto error
    )
    echo Compiling: %%f
    glslangValidator -V --target-env vulkan1.3 -I. -I..\common "%%f" -o "bin\%%~nf.vertex.spv"
    if errorlevel 1 goto error
)

for %%f in (*.frag) do (
    if not exist "%%~nf.vert" (
        echo [CRITICAL] Missing matching vertex shader for: %%f
        goto error
    )
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