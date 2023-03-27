param ([string] $version)
$ErrorActionPreference = "Stop"

$SOURCE = ".\extra\chocolatey"
$OUTPUT = "$SOURCE\out"

If ( $version -eq "" ) {
	Write-Error "No version parameter was passed in."
	Exit 1
}

# Create empty
If ( Test-Path -Path $OUTPUT) {
	Get-ChildItem -Path $OUTPUT -File | foreach { $_.Delete()}
} Else {
	New-Item -Path $OUTPUT -ItemType "directory" -Force > $null
}

Function Copy-File {
	param ([string] $file)
	Write-Host "Copying $file"
	Copy-Item $file $OUTPUT
}

# Copy over zipped up binaries

$file32 = "neko-$version-win.zip"
$file64 = "neko-$version-win64.zip"

ForEach ($file in @(".\WinVS2017Binaries\$file32", ".\WinVS2017x64Binaries\$file64")) {
	If ( ! (Test-Path -Path $file -PathType Leaf) ) {
		Write-Error "File $file missing"
		Exit 2
	}
	Copy-File $file
}

# Generate install script
Write-Host "Generating install script"

# Load template
$template = (Get-Content -Path "$source\chocolateyInstall.ps1.template" -Raw)

# Get checksums
$checksum32 = (Get-FileHash $OUTPUT\$file32).Hash.ToLower()
$checksum64 = (Get-FileHash $OUTPUT\$file64).Hash.ToLower()

# Generate install script with correct checksums
$installScript = $template -Replace '::CHECKSUM32::',$checksum32 -Replace '::CHECKSUM64::',$checksum64

Out-File -FilePath "$OUTPUT\chocolateyInstall.ps1" -InputObject $installScript -NoNewline

# Copy over general files
$toCopy = @(".\LICENSE", "$SOURCE\VERIFICATION.txt", "$SOURCE\neko.nuspec","$SOURCE\chocolateyUninstall.ps1")

ForEach ($item in $toCopy) {
	Copy-File $item
}
