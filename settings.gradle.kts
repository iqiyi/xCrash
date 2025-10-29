pluginManagement {
    repositories {
        mavenLocal()
        gradlePluginPortal()
        google()
        mavenCentral()
        maven { setUrl("https://s01.oss.sonatype.org/content/groups/public") }
        maven { setUrl("https://s01.oss.sonatype.org/content/repositories/releases") }
    }
}

val dependencyDir: File = file("${rootDir.path}/.config/dependencies")

dependencyResolutionManagement {
    versionCatalogs {
        create("androidxLibs") { from(files("${dependencyDir.path}/androidx.toml")) }
        create("gradleLibs") { from(files("${dependencyDir.path}/gradle.toml")) }
        create("iQiyiLibs") { from(files("${dependencyDir.path}/iqiyi.toml")) }
    }
}

rootProject.name = "xCrash"

include(":xcrash_lib", ":xcrash_sample")
