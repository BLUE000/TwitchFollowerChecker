# TwitchFollowerChecker

TwitchFollowerChecker is a C++ application built with Qt6 to check and track Twitch followers.

## Features (Proposed)
- Track follower changes (new followers, unfollowers)
- Analyze Twitch stream follower data
- Modern graphical user interface using Qt6

## Getting Started

### Prerequisites
- C++17 or higher compatible compiler (MSVC, GCC, Clang)
- [CMake](https://cmake.org/) (3.16 or higher)
- [Qt6](https://www.qt.io/) (Core, Gui, Widgets)

### Local Configuration
PC-specific environment variables and library paths should be configured in `local_config.cmake` at the root directory.

1. Copy `local_config.cmake.example` to `local_config.cmake`:
   ```bash
   cp local_config.cmake.example local_config.cmake
   ```
2. Open `local_config.cmake` and configure your Qt6 path.

### Building the Project
```bash
mkdir build
cd build
cmake ..
cmake --build .
```

## Documentation
Additional project documentation is located in the [doc/](doc/) directory.

## License
This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
