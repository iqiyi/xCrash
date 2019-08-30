# xCrash

![](https://img.shields.io/badge/license-MIT-brightgreen.svg?style=flat)
![](https://img.shields.io/badge/PRs-welcome-brightgreen.svg?style=flat)
![](https://img.shields.io/badge/release-2.3.5-red.svg?style=flat)
![](https://img.shields.io/badge/Android-4.0%20--%2010-blue.svg?style=flat)
![](https://img.shields.io/badge/arch-armeabi%20%7C%20armeabi--v7a%20%7C%20arm64--v8a%20%7C%20x86%20%7C%20x86__64-blue.svg?style=flat)

xCrash provides the Android app with the ability to capture java crashes, native crashes and ANR. (no root permission or any system permissions are required)

xCrash can generate a tombstone file (similar format as Android system's tombstone file) in the directory you specified when the app process crashes or ANR.

<p align="left"><img src="doc/intro.png" alt="intro" width="320px"></p>

xCrash is used in a variety of Android APPs (including iQIYI Video) from [iQIYI](http://www.iqiyi.com/) for many years.

[README 中文版](README.zh-CN.md)


## Features

* Support Android 4.0 - 10 (API level 14 - 29).
* Support armeabi, armeabi-v7a, arm64-v8a, x86 and x86_64.
* Capturing java crashes, native crashes and ANR.
* Dumping detailed memory usage statistics.
* Setting which thread's info should be dumped via regular expressions.
* Do not require root permission or any system permissions.


## Overview Maps

### Architecture

<p align="left"><img src="doc/architecture.png" alt="catching native crash" width="750px"></p>

### Catching Native Crash

<p align="left"><img src="doc/catching_native_crash.png" alt="catching native crash" width="520px"></p>


## Usage

#### 1. Add dependency.

```Gradle
dependencies {
    implementation 'com.iqiyi.xcrash:xcrash-android-lib:2.3.5'
}
```

#### 2. Specify one or more ABI(s) you need.

```Gradle
android {
    defaultConfig {
        ndk {
            abiFilters 'armeabi', 'armeabi-v7a', 'arm64-v8a', 'x86', 'x86_64'
        }
    }
}
```

#### 3. Initialize xCrash.

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

Tombstone files will be written to `Context#getFilesDir() + "/tombstones"` directory by default. (usually in: `/data/data/PACKAGE_NAME/files/tombstones`)

There is a more practical and complex sample APP in the [src/java/xcrash/xcrash_sample](src/java/xcrash/xcrash_sample) folder.


## Build

If you want to build xCrash from source code. Follow this guide:

#### 1. Download [Android NDK r16b](https://developer.android.com/ndk/downloads/revision_history.html), set PATH environment. 

#### 2. Build and copy the native libraries.

```
cd ./src/native/
./build.sh
./install.sh
```

#### 3. Build AAR library.

```
cd ./src/java/xcrash/
./gradlew :xcrash_lib:build
```


## Support

1. Check the [xcrash-sample](src/java/xcrash/xcrash_sample).
2. Communicate on [GitHub issues](https://github.com/iqiyi/xCrash/issues).
3. Email: <a href="mailto:caikelun@gmail.com">caikelun@gmail.com</a>
4. QQ group: 603635869. QR code:

<p align="left"><img src="doc/qq_group.jpg" alt="qq group" width="300px"></p>


## Contributing

See [xCrash Contributing Guide](CONTRIBUTING.md).


## License

xCrash is MIT licensed, as found in the [LICENSE](LICENSE) file.

xCrash documentation is Creative Commons licensed, as found in the [LICENSE-docs](LICENSE-docs) file.
