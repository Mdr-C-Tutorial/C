$ErrorActionPreference = "Stop"
$PSNativeCommandUseErrorActionPreference = $false

$compilerRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$repoRoot = Resolve-Path (Join-Path $compilerRoot "..\..\..\..")
$exe = Join-Path $compilerRoot "build\mdrcc.exe"
$examples = Resolve-Path (Join-Path $repoRoot "docs\*\*\*\Mdr_Lang\example")

if (-not (Test-Path $exe)) {
    throw "missing executable: $exe"
}

Get-ChildItem $examples -Filter *.mdr | ForEach-Object {
    & $exe $_.FullName
    if ($LASTEXITCODE -ne 0) {
        throw "parse failed: $($_.FullName)"
    }
}

function Assert-CompileFail {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Code
    )

    $tempFile = Join-Path $env:TEMP ("mdr-semantic-" + [guid]::NewGuid().ToString("N") + ".mdr")
    try {
        [System.IO.File]::WriteAllText($tempFile, $Code, [System.Text.UTF8Encoding]::new($false))
        $cmd = '"' + $exe + '" "' + $tempFile + '" >nul 2>nul'
        cmd /c $cmd | Out-Null
        if ($LASTEXITCODE -eq 0) {
            throw "expected semantic failure for: $Code"
        }
    } finally {
        Remove-Item -Path $tempFile -ErrorAction SilentlyContinue -Force
    }
}

Assert-CompileFail "break;"
Assert-CompileFail "function main() -> () { x = 1; }"
Assert-CompileFail "function foo(a: i32) -> () { } function main() -> () { foo(); }"
Assert-CompileFail "struct Vec2(a: i32, b: i32); function main() -> () { Vec2(1); }"
Assert-CompileFail "function main() -> () { let x: i32 = 1; x = true; }"
Assert-CompileFail "function main() -> () { let x: bool = true; x += 1; }"
Assert-CompileFail "function main() -> () { let x = true + 1; }"
Assert-CompileFail "function main() -> () { if (1) { } }"
Assert-CompileFail "function bad() -> i32 { return true; }"
Assert-CompileFail "function bad() -> i32 { return; }"
Assert-CompileFail "struct Box(v: i32); function main() -> () { let b = Box(1); b.v = true; }"
Assert-CompileFail "struct Box(v: i32); function main() -> () { Box(1).v(); }"
Assert-CompileFail "struct Calc(v: i32); function Calc.as_bool() -> bool { return true; } function main() -> () { let x: i32 = Calc(1).as_bool(); }"

Write-Host "smoke test passed"
