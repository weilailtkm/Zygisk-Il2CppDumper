# Zygisk-Il2CppDumper

Dump Unity IL2CPP metadata at runtime through **Zygisk** (bypasses encrypted/on-disk `global-metadata` issues).

Based on [weilailtkm/Zygisk-Il2CppDumper](https://github.com/weilailtkm/Zygisk-Il2CppDumper) / Perfare.

中文说明：[README.zh-CN.md](README.zh-CN.md)

## Design

- **Inject**: Zygisk only (Magisk Zygisk or KernelSU + ZygiskNext)
- **Config**: runtime package name (no rebuild required)
- **Output**: `/data/data/<package>/files/dump.cs`
- **Not included**: kernel injection / KPM

## Requirements

- Magisk v24+ with Zygisk, **or** KernelSU + [ZygiskNext](https://github.com/Dr-TSNG/ZygiskNext)
- Target app must use IL2CPP

## Build

```bash
./gradlew :module:assembleRelease
```

Zip is under `out/`.

GitHub Actions: run **Build** workflow; set package in WebUI after install.

## Install & configure

1. Flash the module, reboot
2. Set dump package:
   - **Shell** (Magisk / no WebUI bridge):
     ```bash
     su -c 'echo com.your.game > /data/adb/modules/zygisk_il2cppdumper/target_package'
     ```
   - **KernelSU WebUI**: Manager → Il2CppDumper → WebUI → type package → Save  
     (page must show yellow `webui-4`; otherwise old file is still loaded)
3. Cold-start the game
4. Pull dump:
   ```bash
   adb shell su -c 'cat /data/data/<package>/files/dump.cs' > dump.cs
   ```

Logs: `adb logcat | grep Il2CppDumper`

## Notes

- Waits up to ~120s for `libil2cpp.so`
- RVA uses xdl module base
- Emulators (x86) still use Native Bridge path when needed
