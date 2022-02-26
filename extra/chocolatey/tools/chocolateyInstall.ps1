$ErrorActionPreference = "Stop"
# set variables
$shortVersion = '[[ShortVersion]]'

$packDir = "$(Split-Path -Parent $MyInvocation.MyCommand.Definition | Split-Path -Parent)"

$packageArgs = @{
	PackageName = 'neko'
	UnzipLocation = "$packDir"
	Url = "$packDir\neko-$shortVersion-win.zip"
	Checksum = '[[Checksum32]]'
	ChecksumType = 'sha256'
	Url64bit = "$packDir\neko-$shortVersion-win64.zip"
    Checksum64 = '[[Checksum64]]'
    ChecksumType64 = 'sha256'
}

Install-ChocolateyZipPackage @packageArgs

$nekoDir = "$(Get-Item "$packDir/neko-$shortVersion-win*" -Exclude "*.zip")"

# Install the dll files to C:\ProgramData\chocolatey\bin
# It is because they are loaded by other neko binaries, e.g. haxelib.exe
$chocoBin = Join-Path $env:ChocolateyInstall 'bin'
$dllFiles = @('gcmt-dll.dll', 'neko.dll')
foreach ($file in $dllFiles) {
    Copy-Item "$nekoDir\$file" "$chocoBin"
}

# Set NEKOPATH such that the ndll files can be loaded.
Install-ChocolateyEnvironmentVariable -VariableName NEKOPATH -VariableValue $nekoDir
