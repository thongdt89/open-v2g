[CmdletBinding()]
param(
    [string]$BuildDirectory = "build-gcc-windows",
    [ValidateSet("Debug", "Release")]
    [string]$BuildType = "Debug"
)

$ErrorActionPreference = "Stop"
$repositoryRoot = Split-Path -Parent $PSScriptRoot

function Find-Executable {
    param(
        [string]$CommandName,
        [string[]]$Candidates
    )

    $command = Get-Command $CommandName -ErrorAction SilentlyContinue
    if ($null -ne $command) {
        return $command.Source
    }
    foreach ($candidate in $Candidates) {
        if (Test-Path -LiteralPath $candidate) {
            return $candidate
        }
    }
    return $null
}

$cmakeCandidates = @(
    "C:\Program Files\CMake\bin\cmake.exe",
    "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
    "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
    "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
    "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
)
$gccCandidates = @(
    "C:\msys64\ucrt64\bin\gcc.exe",
    "C:\msys64\mingw64\bin\gcc.exe",
    "C:\mingw64\bin\gcc.exe"
)

$cmake = Find-Executable -CommandName "cmake" -Candidates $cmakeCandidates
$gcc = Find-Executable -CommandName "gcc" -Candidates $gccCandidates
if ($null -eq $cmake) {
    throw "CMake was not found. Install CMake or Visual Studio CMake tools."
}
if ($null -eq $gcc) {
    throw "GCC was not found. Install a 64-bit MinGW-w64/MSYS2 GCC toolchain or add gcc.exe to PATH."
}

$gccDirectory = Split-Path -Parent $gcc
$ninja = Find-Executable -CommandName "ninja" -Candidates @(
    (Join-Path $gccDirectory "ninja.exe"),
    "C:\Program Files\CMake\bin\ninja.exe",
    "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
)
$mingwMake = Find-Executable -CommandName "mingw32-make" -Candidates @(
    (Join-Path $gccDirectory "mingw32-make.exe")
)

if ($null -ne $ninja) {
    $generator = "Ninja"
    $generatorArguments = @("-G", $generator, "-DCMAKE_MAKE_PROGRAM:FILEPATH=$ninja")
} elseif ($null -ne $mingwMake) {
    $generator = "MinGW Makefiles"
    $generatorArguments = @("-G", $generator, "-DCMAKE_MAKE_PROGRAM:FILEPATH=$mingwMake")
} else {
    throw "Neither ninja.exe nor mingw32-make.exe was found beside the GCC toolchain or in PATH."
}

$resolvedBuildDirectory = [System.IO.Path]::GetFullPath(
    (Join-Path $repositoryRoot $BuildDirectory))
$configureArguments = @(
    "-S", $repositoryRoot,
    "-B", $resolvedBuildDirectory,
    "-DCMAKE_BUILD_TYPE=$BuildType",
    "-DCMAKE_C_COMPILER:FILEPATH=$gcc",
    "-DOPENV2G_BUILD_TESTS=ON",
    "-DOPENV2G_BUILD_EXAMPLES=ON",
    "-DOPENV2G_BUILD_SIMULATION=ON",
    "-DOPENV2G_WARNINGS_AS_ERRORS=ON"
) + $generatorArguments

Write-Host "Configuring with $generator and $gcc"
& $cmake @configureArguments
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

& $cmake --build $resolvedBuildDirectory --parallel
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

$ctest = Join-Path (Split-Path -Parent $cmake) "ctest.exe"
if (-not (Test-Path -LiteralPath $ctest)) {
    throw "ctest.exe was not found next to cmake.exe."
}
Push-Location $resolvedBuildDirectory
try {
    & $ctest --output-on-failure
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
} finally {
    Pop-Location
}

Write-Host "GCC host/SIL build and tests passed in $resolvedBuildDirectory"
