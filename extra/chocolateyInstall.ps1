$scriptPath = (Split-Path -parent $MyInvocation.MyCommand.Definition)

# Install the dll files to C:\ProgramData\chocolatey\bin
# It is because they are loaded by other neko binaries, e.g. haxelib.exe
$chocoBin = Join-Path $env:ChocolateyInstall 'bin'
$dllFiles = @('gcmt-dll.dll', 'neko.dll')
foreach ($file in $dllFiles) {
    $dllFile = Join-Path $scriptPath $file
    copy "$dllFile" "$chocoBin"
}

# Set NEKOPATH such that the ndll files can be loaded.
Install-ChocolateyEnvironmentVariable -VariableName NEKOPATH -VariableValue $scriptPath