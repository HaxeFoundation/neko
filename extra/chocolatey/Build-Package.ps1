param ([string] $version, [string] $longVersion)
$ErrorActionPreference = "Stop"

If ( $version -eq "" ) {
	Write-Error "No version parameter was passed in."
	Exit 1
}
If ( $longVersion -eq "" ) {
	Write-Error "No longVersion parameter was passed in."
	Exit 1
}

$DIR = "$(Split-Path -parent $MyInvocation.MyCommand.Definition)"

# Create chocolatey template

$TEMPLATE_DIR = "$env:ChocolateyInstall\templates\neko-template"

If ( Test-Path -Path $TEMPLATE_DIR) {
	Get-ChildItem -Path $TEMPLATE_DIR -File | foreach { $_.Delete()}
} Else {
	New-Item -Path $TEMPLATE_DIR -ItemType "directory" -Force > $null
}

Copy-Item -Path $DIR\* -Recurse -Include neko.nuspec,tools -Destination $TEMPLATE_DIR -Force
Copy-Item .\LICENSE $TEMPLATE_DIR\tools\LICENSE

# Create package contents from template

$file32 = ".\WinVS2017Binaries\neko-$version-win.zip"
$file64 = ".\WinVS2017x64Binaries\neko-$version-win64.zip"

$checksum32 = (Get-FileHash $file32).Hash.ToLower()
$checksum64 = (Get-FileHash $file64).Hash.ToLower()

choco new neko -t neko-template --version $longVersion --out $DIR --force Checksum32=$checksum32 Checksum64=$checksum64 ShortVersion=$version
Copy-Item -Path $file32,$file64 -Destination $DIR\neko

# Package everything

$OUTPUT = "$DIR\pack"
New-Item -Path $OUTPUT -ItemType "directory" -Force > $null
choco pack --version $longVersion $DIR\neko\neko.nuspec -Out $OUTPUT
