# DEM Viewer

![License](https://img.shields.io/badge/license-MIT-blue.svg)
![C++](https://img.shields.io/badge/C%2B%2B-17-blue)
![Qt](https://img.shields.io/badge/Qt-6-green)

A simple C++ application using Qt to load and visualize Digital Elevation Models (DEM) in orthographic projection **without OpenGL**. All rendering algorithms are implemented manually.


## Features

- Load DEM files (XYZ format)
- Wireframe rendering
- Interactive transformations: rotation, scaling (incl. Z), translation

## Build

**Requirements:** C++17, Qt 6, CMake >= 3.16

```bash
git clone https://github.com/xDGrishin/DEM_viewer.git
cd DEM_viewer
mkdir build && cd build
cmake ..
make
./DEM_viewer
