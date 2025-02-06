@echo off

set godot_version=4.3
set agility_sdk_version=1.614.0

echo.
echo --------------------------------------------------------------------------------
echo This script sets up optimized Direct3D 12 support on Windows 10 for Godot [96m%godot_version%[0m.

echo This script will download and install an optional library for enhanced Direct3D 12 support.
echo The library will also be installed in the export templates folder, so they can be
echo copied on export when a project is configured to use Direct3D 12.
echo.
echo This library is not as useful on Windows 11, but installing it will still benefit
echo players on Windows 10 for projects you export from this PC.
echo --------------------------------------------------------------------------------
echo.

echo.
echo [1mDownloading DirectX 12 Agility SDK release %agility_sdk_version%...[0m
curl.exe -L https://www.nuget.org/api/v2/package/Microsoft.Direct3D.D3D12/%agility_sdk_version% -o "%TEMP%\agility_sdk.zip"
echo Download completed.
echo Extracting DirectX 12 Agility SDK...
call :unzipfile "%TEMP%\agility_sdk" "%TEMP%\agility_sdk.zip"
del "%TEMP%\agility_sdk.zip"
mkdir "%APPDATA%\Godot" 2>nul
mkdir "%APPDATA%\Godot\export_templates" 2>nul
mkdir "%APPDATA%\Godot\export_templates\%godot_version%" 2>nul
mkdir x64 2>nul
mkdir arm64 2>nul
mkdir x86 2>nul
copy "%TEMP%\agility_sdk\build\native\bin\x64\D3D12Core.dll" %APPDATA%\Godot\export_templates\%godot_version%\D3D12Core.x64.dll >nul
move "%TEMP%\agility_sdk\build\native\bin\x64\D3D12Core.dll" x64\D3D12Core.dll >nul
copy "%TEMP%\agility_sdk\build\native\bin\arm64\D3D12Core.dll" %APPDATA%\Godot\export_templates\%godot_version%\D3D12Core.arm64.dll >nul
move "%TEMP%\agility_sdk\build\native\bin\arm64\D3D12Core.dll" arm64\D3D12Core.dll >nul
copy "%TEMP%\agility_sdk\build\native\bin\win32\D3D12Core.dll" %APPDATA%\Godot\export_templates\%godot_version%\D3D12Core.x86.dll >nul
move "%TEMP%\agility_sdk\build\native\bin\win32\D3D12Core.dll" x86\D3D12Core.dll >nul
copy "%TEMP%\agility_sdk\build\native\bin\x64\d3d12SDKLayers.dll" %APPDATA%\Godot\export_templates\%godot_version%\d3d12SDKLayers.x64.dll >nul
move "%TEMP%\agility_sdk\build\native\bin\x64\d3d12SDKLayers.dll" x64\d3d12SDKLayers.dll >nul
copy "%TEMP%\agility_sdk\build\native\bin\arm64\d3d12SDKLayers.dll" %APPDATA%\Godot\export_templates\%godot_version%\d3d12SDKLayers.arm64.dll >nul
move "%TEMP%\agility_sdk\build\native\bin\arm64\d3d12SDKLayers.dll" arm64\d3d12SDKLayers.dll >nul
copy "%TEMP%\agility_sdk\build\native\bin\win32\d3d12SDKLayers.dll" %APPDATA%\Godot\export_templates\%godot_version%\d3d12SDKLayers.x86.dll >nul
move "%TEMP%\agility_sdk\build\native\bin\win32\d3d12SDKLayers.dll" x86\d3d12SDKLayers.dll >nul
del /s /q "%TEMP%\agility_sdk" >nul 2>&1
echo Done installing Agility SDK libraries.

echo.
echo --------------------------------------------------------------------------------
echo [92mSuccess! Optimized Direct3D 12 support is now enabled for this Godot installation (%godot_version%).[0m
echo.
echo The Direct3D 12 Agility SDK will automatically be copied along with a project
echo exported for Windows if the project is configured to use Direct3D 12, or if the
echo `application/export_d3d12` option is enabled in the Windows export preset
echo in a given project.
echo.
echo [1mAfter updating Godot to a newer version, remember to run the Direct3D 12[0m
echo [1msetup script that comes with it to install the Agility SDK[0m
echo [1mcorresponding to the new Godot version. Each Godot version stores
echo [1mDirect3D 12 libraries in a version-specific folder to be copied when exporting.[0m
echo --------------------------------------------------------------------------------
pause
exit /b 0

:unzipfile <extractTo> <newZipFile>
set vbs="%TEMP%\_.vbs"
if exist %vbs% del /f /q %vbs%
>%vbs%  echo Set fso = CreateObject("Scripting.FileSystemObject")
>>%vbs% echo If NOT fso.FolderExists(%1) Then
>>%vbs% echo fso.CreateFolder(%1)
>>%vbs% echo End If
>>%vbs% echo Set objShell = CreateObject("Shell.Application")
>>%vbs% echo Set filesInZip=objShell.NameSpace(%2).items
>>%vbs% echo objShell.NameSpace(%1).CopyHere(filesInZip)
>>%vbs% echo Set fso = Nothing
>>%vbs% echo Set objShell = Nothing
cscript //nologo %vbs%
if exist %vbs% del /f /q %vbs%
goto :eof
