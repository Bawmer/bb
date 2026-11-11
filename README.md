# BL Oxford Ultra Limiter (BUB LEON)

C++20 / JUCE 8.0.4 mastering limiter — VST3 + Standalone, Windows 11 x64.

## Build on Windows (Visual Studio 2022 + CMake 3.24+)
```
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target BubLeonOxfordUltra_VST3
```
Output: `build\BubLeonOxfordUltra_artefacts\Release\VST3\BL Oxford Ultra Limiter.vst3`
Copy the whole `.vst3` folder to `C:\Program Files\Common Files\VST3\`.

## Build in the cloud
Push to GitHub — `.github/workflows/build-windows.yml` builds with MSVC, runs pluginval,
and attaches `BL_Oxford_Ultra_Limiter_Win64.zip` (VST3 + Standalone .exe) to the run.
