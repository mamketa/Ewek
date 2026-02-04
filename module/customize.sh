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

# shellcheck disable=SC1091,SC2034,SC2317
SKIPUNZIP=1
SOC=0

MODULE_CONFIG="/data/adb/.config/Nusantara"

make_node() {
	[ ! -f "$2" ] && echo "$1" >"$2"
}

make_dir() {
	[ ! -d "$1" ] && mkdir -p "$1"
}

abort_unsupported_arch() {
	ui_print "*********************************************************"
	ui_print "! Unsupported Architecture Detected"
	ui_print "! This device architecture is not supported by Nusantara Tweaks."
	ui_print "! Supported architecture: arm64-v8a only."
	abort "*********************************************************"
}

abort_roasted_32bit() {
	ui_print "*********************************************************"
	ui_print "! 32-bit Device Detected (armeabi-v7a)"
	ui_print "! Seriously? In 2026?"
	ui_print "! Nusantara Tweaks refuses to run on ancient hardware."
	ui_print "! Please upgrade your device and come back stronger."
	ui_print "! The future is 64-bit. This device is not."
	abort "*********************************************************"
}

abort_corrupted() {
	ui_print "*********************************************************"
	ui_print "! Failed to extract verify.sh"
	ui_print "! Installation aborted due to corrupted package."
	ui_print "! Please re-download the module and try again."
	abort "*********************************************************"
}

soc_recognition_extra() {
	[ -d /sys/class/kgsl/kgsl-3d0/devfreq ] && {
		SOC=2
		ui_print "- Snapdragon platform detected"
		return 0
	}

	[ -d /sys/devices/platform/kgsl-2d0.0/kgsl ] && {
		SOC=2
		ui_print "- Snapdragon platform detected"
		return 0
	}

	[ -d /sys/kernel/ged/hal ] && {
		SOC=1
		ui_print "- MediaTek platform detected"
		return 0
	}

	return 1
}

get_soc_getprop() {
	local SOC_PROP="
ro.board.platform
ro.soc.model
ro.hardware
ro.chipname
ro.hardware.chipname
ro.vendor.soc.model.external_name
ro.vendor.qti.soc_name
ro.vendor.soc.model.part_name
ro.vendor.soc.model
"
	for prop in $SOC_PROP; do
		getprop "$prop"
	done
}

recognize_soc() {
	case "$1" in
	*mt* | *MT*) SOC=1 ;;
	*sm* | *qcom* | *SM* | *QCOM* | *Qualcomm*) SOC=2 ;;
	*exynos* | *Exynos* | *EXYNOS* | *universal* | *samsung* | *erd* | *s5e*) SOC=3 ;;
	*Unisoc* | *unisoc* | *ums*) SOC=4 ;;
	*gs* | *Tensor* | *tensor*) SOC=5 ;;
	esac

	case "$SOC" in
	1) ui_print "- Applying MediaTek-specific tweaks" ;;
	2) ui_print "- Applying Snapdragon-specific tweaks" ;;
	3) ui_print "- Applying Exynos-specific tweaks" ;;
	4) ui_print "- Applying Unisoc-specific tweaks" ;;
	5) ui_print "- Applying Google Tensor-specific tweaks" ;;
	0) return 1 ;;
	esac
}

# Integrity Check
ui_print "- Extracting verification script"
unzip -o "$ZIPFILE" 'verify.sh' -d "$TMPDIR" >&2
[ ! -f "$TMPDIR/verify.sh" ] && abort_corrupted
source "$TMPDIR/verify.sh"

# Architecture Validation
ui_print "- Checking device architecture"

case "$ARCH" in
arm64)
	ARCH_TMP="arm64-v8a"
	ui_print "- Architecture supported: arm64-v8a"
	;;
arm)
	abort_roasted_32bit
	;;
*)
	abort_unsupported_arch
	;;
esac

# Extract Module Core
ui_print "- Extracting module components"
extract "$ZIPFILE" 'module.prop' "$MODPATH"
extract "$ZIPFILE" 'service.sh' "$MODPATH"
extract "$ZIPFILE" 'uninstall.sh' "$MODPATH"
extract "$ZIPFILE" 'action.sh' "$MODPATH"
extract "$ZIPFILE" 'system/bin/nusantara_profiler' "$MODPATH"
extract "$ZIPFILE" 'system/bin/nusantara_utility' "$MODPATH"
extract "$ZIPFILE" 'system/bin/sys.npreloader' "$MODPATH"

# Extract Architecture Binaries
extract "$ZIPFILE" "libs/$ARCH_TMP/sys.nusaservice" "$TMPDIR"
cp "$TMPDIR/libs/$ARCH_TMP/"* "$MODPATH/system/bin"
ln -sf "$MODPATH/system/bin/sys.nusaservice" "$MODPATH/system/bin/nusantara_log"
rm -rf "$TMPDIR/libs"

# KernelSU / APatch Handling
if [ "$KSU" = "true" ] || [ "$APATCH" = "true" ]; then
	rm "$MODPATH/action.sh"
	touch "$MODPATH/skip_mount"
	ui_print "- KernelSU / APatch detected, using dynamic binary linking"

	manager_paths="/data/adb/ap/bin /data/adb/ksu/bin"
	BIN_PATH="/data/adb/modules/nusantara/system/bin"

	for dir in $manager_paths; do
		[ -d "$dir" ] && {
			ui_print "- Linking binaries into $dir"
			ln -sf "$BIN_PATH/sys.nusaservice" "$dir/sys.nusaservice"
			ln -sf "$BIN_PATH/sys.nusaservice" "$dir/nusantara_log"
			ln -sf "$BIN_PATH/nusantara_profiler" "$dir/nusantara_profiler"
			ln -sf "$BIN_PATH/nusantara_utility" "$dir/nusantara_utility"
			ln -sf "$BIN_PATH/sys.npreloader" "$dir/sys.npreloader"
		}
	done
fi

# Web Interface
ui_print "- Deploying web interface"
unzip -o "$ZIPFILE" "webroot/*" -d "$MODPATH" >&2

# Velocity Toast
ui_print "- Installing Velocity Toast"
extract "$ZIPFILE" velocitytoast.apk "$MODPATH"
pm install "$MODPATH/velocitytoast.apk" >/dev/null 2>&1
rm "$MODPATH/velocitytoast.apk"

# Root Trace Cleanup
[ -d /data/nusantara ] && rm -rf /data/nusantara
[ -f /data/local/tmp/nusantara_logo.png ] && rm -f /data/local/tmp/nusantara_logo.png

# Configuration Setup
ui_print "- Initializing Nusantara configuration"
make_dir "$MODULE_CONFIG"
make_node 0 "$MODULE_CONFIG/lite_mode"
make_node 0 "$MODULE_CONFIG/dnd_gameplay"
make_node 0 "$MODULE_CONFIG/device_mitigation"
[ ! -f "$MODULE_CONFIG/ppm_policies_mediatek" ] && echo 'PWR_THRO|THERMAL' >"$MODULE_CONFIG/ppm_policies_mediatek"
[ ! -f "$MODULE_CONFIG/gamelist.txt" ] && extract "$ZIPFILE" 'gamelist.txt' "$MODULE_CONFIG"

# Permissions
ui_print "- Applying permissions"
set_perm_recursive "$MODPATH/system/bin" 0 0 0755 0755

# SoC Recognition
soc_recognition_extra
[ $SOC -eq 0 ] && recognize_soc "$(get_soc_getprop)"
[ $SOC -eq 0 ] && recognize_soc "$(grep -E "Hardware|Processor" /proc/cpuinfo | uniq | cut -d ':' -f 2 | sed 's/^[ \t]*//')"
[ $SOC -eq 0 ] && recognize_soc "$(grep "model\sname" /proc/cpuinfo | uniq | cut -d ':' -f 2 | sed 's/^[ \t]*//')"

[ $SOC -eq 0 ] && {
	ui_print "! Unable to determine SoC"
	ui_print "! Some platform-specific optimizations will be skipped"
}

echo $SOC >"$MODULE_CONFIG/soc_recognition"

# Nusantara Easter Egg
case "$((RANDOM % 9 + 1))" in
1) ui_print "- Have you ever explored Nusantara beyond the map?" ;;
2) ui_print "- Do you know how many cultures live inside Nusantara?" ;;
3) ui_print "- Nusantara is not just land, it's legacy." ;;
4) ui_print "- From Sabang to Merauke, performance matters." ;;
5) ui_print "- Ready to unlock the true spirit of Nusantara?" ;;
6) ui_print "- Diversity is strength, optimization is power." ;;
7) ui_print "- Nusantara asks: are you ready to go further?" ;;
8) ui_print "- Strong device, strong roots, strong Nusantara." ;;
9) ui_print "- Wen Donate?" ;;
esac