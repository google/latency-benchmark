@echo off
pushd "%~dp0"
python third_party/gyp/gyp ^
    --depth=. ^
    --generator-output=build ^
    -G msvs_version=2012e ^
    -f make ^
    -f xcode ^
    -f msvs
popd
