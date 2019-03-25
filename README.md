# xCrash

![](https://img.shields.io/badge/license-MIT-brightgreen.svg?style=flat)
![](https://img.shields.io/badge/PRs-welcome-brightgreen.svg?style=flat)
![](https://img.shields.io/badge/release-2.0.5-red.svg?style=flat)
![](https://img.shields.io/badge/Android-4.0%20--%209.0-blue.svg?style=flat)
![](https://img.shields.io/badge/arch-armeabi%20%7C%20armeabi--v7a%20%7C%20arm64--v8a%20%7C%20x86%20%7C%20x86__64-blue.svg?style=flat)

[README 中文版](README.zh-CN.md)

xCrash is a crash reporting library for Android. It support catching native crash and Java exception.

xCrash can generate a tombstone file (similar format as Android system's tombstone file) in the directory you specified when the App process crashes. And, no root permission or any system permissions are required.


## Features

* Support Android 4.0 - 9.0 (API level 14 - 28).
* Support armeabi, armeabi-v7a, arm64-v8a, x86 and x86_64.
* Support catching native crash and Java exception.
* Does not require root permission or any system permissions.
* More features than the Android system's tombstone:
    * More hardware and OS information.
    * Checking if the device is rooted.
    * Build-Id list (from `.note.gnu.build-id`) for all ELFs in the crashed thread's backtrace.
    * Detailed memory usage statistics. (like `dumpsys meminfo`)
    * Setting which threads information should be dumped by a list of regular expression.
    * Callback interface used to attach more user-defined information.
    * A tombstone file parser class. (parse to `Map<String, String>`)
    * A tombstone files manager.


## Catching Native Crash

<p align="left"><img src="doc/catching_native_crash.png" alt="catching native crash" width="75%"></p>


## Usage

* Declaring JCenter repository in Project's `build.gradle`.

```Gradle
allprojects {
    repositories {
        jcenter()
    }
}
```

* Declaring a dependency in Module's `build.gradle`.

```Gradle
dependencies {
    implementation 'com.iqiyi.xcrash:xcrash-android-lib:2.0.5'
}
```

* Selecting one or more ABI you need in Module's `build.gradle`.

```Gradle
android {
    defaultConfig {
    ndk {
        abiFilters 'armeabi', 'armeabi-v7a', 'arm64-v8a', 'x86', 'x86_64'
    }
}
```

* Adding rules in `proguard-rules.pro`.

```
-keep class xcrash.NativeCrashHandler {
    native <methods>;
    void callback(...);
}
```

* Initialize xCrash in `Application#attachBaseContext()`.

```Java
public class MyCustomApplication extends Application {

    @Override
    protected void attachBaseContext(Context base) {
        super.attachBaseContext(base);
        
        xcrash.XCrash.init(this);
    }
}
```

* That's all.

Tombstone files will be written to `Context#getFilesDir() + "/tombstones"` directory by default. (usually in: `/data/data/<APP_PACKAGE_NAME>/files/tombstones`)

There is a more practical and complex sample APP in the `src/java/xcrash_sample` folder.


## Build

If you want to build xCrash from source code. Follow this guide:

* Download [Android NDK r16b](https://developer.android.com/ndk/downloads/revision_history.html), set environment PATH. (support for armeabi has been removed since r17)

* Build and install the native libraries.

```
cd ./src/native/
./build.sh
./install.sh
```

* Build AAR library.

```
cd ./src/java/xcrash/
./gradlew :xcrash_lib:build
```


## Contributing

See: [xCrash Contributing Guide](CONTRIBUTING.md).


## License

xCrash is MIT licensed, as found in the [LICENSE](LICENSE) file.

xCrash documentation is Creative Commons licensed, as found in the [LICENSE-docs](LICENSE-docs) file.
