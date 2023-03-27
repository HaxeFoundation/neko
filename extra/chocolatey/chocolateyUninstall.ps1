# Remove the dll files from C:\ProgramData\chocolatey\bin
$chocoBin = Join-Path $env:ChocolateyInstall 'bin'
$dllFiles = @('gcmt-dll.dll', 'neko.dll')
foreach ($file in $dllFiles) {
    $dllFile = Join-Path $chocoBin $file
    Remove-Item "$dllFile"
}

Uninstall-ChocolateyEnvironmentVariable -VariableName NEKOPATH
