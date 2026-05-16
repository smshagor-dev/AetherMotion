# Install Qt6 on Linux

## Typical Packages

Install Qt6 development packages for:

- `Qt6::Widgets`
- `Qt6::Charts`
- `Qt6::OpenGLWidgets`

Examples vary by distro. Common package names are similar to:

- `qt6-base-dev`
- `qt6-charts-dev`

## Configure Example

```bash
cmake -S . -B build_qt \
  -DARX_ENABLE_QT6=ON \
  -DCMAKE_PREFIX_PATH=/usr/lib/x86_64-linux-gnu/cmake
```

## Validate

```bash
cmake --build build_qt --parallel
```

If Qt6 is not found, ARX keeps headless mode buildable and prints a warning with the doc location.
