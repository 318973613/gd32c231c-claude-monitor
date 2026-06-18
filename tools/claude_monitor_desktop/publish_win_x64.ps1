$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$project = Join-Path $root "ClaudeBoardMonitor.csproj"
$hookProject = Join-Path (Split-Path -Parent $root) "claude_monitor_hook\ClaudeBoardHook.csproj"
$out = Join-Path $root "publish\win-x64"
$zip = Join-Path $root "ClaudeBoardMonitor-win-x64.zip"

dotnet publish $project `
  -c Release `
  -r win-x64 `
  --self-contained true `
  -p:PublishSingleFile=true `
  -p:IncludeNativeLibrariesForSelfExtract=true `
  -p:EnableCompressionInSingleFile=true `
  -o $out

dotnet publish $hookProject `
  -c Release `
  -r win-x64 `
  --self-contained true `
  -p:PublishSingleFile=true `
  -p:IncludeNativeLibrariesForSelfExtract=true `
  -p:EnableCompressionInSingleFile=true `
  -o $out

Copy-Item (Join-Path $root "README.md") (Join-Path $out "README.md") -Force
Remove-Item (Join-Path $out "*.pdb") -ErrorAction SilentlyContinue
Remove-Item $zip -ErrorAction SilentlyContinue
Compress-Archive -Path (Join-Path $out "*") -DestinationPath $zip -Force

Write-Host "Published:" (Join-Path $out "ClaudeBoardMonitor.exe")
Write-Host "Published:" (Join-Path $out "ClaudeBoardHook.exe")
Write-Host "Package:" $zip
