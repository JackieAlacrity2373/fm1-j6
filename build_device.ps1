# Builds build\j6.fwsc with Docker Desktop running.
# Toolchain and SDK files live in ..\FM1-J6-toolchain (outside the project, so the register lint doesn't scan them):
#   jieli-linux-toolchains-*\   from https://pkgman.jieliapp.com/s/linux-toolchain (symlinks materialised as copies on Windows)
#   ac79-sdk\cpu\wl82\tools\    uboot.boot, cfg_tool.bin, cfg\eq_cfg_hw.bin (SHA-256 pinned by build.py)
$r = $PSScriptRoot
$t = Join-Path (Split-Path $r) "FM1-J6-toolchain"
$env:PYTHONUTF8 = "1"
$env:PYTHONPATH = "$r\shim"
$env:JIELI_TOOLCHAIN = (Get-ChildItem $t -Directory -Filter "jieli-linux-toolchains-*" | Select-Object -First 1).FullName
$env:JIELI_DOCKER = "1"
$env:AC79_SDK = "$t\ac79-sdk"
Set-Location "$r\fm1-base"
python tools/build.py .. @args
