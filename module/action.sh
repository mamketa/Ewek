#
# Copyright (C) 2025-2026 VelocityFox22
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
if [ -n "$MMRL" ]; then
	echo "- This script isn’t meant to be run from MMRL."
	echo "- Please open the Nusantara Tweaks WebUI via the module card."
	exit 0
fi

if [ -n "$MAGISKTMP" ]; then
	pm path io.github.a13e300.ksuwebui >/dev/null 2>&1 && {
		echo "- Opening WebUI using KSU WebUI Standalone..."
		am start -n "io.github.a13e300.ksuwebui/.WebUIActivity" -e id "nusantara"
		exit 0
	}
	pm path com.dergoogler.mmrl.webuix >/dev/null 2>&1 && {
		echo "- Opening WebUI using WebUI X..."
		am start -n "com.dergoogler.mmrl.webuix/.ui.activity.webui.WebUIActivity" -e MOD_ID "nusantara"
		exit 0
	}
fi

echo "! WebUI not found."
echo "! Please install KSU WebUI to access the interface."
sleep 2
am start -a android.intent.action.VIEW -d https://github.com/5ec1cff/KsuWebUIStandalone/releases
exit 0
