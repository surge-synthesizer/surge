# build-win.ps1 is a powershell script which lets you do some basic command line builds.
# inspired by build-osx.sh but far less powerful and general.
#
# @baconpaul decided to check this in so he could keep doing windows builds and tests
# fairly easily on various branches but really if you want to use this seriously for development 
# you will have to deal with the various path and configuration issues below. Caveat emptor - this
# is not production build infrastructure at all.

$msBuildExe = "c:\Program Files (x86)\Microsoft Visual Studio\2017\Community\MSBuild\15.0\Bin\MSBuild.exe"
$surgeSln = ".\surge.sln"
$vst2Dir = "C:\users\paul\VST2"

function Build-Surge
{
    Write-Host "Building"
    & "$($msBuildExe)" "$($surgeSln)" /t:Build /m /p:Configuration=Release
}

function Clean-Surge
{
    Write-Host "Building"
    & "$($msBuildExe)" "$($surgeSln)" /t:Clean /m 
}

function Install-Surge
{
    # Copy to APPDATA
    Write-Host "Moving data to $env:LOCALAPPDATA"
    
    Remove-Item "$($env:LOCALAPPDATA)\Surge" -Recurse -Force
    Copy-Item "resources\data\" -Destination "$($env:LOCALAPPDATA)\Surge" -Recurse -Force

    Copy-Item "target\vst2\Release\Surge.dll" -Destination $vst2Dir -Force 

    # TODO: VST3 install with runAs

}

# Clean-Surge
Build-Surge
Install-Surge