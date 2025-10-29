plugins {
    alias(gradleLibs.plugins.android.application)
}

val abiFiltersStr = rootProject.extra["abiFilters"].toString()

android {
    namespace = "xcrash.sample"
    compileSdk = gradleLibs.versions.compile.sdk.get().toInt()
    buildToolsVersion = gradleLibs.versions.build.tools.get()
    ndkVersion = gradleLibs.versions.ndk.get()

    defaultConfig {
        minSdk = gradleLibs.versions.min.sdk.get().toInt()
        targetSdk = gradleLibs.versions.target.sdk.get().toInt()

        applicationId = "xcrash.sample"
        versionCode = 1
        versionName = "1.0"

        ndk {
            val abiList: List<String> = abiFiltersStr
                .split(",")
                .map(String::trim)
                .filter(String::isNotEmpty)
            abiFilters += abiList
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
            proguardFiles(
                getDefaultProguardFile("proguard-android.txt"),
                "proguard-rules.pro"
            )
        }
        getByName("release") {
            isMinifyEnabled = true
            proguardFiles(
                getDefaultProguardFile("proguard-android.txt"),
                "proguard-rules.pro"
            )
        }
    }
}

dependencies {
    implementation(fileTree("libs") { include("*.jar") })
    implementation(androidxLibs.appcompat)
    implementation(androidxLibs.constraint.layout)
    // implementation(iQiyiLibs.xcrash)
    implementation(project(":xcrash_lib"))
}

apply(from = rootProject.file("gradle/sanitizer.gradle"))
