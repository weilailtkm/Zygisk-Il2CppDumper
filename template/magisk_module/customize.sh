#!/system/bin/sh
ui_print "- Il2CppDumper Zygisk"
ui_print "- Set package via KernelSU Manager → WebUI"

# Preserve package config across updates when Magisk keeps the old file briefly.
if [ -f "$MODPATH/target_package" ] && [ ! -s "$MODPATH/target_package" ]; then
  :
fi
if [ ! -f "$MODPATH/target_package" ]; then
  touch "$MODPATH/target_package"
fi

set_perm_recursive "$MODPATH" 0 0 0755 0644
