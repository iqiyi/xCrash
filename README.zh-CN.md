# xCrash

![](https://img.shields.io/badge/license-MIT-brightgreen.svg?style=flat)
![](https://img.shields.io/badge/PRs-welcome-brightgreen.svg?style=flat)
![](https://img.shields.io/badge/release-2.0.5-red.svg?style=flat)
![](https://img.shields.io/badge/Android-4.0%20--%209.0-blue.svg?style=flat)
![](https://img.shields.io/badge/arch-armeabi%20%7C%20armeabi--v7a%20%7C%20arm64--v8a%20%7C%20x86%20%7C%20x86__64-blue.svg?style=flat)

[README English Version](README.md)

xCrash 是一个安卓平台的崩溃捕获库。它支持捕获 native 崩溃和 Java 异常。

xCrash 能在 App 进程崩溃时，在你指定的目录中生成一个 tombstone 文件（格式与安卓系统的 tombstone 文件类似）。并且，不需要 root 权限或任何系统权限。

xCrash 已经在爱奇艺公司的很多安卓 APP（包括爱奇艺视频）中被使用了很多年。


## 特征

* 支持 Android 4.0 - 9.0（API level 14 - 28）。
* 支持 armeabi，armeabi-v7a，arm64-v8a，x86 和 x86_64。
* 捕获 native 崩溃和 Java 异常。
* 通过正则表达式设置需要获取哪些线程的信息。
* 获取详细的内存使用统计信息。
* 不需要 root 权限或任何系统权限。


## 捕获 native 崩溃

<p align="left"><img src="doc/catching_native_crash.png" alt="catching native crash" width="75%"></p>


## 使用

* 在 APP Project 的 `build.gradle` 中，添加 JCenter 仓库。

```Gradle
allprojects {
    repositories {
        jcenter()
    }
}
```

* 在 APP Module 的 `build.gradle` 中，添加依赖。

```Gradle
dependencies {
    implementation 'com.iqiyi.xcrash:xcrash-android-lib:2.0.5'
}
```

* 在 APP Module 的 `build.gradle` 中，指定一个或多个你想要的 ABI 支持。

```Gradle
android {
    defaultConfig {
    ndk {
        abiFilters 'armeabi', 'armeabi-v7a', 'arm64-v8a', 'x86', 'x86_64'
    }
}
```

* 在 `proguard-rules.pro` 中，添加规则。

```
-keep class xcrash.NativeCrashHandler {
    native <methods>;
    void callback(...);
}
```

* 在 `Application#attachBaseContext()` 中初始化 xCrash。

```Java
public class MyCustomApplication extends Application {

    @Override
    protected void attachBaseContext(Context base) {
        super.attachBaseContext(base);
        
        xcrash.XCrash.init(this);
    }
}
```

* 完成。

Tombstone 文件默认将被写入到 `Context#getFilesDir() + "/tombstones"` 目录。（通常在： `/data/data/<APP_PACKAGE_NAME>/files/tombstones`）

在 `src/java/xcrash_sample` 文件夹中，有一个更实际和复杂的示例 APP。

## 构建

如果你想编译 xCrash 的源码。请看这里：

* 下载 [Android NDK r16b](https://developer.android.com/ndk/downloads/revision_history.html)，设置 PATH 环境变量。（对 armeabi 的支持，从 r17 版本开始被移除了）

* 编译和安装 native 库。

```
cd ./src/native/
./build.sh
./install.sh
```

* 编译 AAR 库。

```
cd ./src/java/xcrash/
./gradlew :xcrash_lib:build
```


## 贡献

请阅读 [xCrash Contributing Guide](CONTRIBUTING.md)。


## 许可证

xCrash 使用 [MIT 许可证](LICENSE)。

xCrash 的文档使用 [Creative Commons 许可证](LICENSE-docs)。

