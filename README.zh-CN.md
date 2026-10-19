# Zygisk-Il2CppDumper

用 **Zygisk** 在运行时 dump Unity IL2CPP 元数据，可绕过磁盘上 metadata 加密/混淆。

上游参考：[weilailtkm/Zygisk-Il2CppDumper](https://github.com/weilailtkm/Zygisk-Il2CppDumper)

## 设计

| 项目 | 做法 |
|------|------|
| 注入 | 仅 Zygisk（Magisk Zygisk 或 KernelSU + ZygiskNext） |
| 配置 | 运行时写包名，无需为换游戏重编 so |
| 产物 | `/data/data/<包名>/files/dump.cs` |
| 不做 | 内核注入 / KPM |

## 环境

- Magisk v24+ 并开启 Zygisk，**或** KernelSU + [ZygiskNext](https://github.com/Dr-TSNG/ZygiskNext)
- 目标为 IL2CPP 游戏

## 编译

```bash
./gradlew :module:assembleRelease
```

zip 在 `out/`。

也可用 GitHub Actions 的 **Build**；装好后在 WebUI 设包名。

## 安装与选包

1. 刷入模块并重启  
2. 设置要 dump 的包名（KernelSU）：Manager → Il2CppDumper → WebUI → 列表选包或手输 →「保存并用于 dump」  
3. **冷启动**游戏  
4. 取出 dump：
   ```bash
   adb shell su -c 'cat /data/data/<包名>/files/dump.cs' > dump.cs
   ```

日志：`adb logcat | grep Il2CppDumper`

## 说明

- 最多等待约 120 秒加载 `libil2cpp.so`
- RVA 使用 xdl 模块基址
- x86 模拟器仍走 Native Bridge（如需要）
