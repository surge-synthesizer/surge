$vstDir = "$($env:CommonProgramFiles)\VST3"

Write-Host "Installing into $vstDir"
If(!(Test-Path $vstDir))
{
    New-Item -ItemType Directory -Force -Path $vstDir
}

Copy-Item "$PSScriptRoot\..\..\target\vst3\Release\Surge.vst3" -Destination $vstDir -Force
Copy-Item "$PSScriptRoot\..\..\target\vst3\Release\Surge_x86.vst3" -Destination $vstDir -Force 
