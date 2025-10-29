import org.gradle.api.publish.PublishingExtension
import org.gradle.api.publish.maven.MavenPublication
import org.gradle.plugins.signing.SigningExtension
import java.io.FileInputStream
import java.util.Properties

apply(plugin = "maven-publish")
apply(plugin = "signing")

// Default empty values for signing and OSSRH credentials
extra["signing.keyId"] = ""
extra["signing.password"] = ""
extra["signing.secretKeyRingFile"] = ""
extra["signingKey"] = ""
extra["signingPassword"] = ""
extra["ossrhUsername"] = ""
extra["ossrhPassword"] = ""

val secretPropsFile = project.rootProject.file("local.properties")
if (secretPropsFile.exists()) {
    val p = Properties()
    FileInputStream(secretPropsFile).use { p.load(it) }
    p.forEach { name, value ->
        extra[name as String] = value
    }
} else {
    extra["signing.keyId"] = System.getenv("SIGNING_KEY_ID").orEmpty()
    extra["signing.password"] = System.getenv("SIGNING_PASSWORD").orEmpty()
    extra["signing.secretKeyRingFile"] = System.getenv("SIGNING_SECRET_KEY_RING_FILE").orEmpty()
    extra["signingKey"] = System.getenv("SIGNING_KEY").orEmpty()
    extra["signingPassword"] = System.getenv("SIGNING_PASSWORD").orEmpty()
    extra["ossrhUsername"] = System.getenv("OSSRH_USERNAME").orEmpty()
    extra["ossrhPassword"] = System.getenv("OSSRH_PASSWORD").orEmpty()
}

/**
 * Helper to read extra properties from root or current project safely
 *
 * @param key Key for required value
 */
fun prop(key: String): String = when {
    rootProject.extra.has(key) -> rootProject.extra[key].toString()
    extra.has(key) -> extra[key].toString()
    else -> ""
}

project.afterEvaluate {
    // Configure publishing after the Android components are realized
    configure<PublishingExtension> {
        publications {
            create<MavenPublication>("release") {
                from(components["release"])

                artifactId = prop("POM_ARTIFACT_ID")
                groupId = prop("POM_GROUP_ID")
                version = prop("POM_VERSION_NAME")

                pom {
                    name.set(prop("POM_NAME"))
                    description.set(prop("POM_DESCRIPTION"))
                    url.set(prop("POM_URL"))
                    inceptionYear.set(prop("POM_INCEPTION_YEAR"))
                    packaging = prop("POM_PACKAGING")
                    scm {
                        connection.set(prop("POM_SCM_CONNECTION"))
                        url.set(prop("POM_URL"))
                    }
                    issueManagement {
                        system.set(prop("POM_ISSUE_SYSTEM"))
                        url.set(prop("POM_ISSUE_URL"))
                    }
                    licenses {
                        license {
                            name.set(prop("POM_LICENCE_NAME"))
                            url.set(prop("POM_LICENCE_URL"))
                            distribution.set(prop("POM_LICENCE_DIST"))
                        }
                    }
                    developers {
                        developer {
                            id.set(prop("POM_DEVELOPER_ID"))
                            name.set(prop("POM_DEVELOPER_NAME"))
                        }
                    }
                }
            }
        }
        repositories {
            maven {
                name = "sonatype"

                val releasesRepoUrl =
                    uri("https://s01.oss.sonatype.org/service/local/staging/deploy/maven2/")
                val snapshotsRepoUrl =
                    uri("https://s01.oss.sonatype.org/content/repositories/snapshots/")
                val isSnapshot = version.toString().endsWith("SNAPSHOT")
                url = if (isSnapshot) snapshotsRepoUrl else releasesRepoUrl

                credentials {
                    username =
                        (if (extra.has("ossrhUsername")) extra["ossrhUsername"].toString() else null)
                    password =
                        (if (extra.has("ossrhPassword")) extra["ossrhPassword"].toString() else null)
                }
            }
        }
    }

    configure<SigningExtension> {
        val pub = extensions.getByType(PublishingExtension::class.java)

        val taskNames = gradle.startParameter.taskNames
        val isMavenLocal = taskNames.any { it.contains("MavenLocal", ignoreCase = true) }

        // Do not wire signing at all for Maven Local
        if (isMavenLocal) {
            isRequired = false
            return@configure
        }

        val inMemKey = prop("signingKey")
        val inMemPass = prop("signingPassword")
        val hasInMem = inMemKey.isNotBlank() && inMemPass.isNotBlank() &&
                inMemKey.contains("BEGIN PGP", ignoreCase = true)

        if (hasInMem) {
            useInMemoryPgpKeys(inMemKey, inMemPass)
            isRequired = true
            sign(pub.publications)
            return@configure
        }

        // Fallback to legacy keyRing if fully provided
        val keyId = prop("signing.keyId")
        val password = prop("signing.password")
        val ringFile = prop("signing.secretKeyRingFile")
        val hasLegacy = keyId.isNotBlank() && password.isNotBlank() && ringFile.isNotBlank()
        isRequired = hasLegacy
        if (hasLegacy) {
            // Rely on Gradle Signing plugin reading legacy properties
            sign(pub.publications)
        }
    }
}
