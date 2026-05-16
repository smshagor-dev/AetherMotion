# Install Qt6 on Windows

## Recommended

1. Install Qt Maintenance Tool or use an existing Qt6 SDK.
2. Install a Qt6 kit that includes `Widgets`, `Charts`, and `OpenGLWidgets`.
3. Expose Qt to CMake with one of:
   - `-DCMAKE_PREFIX_PATH="C:/Qt/6.x.x/msvc2022_64"`
   - `-DQt6_DIR="C:/Qt/6.x.x/msvc2022_64/lib/cmake/Qt6"`

## Configure Example

```powershell
cmake -S . -B build_qt `
  -DARX_ENABLE_QT6=ON `
  -DCMAKE_PREFIX_PATH="C:/Qt/6.x.x/msvc2022_64"
```

## Validate

```powershell
cmake --build build_qt --parallel
```

If Qt6 is requested but not found, ARX will emit a clear CMake warning and continue building headless targets.
