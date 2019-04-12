git submodule update --init --recursive || goto error

premake5 vs2017 || goto error
REM msbuild /target:Build surge-vst2.vcxproj /maxcpucount /p:Configuration=Release;Platform=x64 || goto error
REM msbuild /target:Build surge-vst3.vcxproj /maxcpucount /p:Configuration=Release;Platform=x64 || goto error
msbuild buildtask.xml /maxcpucount

exit /b 0

:error
echo build failed!
exit /b 1