plugins {
    alias(gradleLibs.plugins.android.application) apply false
    alias(gradleLibs.plugins.android.library) apply false
}

allprojects {
    repositories {
        mavenLocal()
        google()
        mavenCentral()
        maven { setUrl("https://s01.oss.sonatype.org/content/groups/public") }
        maven { setUrl("https://s01.oss.sonatype.org/content/repositories/releases") }
    }
}

tasks.register<Delete>("clean") {
    delete(rootProject.layout.buildDirectory)
}

rootProject.ext {
    this["abiFilters"] = "armeabi-v7a,arm64-v8a,x86,x86_64"

    val xCrash = iQiyiLibs.xcrash.get()

    this["POM_GROUP_ID"] = xCrash.group
    this["POM_ARTIFACT_ID"] = xCrash.name
    this["POM_VERSION_NAME"] = xCrash.version.orEmpty()

    this["POM_NAME"] = "xCrash Android Lib"
    this["POM_DESCRIPTION"] =
        "xCrash provides the Android app with the ability to capture java crash, native crash and ANR."
    this["POM_URL"] = "https://github.com/iqiyi/xCrash"
    this["POM_INCEPTION_YEAR"] = "2020"
    this["POM_PACKAGING"] = "aar"

    this["POM_SCM_CONNECTION"] = "https://github.com/iqiyi/xCrash.git"

    this["POM_ISSUE_SYSTEM"] = "github"
    this["POM_ISSUE_URL"] = "https://github.com/iqiyi/xCrash/issues"

    this["POM_LICENCE_NAME"] = "The MIT License"
    this["POM_LICENCE_URL"] = "https://opensource.org/licenses/MIT"
    this["POM_LICENCE_DIST"] = "repo"

    this["POM_DEVELOPER_ID"] = "iQIYI"
    this["POM_DEVELOPER_NAME"] = "iQIYI, Inc."
}
