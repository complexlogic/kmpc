# Kodi M3U Playlist Converter
Kodi M3U Playlist Converter (kmpc) is a desktop application that converts M3U music playlists for use in Kodi by changing file paths to Kodi music database URLs. kmpc is compatible with Windows, macOS, and Linux.

## Screenshot


## Installation
### Windows
Download the win64 zip file from the latest release and extract its contents to a directory of your choice.

### macOS
There is a Homebrew formula available for macOS users. Make sure you have the latest available Xcode installed, as well as Homebrew. Then, execute the following command:
```
brew install complexlogic/tap/kmpc
```
### Linux
The Linux version is distributed as source only. See [Building](#building).

## Usage
kmpc converts your M3U music playlists by changing the file paths to Kodi database URLs. Database-based playlists provide better integration with Kodi than path-based playlists. However, this means that kmpc needs access your Kodi music database file. If you run Kodi on a different PC, you will need to either copy the database to the computer running kmpc, or share it via a network filesystem. As of v19 Matrix, the Kodi music database file is named `MyMusic82.db`.

It is important to ensure that every song in your input M3U playlists exists in your Kodi music database. If a single song from an input M3U playlist does not exist in the database, the conversion of that playlist will fail.

kmpc assumes you have all of your M3U playlists contained in a single folder, which you can browse for in the settings. You also must set the path to the output folder, and the path to the Kodi music database file. 

#### Target System
This setting tells kmpc which system you're running Kodi on (not which system the playlists were created on). Set this to Windows if you're running Kodi on Windows, and Unix for all other platforms. This setting is needed so kmpc can convert path separators if necessary (e.g. \ to / if you're going from Windows to Unix).

#### File Extension
There are two types of M3U playlists: the default `.m3u` and the UTF-8 econded `.m3u8`. As of v19 Matrix, only `.m3u` is supported in Kodi, but `.m3u8` support is planned for v20 Nexus. The file extension setting can be used convert the file extension if necessary. However, this does not affect the encoding of the playlist contents.

#### Path Replacement
If you run Kodi on a different system than the one the playlists were created on, you will likely need to do path replacement. This setting applies a text replacement for every path in your M3U playlists before looking it up in the database. For example, if you share your music via an NFS share:
```
Input:  /home/user/Music
Output: nfs://192.168.1.10/home/user/Music
```
If you are unsure of the correct output path, you can use a graphical database viewer such as [Sqlite DB Browser](https://github.com/sqlitebrowser/sqlitebrowser) to check what Kodi has stored the path as, and update the kmpc path replacement output accordingly.

## Building
The following dependencies are required to build kmpc:
- Qt6 for Windows/Mac. The Linux version can use Qt5 or Qt6 but defaults to Qt5
- fmt
- sqlite3

### Linux/Mac
Make sure you have the dependencies installed.

**Debian/Ubuntu**
```
sudo apt install qtbase5-dev libfmt-dev sqlite3
```
**Arch/Majaro**
```
sudo pacman -S qt5-base fmt sqlite
```
**Fedora**
```
sudo dnf install qt5-qtbase-devel fmt-devel sqlite-devel
```
**macOS (Homebrew)**
```
brew install qt fmt sqlite
```

Build with the following steps
```
git clone
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make
sudo make install
```
On Linux, Qt5 will be used by default. If you want to use Qt6 instead, pass `-DUSE_QT6=ON` to cmake.

### Windows
The Windows toolchain consists of Visual Studio and vcpkg in addition to Git and CMake. Before starting, make sure that Visual Studio is installed with C++ core desktop features and C++ CMake tools. The free Community Edition is sufficient.

Build with the following steps:
```
git clone https://github.com/complexlogic/kmpc.git
cd kmpc
mkdir build
cd build
git clone https://github.com/microsoft/vcpkg
cmake .. -DCMAKE_TOOLCHAIN_FILE=".\vcpkg\scripts\buildsystems\vcpkg.cmake" -DVCPKG_TARGET_TRIPLET="x64-windows"
cmake --build . --config Release
```
