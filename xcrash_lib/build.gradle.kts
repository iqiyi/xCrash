plugins {
    alias(gradleLibs.plugins.android.library)
}

val abiFiltersStr = rootProject.ext["abiFilters"].toString()

android {
    namespace = "xcrash.lib"
    compileSdk = gradleLibs.versions.compile.sdk.get().toInt()
    buildToolsVersion = gradleLibs.versions.build.tools.get()
    ndkVersion = gradleLibs.versions.ndk.get()

    defaultConfig {
        minSdk = gradleLibs.versions.min.sdk.get().toInt()
        targetSdk = gradleLibs.versions.target.sdk.get().toInt()
        consumerProguardFiles("proguard-rules.pro")

        @Suppress("UnstableApiUsage")
        externalNativeBuild {
            cmake {
                val abiList = abiFiltersStr
                    .split(",")
                    .map(String::trim)
                    .filter(String::isNotEmpty)
                abiFilters += abiList

                val useASAN = false
                if (useASAN) {
                    // Kotlin DSL 中对 arguments 使用 += 列表更直观
                    arguments += listOf(
                        "-DANDROID_ARM_MODE=arm",
                        "-DUSEASAN=ON"
                    )
                }
            }
        }
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = gradleLibs.versions.cmake.get()
        }
    }

    compileOptions {
        val javaVersionInt = gradleLibs.versions.java.get().toInt()
        sourceCompatibility = JavaVersion.toVersion(javaVersionInt)
        targetCompatibility = JavaVersion.toVersion(javaVersionInt)
    }

    buildTypes {
        getByName("debug") {
            isMinifyEnabled = false
        }
        getByName("release") {
            isMinifyEnabled = false
        }
    }

    publishing {
        singleVariant("release") {
            withSourcesJar()
            withJavadocJar()
        }
    }
}

apply(from = rootProject.file("gradle/check.gradle.kts"))
apply(from = rootProject.file("gradle/publish.gradle.kts"))
