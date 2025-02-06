#!/usr/bin/env sh
set -eu
IFS=$(printf '\n\t')

print_info() {
	printf "\e[1m%b\e[0m\n" "$1"
}

print_success() {
	printf "\e[1;92m%b\e[0m\n" "$1"
}

print_error() {
	printf "\e[1;91mERROR: \e[22m%b\e[0m\n" "$1"
}

# The detection below also covers derivatives such as Linux Mint and Manjaro.
if [ -f "/etc/alpine-release" ]; then
	distribution="alpine"
	distribution_name="\e[94mAlpine"
elif [ -f "/etc/arch-release" ]; then
	distribution="arch"
	distribution_name="\e[96mArch Linux"
elif [ -f "/etc/fedora-release" ]; then
	distribution="fedora"
	distribution_name="\e[94mFedora"
elif [ -f "/etc/gentoo-release" ]; then
	distribution="gentoo"
	distribution_name="\e[95mGentoo"
elif [ -f "/etc/mageia-release" ]; then
	distribution="mageia"
	distribution_name="\e[94mMageia"
elif grep -q openSUSE /etc/os-release; then
	distribution="opensuse"
	distribution_name="\e[92mopenSUSE"
elif [ -f "/etc/solus-release" ]; then
	distribution="solus"
	distribution_name="\e[96mSolus"
elif [ -f "/etc/lsb-release" ]; then
	# Check for lsb-release last as distributions like Mageia also have this file.
	# Ubuntu must be checked before checking for Debian as Ubuntu also has `/etc/debian_version`.
	distribution="ubuntu"
	distribution_name="\e[38;5;208mUbuntu"
elif [ -f "/etc/debian_version" ]; then
	distribution="debian"
	distribution_name="\e[91mDebian"
elif [ "$(uname -s)" = "FreeBSD" ]; then
	distribution="freebsd"
	distribution_name="\e[91mFreeBSD"
elif [ "$(uname -s)" = "OpenBSD" ]; then
	distribution="openbsd"
	distribution_name="\e[93mOpenBSD"
elif [ "$(uname -s)" = "NetBSD" ]; then
	distribution="netbsd"
	distribution_name="\e[38;5;208mNetBSD"
else
	print_error "Couldn't detect your Linux/*BSD distribution. No dependencies have been installed.\n       You'll have to install dependencies manually using your distribution's package manager:\n\n       https://docs.godotengine.org/en/latest/contributing/development/compiling/compiling_for_linuxbsd.html"
	exit 1
fi

print_info "Detected Linux/*BSD distribution: $distribution_name"

if [ -n "$(command -v sudo)" ]; then
	# Use `sudo` when needed.
	sudo="sudo"
	print_info "Installing dependencies with sudo... (you may be prompted for your password)"
else
	# Don't use `sudo`.
	sudo=""
	print_info "Installing dependencies..."
fi

case "$distribution" in
alpine)
	$sudo apk add \
		scons \
		pkgconf \
		gcc \
		g++ \
		libx11-dev \
		libxcursor-dev \
		libxinerama-dev \
		libxi-dev \
		libxrandr-dev \
		mesa-dev \
		eudev-dev \
		alsa-lib-dev \
		pulseaudio-dev
	;;
arch)
	$sudo pacman -Sy --noconfirm --needed \
		scons \
		pkgconf \
		gcc \
		libxcursor \
		libxinerama \
		libxi \
		libxrandr \
		wayland-utils \
		mesa \
		glu \
		libglvnd \
		alsa-lib \
		pulseaudio
	;;
debian | ubuntu)
	export DEBIAN_FRONTEND="noninteractive"
	$sudo apt-get update
	$sudo apt-get install -y \
		build-essential \
		scons \
		pkg-config \
		libx11-dev \
		libxcursor-dev \
		libxinerama-dev \
		libgl1-mesa-dev \
		libglu1-mesa-dev \
		libasound2-dev \
		libpulse-dev \
		libudev-dev \
		libxi-dev \
		libxrandr-dev \
		libwayland-dev
	;;
fedora)
	$sudo dnf install -y \
		scons \
		pkgconfig \
		libX11-devel \
		libXcursor-devel \
		libXrandr-devel \
		libXinerama-devel \
		libXi-devel \
		wayland-devel \
		mesa-libGL-devel \
		mesa-libGLU-devel \
		alsa-lib-devel \
		pulseaudio-libs-devel \
		libudev-devel \
		gcc-c++ \
		libstdc++-static \
		libatomic-static
	;;
freebsd)
	$sudo pkg install \
		py37-scons \
		pkgconf \
		xorg-libraries \
		libXcursor \
		libXrandr \
		libXi \
		xorgproto \
		libGLU \
		alsa-lib \
		pulseaudio
	;;
gentoo)
	$sudo emerge --sync
	$sudo emerge -an \
		dev-build/scons \
		x11-libs/libX11 \
		x11-libs/libXcursor \
		x11-libs/libXinerama \
		x11-libs/libXi \
		dev-util/wayland-scanner \
		media-libs/mesa \
		media-libs/glu \
		media-libs/alsa-lib \
		media-sound/pulseaudio
	;;
mageia)
	$sudo urpmi --auto \
		scons \
		task-c++-devel \
		wayland-devel \
		"pkgconfig(alsa)" \
		"pkgconfig(glu)" \
		"pkgconfig(libpulse)" \
		"pkgconfig(udev)" \
		"pkgconfig(x11)" \
		"pkgconfig(xcursor)" \
		"pkgconfig(xinerama)" \
		"pkgconfig(xi)" \
		"pkgconfig(xrandr)"
	;;
netbsd)
	$sudo pkg_add \
		pkg-config \
		py37-scons
	;;
openbsd)
	$sudo pkg_add \
		python \
		scons \
		llvm
	;;
opensuse)
	$sudo zypper install -y \
		scons \
		pkgconfig \
		libX11-devel \
		libXcursor-devel \
		libXrandr-devel \
		libXinerama-devel \
		libXi-devel \
		wayland-devel \
		Mesa-libGL-devel \
		alsa-devel \
		libpulse-devel \
		libudev-devel \
		gcc-c++ \
		libGLU1
	;;
solus)
	$sudo eopkg install -y \
		-c system.devel \
		scons \
		libxcursor-devel \
		libxinerama-devel \
		libxi-devel \
		libxrandr-devel \
		wayland-devel \
		mesalib-devel \
		libglu \
		alsa-lib-devel \
		pulseaudio-devel
	;;
esac

# shellcheck disable=SC2016
print_success 'Dependencies successfully installed! You can now build Godot using the `scons` command.'
