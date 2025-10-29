import org.gradle.api.plugins.quality.Checkstyle
import org.gradle.api.plugins.quality.CheckstyleExtension

apply(plugin = "checkstyle")

// 不能直接写 checkstyle { ... }，要用扩展配置方式
configure<CheckstyleExtension> {
    configFile = rootProject.file("checkstyle.xml")
    toolVersion = "8.18"
    isIgnoreFailures = false
    isShowViolations = true
}

tasks.register<Checkstyle>("checkstyle") {
    source("src/main/java")
    include("**/*.java")
    classpath = files()
}

tasks.named("check") {
    dependsOn("checkstyle")
}
