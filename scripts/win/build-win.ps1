# build-win.ps1 is a powershell script which lets you do some basic command line builds.
# inspired by build-osx.sh but far less powerful and general.
#
# @baconpaul decided to check this in so he could keep doing windows builds and tests
# fairly easily on various branches but really if you want to use this seriously for development 
# you will have to deal with the various path and configuration issues below. Caveat emptor - this
# is not production build infrastructure at all.

Param(
    [switch]$help,
    [switch]$clean,
    [switch]$cleanall,
    [switch]$build,
    [switch]$install,
    [switch]$bi, # build and install
    [switch]$cb, # Clean and Build
    [switch]$cbi, # clean build and install
    [switch]$w32
)

function Show-Help
{
    Write-Host @"
build-win.ps1 is a powershell script to control builds and do installs. It takes several arguments

    -help       Show this screen
    -clean      Clean builds
    -cleanall   Clean builds and remove visual studio files
    -build      Build
    -install    Install vst2 and 3. Will prompt for permissions
    -bi         buid + install
    -cb         clean + build
    -cbi        clean + build + install

    -w32        Build 32 bit

It does not run premake yet. 

You need to onetime do

    Set-ExecutionPolicy -scope CurrentUser RemoteSigned 

to use this.
"@
}

$msBuildExe = "c:\Program Files (x86)\Microsoft Visual Studio\2017\Community\MSBuild\15.0\Bin\MSBuild.exe"
$surgeSln = ".\surge.sln"
$userDir = $($env:HOMEDRIVE) + $($env:HOMEPATH)
$vst2Dir = "${userDir}\VST2"
$platform = "/p:Platform=x64"

if( $w32 )
{
    Write-Host "Building 32 bit windows"
    $platform = "/p:Platform=Win32"
}


function Build-Surge
{
    Write-Host "Building"
    & "$($msBuildExe)" "$($surgeSln)" /t:Build /m /p:Configuration=Release "$($platform)"
}

function Clean-Surge
{
    Write-Host "Cleaning"
    & "$($msBuildExe)" "$($surgeSln)" /t:Clean /m /p:Configuration=Release "$($platform)"
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
    Copy-Item "target\vst2\Release\Surge_x86.dll" -Destination $vst2Dir -Force 

    Write-Host "Start-Process -Verb runAs -WorkingDirectory $PSScriptRoot powershell -argumentlist install-vst3.ps1"
    Start-Process -Verb runAs   "powershell" -argumentlist "$PSScriptRoot\install-vst3.ps1"
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
    Remove-Item -path .\target -recurse
    Remove-Item -path .\obj -recurse  
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

if( $help )
{
    Show-Help
}