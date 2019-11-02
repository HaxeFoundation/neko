$packDir = "$(Split-Path -parent $MyInvocation.MyCommand.Definition)"

Install-ChocolateyZipPackage `
    -PackageName 'neko' `
    -UnzipLocation "$packDir" `
    -Url "$packDir\neko-*-win.zip" `
    -Checksum 'fe5a11350d2dd74338f971d62115f2bd21ec6912f193db04c5d28eb987a50485' `
    -ChecksumType 'sha256' `
    -Url64bit "$packDir\neko-*-win64.zip" `
    -Checksum64 'd09fdf362cd2e3274f6c8528be7211663260c3a5323ce893b7637c2818995f0b' `
    -ChecksumType64 'sha256'

$nekoDir = "$(Get-Item "$packDir/neko-*-win*" -Exclude "*.zip")"

# Install the dll files to C:\ProgramData\chocolatey\bin
# It is because they are loaded by other neko binaries, e.g. haxelib.exe
$chocoBin = Join-Path $env:ChocolateyInstall 'bin'
$dllFiles = @('gcmt-dll.dll', 'neko.dll')
foreach ($file in $dllFiles) {
    Copy-Item "$nekoDir/$file" "$chocoBin"
}

# Set NEKOPATH such that the ndll files can be loaded.
Install-ChocolateyEnvironmentVariable -VariableName NEKOPATH -VariableValue $nekoDir