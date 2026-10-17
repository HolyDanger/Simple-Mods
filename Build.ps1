$ErrorActionPreference = 'Stop'
Set-Location -LiteralPath $PSScriptRoot
$dependencies = @(
    @{ name='imgui'; url='https://github.com/ocornut/imgui.git'; commit='4be0b47ecc0edd535a40a79f6ceb6f1ac5936483' },
    @{ name='minhook'; url='https://github.com/TsudaKageyu/minhook.git'; commit='8af6b4acae5a9388fd742b56fa79ece89d96f823' }
)
foreach ($dependency in $dependencies) {
    $path = Join-Path $PSScriptRoot ('vendor\'+$dependency.name)
    if (-not (Test-Path -LiteralPath (Join-Path $path '.git'))) {
        New-Item -ItemType Directory -Force -Path $path | Out-Null
        git -C $path init
        git -C $path remote add origin $dependency.url
        git -C $path fetch --depth 1 origin $dependency.commit
        if ($LASTEXITCODE -ne 0) { throw 'Не удалось загрузить зависимости.' }
        git -C $path checkout --detach FETCH_HEAD
    }
    if ((git -C $path rev-parse HEAD) -ne $dependency.commit) { throw 'Не совпадает версия зависимости. Используйте чистую папку vendor.' }
}
cmake -S . -B build -A x64
if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed.' }
cmake --build build --config Release
if ($LASTEXITCODE -ne 0) { throw 'Build failed.' }
