
# Install Chocolatey + tools (CMake, Git, 7zip, Python, VS Build Tools)
Set-ExecutionPolicy -Force Bypass -Scope Process; `
[System.Net.ServicePointManager]::SecurityProtocol = [System.Net.SecurityProtocolType]::Tls12; `
(New-Object System.Net.WebClient).DownloadString('https://chocolatey.org/install.ps1') | Invoke-Expression; `
$env:PATH += ';C:\ProgramData\chocolatey\bin'; `
choco feature enable -n=allowGlobalConfirmation; `
choco install -y cmake make git 7zip python3 visualstudio2022buildtools --package-parameters "--add Microsoft.VisualStudio.Workload.VCTools --includeRecommended --includeOptional --passive --norestart"; `
setx PATH "$env:PATH;C:\Program Files\CMake\bin;C:\Program Files\Git\bin;C:\Program Files\7-Zip;C:\Python311;C:\Python311\Scripts"

$PATH="C:\ProgramData\chocolatey\bin;C:\Program Files\CMake\bin;C:\Program Files\Git\bin;C:\Program Files\7-Zip;C:\Python311;C:\Python311\Scripts;${PATH}"

# Create working and shared directories
New-Item -ItemType Directory -Path C:\IRvana -Force | Out-Null; `
New-Item -ItemType Directory -Path C:\Shared -Force | Out-Null; `
New-Item -ItemType Directory -Path C:\downloads -Force | Out-Null

Set-Location C:\IRvana

# Install Rust nightly
Write-Host "Installing Rust nightly..."; `
$wc = New-Object System.Net.WebClient; `
$wc.DownloadFile('https://static.rust-lang.org/rustup/dist/x86_64-pc-windows-msvc/rustup-init.exe', 'C:\downloads\rustup-init.exe'); `
Start-Process -FilePath C:\downloads\rustup-init.exe -ArgumentList '-y','--default-toolchain','stable' -NoNewWindow -Wait; `
& "$env:USERPROFILE\.cargo\bin\rustup.exe" toolchain install nightly-2024-06-26; `
Remove-Item C:\downloads\rustup-init.exe -Force

# Install Nim 1.6.6
Write-Host "Installing Nim 1.6.6..."; `
$wc = New-Object System.Net.WebClient; `
$wc.DownloadFile('https://nim-lang.org/download/nim-1.6.6_x64.zip', 'C:\downloads\nim.zip'); `
& 'C:\Program Files\7-Zip\7z.exe' x C:\downloads\nim.zip -oC:\IRvana\nim-1.6.6 | Out-Null; `
Remove-Item C:\downloads\nim.zip -Force

# Download and extract LLVM 18.1.5
Write-Host "Downloading LLVM 18.1.5..."; `
$wc = New-Object System.Net.WebClient; `
$llvmUrl = 'https://github.com/llvm/llvm-project/releases/download/llvmorg-18.1.5/clang+llvm-18.1.5-x86_64-pc-windows-msvc.tar.xz'; `
$llvmArchive = 'C:\downloads\llvm.tar.xz'; `
$wc.DownloadFile($llvmUrl, $llvmArchive); `
Write-Host "Extracting the gunzip..."; `
& 'C:\Program Files\7-Zip\7z.exe' x $llvmArchive -oC:\downloads\tmp1 | Out-Null; `
$tarFile = (Get-ChildItem C:\downloads\tmp1 -Filter *.tar | Select-Object -First 1).FullName; `
Write-Host "Extracting the tar archive..."; `
& 'C:\Program Files\7-Zip\7z.exe' x $tarFile -oC:\downloads\tmp2 | Out-Null; `
$llvmFolder = Get-ChildItem C:\downloads\tmp2 | Where-Object { $_.PSIsContainer -and $_.Name -like 'clang+llvm*' } | Select-Object -First 1; `
New-Item -ItemType Directory -Force -Path C:\IRvana\LLVM-18.1.5 | Out-Null; `
Move-Item -Path (Join-Path $llvmFolder.FullName '*') -Destination 'C:\IRvana\LLVM-18.1.5' -Force; `
Remove-Item -Recurse -Force C:\downloads; `
Write-Host "LLVM 18.1.5 extracted to C:\IRvana\LLVM-18.1.5"

# Refresh the paths
$env:Path = [System.Environment]::GetEnvironmentVariable("Path","Machine") + ";" + [System.Environment]::GetEnvironmentVariable("Path","User")

# Install Windows SDK 10.0.26100 (already implemented in vscode install, but just here for backup)
# RUN Write-Host "Downloading and installing Windows SDK 10.0.26100..."; `
#     $wc = New-Object System.Net.WebClient; `
#     $sdkInstaller = "C:\\downloads\\winsdksetup.exe"; `
#     $wc.DownloadFile("https://go.microsoft.com/fwlink/?linkid=2335755", $sdkInstaller); `
#     Write-Host "Installing Windows SDK..."; `
#     Start-Process -FilePath $sdkInstaller -ArgumentList "/quiet", "/norestart" -Wait; `
#     Remove-Item $sdkInstaller -Force; `
#     Write-Host "Windows SDK installed."

# Clone IRvana repository
git clone --depth 1 https://github.com/m3rcer/IRvana.git C:\IRvana\repo 
# git clone --depth 1 https://github.com/Cipher7/IRvana C:\IRvana\repo (commented for testing)

# Set up environment
$PATH="C:\\IRvana\\bin;C:\\IRvana\\clang\\bin;C:\\IRvana\\nim-1.6.6\\bin;C:\\Users\\ContainerUser\\.cargo\\bin;${PATH}"

# Build IRvana.sln (Release)
Write-Host "Building IRvana.sln in Release mode..."; `
$msbuildPaths = @( `
    'C:\\Program Files (x86)\\Microsoft Visual Studio\\2022\\BuildTools\\MSBuild\\Current\\Bin\\MSBuild.exe', `
    'C:\\Program Files (x86)\\Microsoft Visual Studio\\2022\\Community\\MSBuild\\Current\\Bin\\MSBuild.exe', `
    'C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\BuildTools\\MSBuild\\Current\\Bin\\MSBuild.exe' `
); `
$msbuild = $msbuildPaths | Where-Object { Test-Path $_ } | Select-Object -First 1; `
if (-not $msbuild) { throw 'MSBuild.exe not found. Ensure Visual Studio Build Tools are installed correctly.' }; `
Write-Host "Using MSBuild path: $msbuild"; `
Push-Location C:\\IRvana\\repo; `
& $msbuild "IRvana.sln" /p:Configuration=Release /m /verbosity:minimal; `
Pop-Location

# Copy release binaries to root and clean up
Copy-Item -Path "C:\\IRvana\\repo\*" -Destination "C:\\IRvana\\" -Recurse -Force; `
Remove-Item C:\\IRvana\\repo -Recurse -Force; `
Remove-Item C:\\downloads -Recurse -Force -ErrorAction SilentlyContinue; `
Remove-Item "$env:TEMP\\*" -Recurse -Force -ErrorAction SilentlyContinue; `
Write-Host "Cleanup complete. Build artifacts remain in C:\\IRvana\\x64\\Release."

# Generate the vs_env.mk and place it in all the language specific folders
Set-Location C:\IRvana
Write-Host "Generating vs_env.mk..."; `
& 'C:\IRvana\IRgen\c\detect_sdk.bat'; `
Copy-Item -Path C:\IRvana\vs_env.mk -Destination C:\IRvana\IRgen\c\vs_env.mk -Force; `
Copy-Item -Path C:\IRvana\vs_env.mk -Destination C:\IRvana\IRgen\cxx\vs_env.mk -Force; `
Copy-Item -Path C:\IRvana\vs_env.mk -Destination C:\IRvana\IRgen\nim\vs_env.mk -Force; `
Copy-Item -Path C:\IRvana\vs_env.mk -Destination C:\IRvana\IRgen\rust\vs_env.mk -Force;