#!/bin/sh

#
# Copyright © 2015-2021 the original authors.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

# Standard Gradle wrapper launch script.
# If the wrapper jar is missing (e.g. fresh clone before CI bootstrap),
# fall back to a system-installed gradle.

set -e

APP_HOME=$(cd "$(dirname "$0")" && pwd)
WRAPPER_JAR="$APP_HOME/gradle/wrapper/gradle-wrapper.jar"

if [ -f "$WRAPPER_JAR" ]; then
    exec java -classpath "$WRAPPER_JAR" org.gradle.wrapper.GradleWrapperMain "$@"
elif command -v gradle >/dev/null 2>&1; then
    echo "gradle-wrapper.jar not found; using system gradle" >&2
    exec gradle "$@"
else
    echo "ERROR: neither gradle-wrapper.jar nor a system gradle is available." >&2
    echo "In CI the 'Bootstrap Gradle wrapper' step generates the jar." >&2
    exit 1
fi
