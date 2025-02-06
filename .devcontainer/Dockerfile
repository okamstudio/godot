FROM mcr.microsoft.com/devcontainers/base:bullseye

# Prepare apt
RUN apt-get update && export DEBIAN_FRONTEND=noninteractive

# Install the dev dependencies
# Taken straight from https://docs.godotengine.org/en/latest/contributing/development/compiling/compiling_for_linuxbsd.html#doc-compiling-for-linuxbsd
RUN apt-get install -y \
  build-essential \
  scons \
  pkg-config \
  libx11-dev \
  libxcursor-dev \
  libxinerama-dev \
  libgl1-mesa-dev \
  libglu-dev \
  libasound2-dev \
  libpulse-dev \
  libudev-dev \
  libxi-dev \
  libxrandr-dev

# Install dependencies for Windows build
# Translated from https://github.com/godotengine/build-containers/blob/main/Dockerfile.windows (fedora) to debian via command-not-found.com
RUN apt-get install -y \ 
    # mingw32-gcc
    gcc-mingw-w64-i686 \
    # mingw32-gcc-c++
    g++-mingw-w64-i686 \
    # mingw32-winpthreads-static - Is this needed?
    # mingw64-gcc
    gcc-mingw-w64-x86-64 \
    # mingw64-gcc-c++
    g++-mingw-w64-x86-64
    # mingw64-winpthreads-static - Is this needed?

# Set to posix
RUN \
  # 64-bit
  update-alternatives --set x86_64-w64-mingw32-gcc $(update-alternatives --list x86_64-w64-mingw32-gcc | grep posix) && \
  update-alternatives --set x86_64-w64-mingw32-g++ $(update-alternatives --list x86_64-w64-mingw32-g++ | grep posix) && \
  # 32-bit
  update-alternatives --set i686-w64-mingw32-gcc $(update-alternatives --list i686-w64-mingw32-gcc | grep posix) && \
  update-alternatives --set i686-w64-mingw32-g++ $(update-alternatives --list i686-w64-mingw32-g++ | grep posix)
