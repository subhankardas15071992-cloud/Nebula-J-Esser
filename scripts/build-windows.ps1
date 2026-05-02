param(
    [ValidateSet("Debug", "Release", "RelWithDebInfo")]
    [string] $Config = "Release",
    [string] $BuildDir = "build/windows-x64",
    [string] $JucePath = "",
    [string] $ClapExtensionsPath = ""
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$args = @(
    "-S", $root,
    "-B", (Join-Path $root $BuildDir),
    "-G", "Visual Studio 17 2022",
    "-A", "x64",
    "-DNJ_BUILD_TESTS=ON",
    "-DNJ_ENABLE_CLAP=ON"
)

if ($JucePath -ne "") { $args += "-DNJ_JUCE_PATH=$JucePath" }
if ($ClapExtensionsPath -ne "") { $args += "-DNJ_CLAP_EXTENSIONS_PATH=$ClapExtensionsPath" }

cmake @args
cmake --build (Join-Path $root $BuildDir) --config $Config --parallel
ctest --test-dir (Join-Path $root $BuildDir) -C $Config --output-on-failure
