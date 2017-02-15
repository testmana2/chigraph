$scriptsdir = $PSScriptRoot

if ($args.length < 1) {
	"Usage: download_llvm.ps1 [Debug|Release]"
}


$type = $args[0]

$url = ""
if ($type.CompareTo("Debug")) {
	$url = "https://github.com/chigraph/chigraph/releases/download/dependencies/llvm-3.9.1-debug-msvc14-win64.zip"
} else {
	$url = "https://github.com/chigraph/chigraph/releases/download/dependencies/llvm-3.9.1-release-msvc14-win64.zip"
}

cd $scriptsdir\..\third_party

# download the zip file
$webclient = New-Object System.Net.WebClient

$webclient.DownloadFile("$url", "$pwd/$llvm-$type.zip")


Expand-Archive "$llvm-$type.zip" .