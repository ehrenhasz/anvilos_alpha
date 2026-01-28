set -e
GRADLE_VERSION="8.7"
GRADLE_ZIP="gradle-${GRADLE_VERSION}-bin.zip"
URL="https://services.gradle.org/distributions/${GRADLE_ZIP}"
TARGET_DIR="env/gradle"
if [ -f "${TARGET_DIR}/bin/gradle" ]; then
    echo "Gradle ${GRADLE_VERSION} already installed."
    exit 0
fi
echo "Downloading Gradle ${GRADLE_VERSION}..."
curl -L -o "${GRADLE_ZIP}" "${URL}"
echo "Unzipping..."
unzip -q "${GRADLE_ZIP}"
rm "${GRADLE_ZIP}"
mkdir -p "${TARGET_DIR}"
mv "gradle-${GRADLE_VERSION}"/* "${TARGET_DIR}/"
rmdir "gradle-${GRADLE_VERSION}"
echo "Gradle bootstrapped in ${TARGET_DIR}"
