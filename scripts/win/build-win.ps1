# build-win.ps1 is a powershell script which lets you do some basic command line builds.
# inspired by build-osx.sh but far less powerful and general.
#
# @baconpaul decided to check this in so he could keep doing windows builds and tests
# fairly easily on various branches but really if you want to use this seriously for development 
# you will have to deal with the various path and configuration issues below. Caveat emptor - this
# is not production build infrastructure at all.

Param(
    [switch]$clean,
    [switch]$cleanall,
    [switch]$build,
    [switch]$install,
    [switch]$bi, # build and install
    [switch]$cb, # Clean and Build
    [switch]$cbi # clean build and install
)


$msBuildExe = "c:\Program Files (x86)\Microsoft Visual Studio\2017\Community\MSBuild\15.0\Bin\MSBuild.exe"
$surgeSln = ".\surge.sln"
$userDir = $($env:HOMEDRIVE) + $($env:HOMEPATH)
$vst2Dir = "${userDir}\VST2"

If( -Not ( Test-Path .\Surge.sln ) )
{
    Write-Host "Please run premake"
    return
}

function Build-Surge
{
    Write-Host "Building"
    & "$($msBuildExe)" "$($surgeSln)" /t:Build /m /p:Configuration=Release
}

function Clean-Surge
{
    Write-Host "Cleaning"
    & "$($msBuildExe)" "$($surgeSln)" /t:Clean /m /p:Configuration=Release
}

function Install-Surge
{
    # Copy to APPDATA
    Write-Host "Moving data to $env:LOCALAPPDATA"
    Remove-Item "$($env:LOCALAPPDATA)\Surge" -Recurse -Force
    Copy-Item "resources\data\" -Destination "$($env:LOCALAPPDATA)\Surge" -Recurse -Force

    Write-Host "Installing VST2 into $vst2Dir"
    If(!(Test-Path $vst2Dir))
    {
        New-Item -ItemType Directory -Force -Path $vst2Dir
    }
    Copy-Item "target\vst2\Release\Surge.dll" -Destination $vst2Dir -Force 

    # TODO: VST3 install with runAs

}

if( $bi )
{
    $build = $true
    $install = $true
}

if( $cbi )
{
    $build = $true
    $clean = $true
    $install = $true
}

if( $cb )
{
    $clean = $true
    $build = $true
}
if( $clean )
{
    Write-Host "Clean"
    Clean-Surge
}
if( $cleanall )
{
    Write-Host "CleanAll"
    Clean-Surge
    Write-Host "Delete the visual studio files also"
    Remove-Item -path .\*vcxproj*
    Remove-Item -path .\Surge.sln
}
if( $build )
{
    Write-Host "Build"
    Build-Surge 
}
if( $install )
{
    Write-Host "Install"
    Install-Surge
}