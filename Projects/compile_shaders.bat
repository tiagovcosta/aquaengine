for /r .\data\source\shaders\ %%i in (*.shader) do ..\Tools\ShaderCompiler\Bin\x64\Release\ShaderCompiler.exe --data-dir=data\ %%~ni
::for /r .\data\source\shaders\ %%i in (*.shader) do ..\Tools\ShaderCompiler\Bin\x64\Release\ShaderCompiler.exe --data-dir=data\ --debug %%~ni
PAUSE