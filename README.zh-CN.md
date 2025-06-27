<p align="center"><img src="doc/xcrash_logo.png" alt="xCrash Logo" width="450px"></p>

# xCrash

![](https://img.shields.io/badge/license-MIT-brightgreen.svg?style=flat)
![](https://img.shields.io/badge/PRs-welcome-brightgreen.svg?style=flat)
![](https://img.shields.io/badge/release-3.0.0-red.svg?style=flat)
![](https://img.shields.io/badge/Android-4.1%20--%2011-blue.svg?style=flat)
![](https://img.shields.io/badge/armeabi--v7a%20%7C%20arm64--v8a%20%7C%20x86%20%7C%20x86__64-blue.svg?style=flat)

xCrash 能为安卓 app 提供捕获 java 崩溃，native 崩溃和 ANR 的能力。不需要 root 权限或任何系统权限。

<p align="left"><img src="doc/intro.png" alt="intro" width="320px"></p>

xCrash 能在 app 进程崩溃或 ANR 时，在你指定的目录中生成一个 tombstone 文件（格式与安卓系统的 tombstone 文件类似）。

xCrash 已经在 [爱奇艺](http://www.iqiyi.com/) 的不同平台（手机，平板，电视）的很多安卓 app（包括爱奇艺视频）中被使用了很多年。

[README English Version](README.md)


## 特征

* 支持 Android 4.1 - 11（API level 16 - 30）。
* 支持 armeabi-v7a，arm64-v8a，x86 和 x86_64。
* 捕获 java 崩溃，native 崩溃和 ANR。
* 获取详细的进程、线程、内存、FD、网络统计信息。
* 通过正则表达式设置需要获取哪些线程的信息。
* 不需要 root 权限或任何系统权限。


## Tombstone 文件预览

* [java 崩溃](doc/tombstone_java.txt)
* [native 崩溃 (armeabi-v7a)](doc/tombstone_native_armeabi-v7a.txt)
* [native 崩溃 (arm64-v8a)](doc/tombstone_native_arm64-v8a.txt)
* [native 崩溃 (x86)](doc/tombstone_native_x86.txt)
* [native 崩溃 (x86_64)](doc/tombstone_native_x86_64.txt)
* [ANR (armeabi-v7a)](doc/tombstone_anr_armeabi-v7a.txt)
* [ANR (arm64-v8a)](doc/tombstone_anr_arm64-v8a.txt)
* [ANR (x86)](doc/tombstone_anr_x86.txt)
* [ANR (x86_64)](doc/tombstone_anr_x86_64.txt)


## 概览图

### 架构

<p align="left"><img src="doc/architecture.png" alt="architecture" width="750px"></p>

### 捕获 native 崩溃

<p align="left"><img src="doc/capture_native_crash.png" alt="capture native crash" width="520px"></p>

### 捕获 ANR

<p align="left"><img src="doc/capture_anr.png" alt="capture anr" width="750px"></p>

## 使用

#### 1. 增加依赖。

```Gradle
dependencies {
    implementation 'com.iqiyi.xcrash:xcrash-android-lib:3.0.0'
}
```

#### 2. 指定一个或多个你需要的 ABI。

```Gradle
android {
    defaultConfig {
        ndk {
            abiFilters 'armeabi-v7a', 'arm64-v8a', 'x86', 'x86_64'
        }
    }
}
```

#### 3. 初始化 xCrash。

> Java

```Java
public class MyCustomApplication extends Application {

    @Override
    protected void attachBaseContext(Context base) {
        super.attachBaseContext(base);
        
        xcrash.XCrash.init(this);
    }
}
```

> Kotlin

```Kotlin
class MyCustomApplication : Application() {

    override fun attachBaseContext(base: Context) {
        super.attachBaseContext(base)

        xcrash.XCrash.init(this)
    }
}
```

Tombstone 文件默认将被写入到 `Context#getFilesDir() + "/tombstones"` 目录。（通常在： `/data/data/PACKAGE_NAME/files/tombstones`）

在 [xcrash_sample](xcrash_sample) 文件夹中，有一个更实际和复杂的示例 app。


## 构建

#### 编译 xCrash AAR 库:

```
./gradlew :xcrash_lib:build
```


## 技术支持

* [GitHub Issues](https://github.com/iqiyi/xCrash/issues)
* [GitHub Discussions](https://github.com/iqiyi/xCrash/discussions)
* Email: <a href="mailto:caikelun@gmail.com">caikelun@gmail.com</a>, <a href="mailto:xuqnqn@qq.com">xuqnqn@qq.com</a>


## 贡献

请阅读 [xCrash Contributing Guide](CONTRIBUTING.md)。


## 许可证

xCrash 使用 [MIT 许可证](LICENSE)。

xCrash 的文档使用 [Creative Commons 许可证](LICENSE-docs)。

