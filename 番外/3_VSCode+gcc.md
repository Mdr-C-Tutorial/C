# VSCode安装并配置
## 1. 下载VSCode
从[VSCode网站](https://code.visualstudio.com)上下载对应版本(对于windows用户推荐system而不是user以防止不必要的麻烦 不过前提是有管理员权限)
以下是部分平台安装包直下链接
- .exe(sys)
	- [x64](https://code.visualstudio.com/sha/download?build=stable&os=win32-x64)
	- [arm64](https://code.visualstudio.com/sha/download?build=stable&os=win32-arm64)
- .exe(usr)
	- [x64](https://code.visualstudio.com/sha/download?build=stable&os=win32-x64-user)
	- [arm64](https://code.visualstudio.com/sha/download?build=stable&os=win32-arm64-user)
- .deb
	- [x64](https://code.visualstudio.com/sha/download?build=stable&os=linux-deb-x64)
	- [arm64](https://code.visualstudio.com/sha/download?build=stable&os=linux-deb-arm64)
	- [arm32](https://code.visualstudio.com/sha/download?build=stable&os=linux-deb-armhf)
- .rpm
	- [x64](https://code.visualstudio.com/sha/download?build=stable&os=linux-rpm-x64)
	- [arm64](https://code.visualstudio.com/sha/download?build=stable&os=linux-rpm-arm64)
	- [arm32](https://code.visualstudio.com/sha/download?build=stable&os=linux-rpm-armhf)
- .zip(mac)
	- [Intel chip](https://code.visualstudio.com/sha/download?build=stable&os=darwin)
	- [Apple silicon](https://code.visualstudio.com/sha/download?build=stable&os=darwin-arm64)
	- [universal](https://code.visualstudio.com/sha/download?build=stable&os=darwin-universal)

# 通过msys2安装gcc(windows)
## 1. 下载msys2
从[msys2网站](https://www.msys2.org)上下载安装包
- [githubrelease](https://github.com/msys2/msys2-installer/releases/download/nightly-x86_64/msys2-x86_64-latest.exe)
- [msysrepo](https://repo.msys2.org/distrib/msys2-x86_64-latest.exe)
- [tunamirror](https://mirrors.tuna.tsinghua.edu.cn/msys2/distrib/msys2-x86_64-latest.exe)
> msys2已停止更新x86的GUI安装包,只有自解压包
 [最后一版GUI(github)](https://github.com/msys2/msys2-installer/releases/download/nightly-i686/msys2-i686-20210705.exe)[自解压版(repo)](https://repo.msys2.org/distrib/msys2-i686-latest.sfx.exe)

