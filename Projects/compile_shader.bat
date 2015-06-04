::for /r %%i in (.\data\*.shader) do ..\Tools\ShaderCompiler\Bin\x64\Release\ShaderCompiler.exe --data-dir=data\ %%i
::for /r .\data\ %%i in (*.shader) do @echo %%i
..\Tools\ShaderCompiler\Bin\x64\Release\ShaderCompiler.exe --data-dir=data\
PAUSE