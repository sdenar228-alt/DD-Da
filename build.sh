#!/bin/bash
# Builds the client into build/Leviathan, the macOS counterpart of build.bat.
#
# Git for Windows cannot store the executable bit, so a checkout made there
# leaves this file unexecutable: run `chmod +x build.sh` once on the Mac, or
# start it as `bash build.sh`.
#
# Uses the Ninja generator for the same reason the Windows script does: CMake
# copies the data folder and the shared libraries into the build directory root
# while it configures, so a multi-config generator like Xcode would put the
# executable into build/Release/ where it finds neither of them.
set -e

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"

# The command line tools carry clang, the macOS SDK and the frameworks the
# client links against. xcode-select answers even when the tools are only half
# installed, so ask xcrun for the compiler itself instead.
if ! xcrun --find clang > /dev/null 2>&1; then
	echo "ERROR: the Xcode command line tools are missing, install them with 'xcode-select --install'." >&2
	exit 1
fi

for TOOL in cmake ninja; do
	if ! command -v "$TOOL" > /dev/null 2>&1; then
		echo "ERROR: $TOOL was not found, install it with 'brew install $TOOL'." >&2
		exit 1
	fi
done

# This repository does not carry ddnet-libs: the folder is gitignored and, while
# .gitmodules still names it, the tree records no submodule for it any more, so
# `git submodule update --init` has nothing to check out and the folder has to
# be cloned by hand. The Discord SDK is only ever found there, so the build
# below cannot go ahead without it, and the error CMake would give instead names
# the missing library rather than the folder that was supposed to hold it.
if [ ! -d "$ROOT/ddnet-libs/discord" ]; then
	echo "ERROR: ddnet-libs is missing, clone it into the source root first:" >&2
	echo "       git clone --depth 1 https://github.com/ddnet/ddnet-libs ddnet-libs" >&2
	exit 1
fi

# PREFER_BUNDLED_LIBS only moves ddnet-libs ahead of the system in the search
# order, so a library it does not carry is still taken from wherever the Mac
# keeps it. It matters because of what comes after this build: the libraries
# Homebrew installs carry absolute /opt/homebrew load commands, and nothing in
# the packaging step rewrites those. A .app built against them runs perfectly on
# the machine that built it and fails to launch on every other one, which is a
# thing to find out now rather than after handing the disk image to somebody.
cmake -S "$ROOT" -B "$ROOT/build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DPREFER_BUNDLED_LIBS=ON -DCLIENT_EXECUTABLE=Leviathan -DDISCORD=ON
cmake --build "$ROOT/build" --target game-client

# What that leaves in build/ is the bare executable, run from that directory
# because the data folder and the libraries sit next to it. It is not the .app
# a Mac expects: the bundle, its Info.plist, its icon and the frameworks copied
# inside it are put together by a separate target, package_dmg, which wraps the
# result in a disk image. That target only exists when dmgbuild was found while
# configuring, so `pip3 install dmgbuild` before running
# `cmake --build build --target package_dmg`.
echo
echo "Built $ROOT/build/Leviathan"
