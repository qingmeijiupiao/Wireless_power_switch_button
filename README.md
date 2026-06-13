# Wireless_power_switch_button

ESP32-C3 低功耗无线开关，通过 ESP-NOW 控制 `Wireless_power_meter_lite`。
按键路径以即时灯光反馈为优先，空闲后进入深度休眠；Shell 仅在 USB 插入时启用。

## 硬件定义

| 功能 | GPIO | 电气特性 |
|------|-----:|----------|
| 按键 | 4 | 低有效，外部 10 kΩ 上拉，可深睡唤醒 |
| USB 检测 | 5 | 插入为高，外部 2.4 MΩ 下拉，可深睡唤醒 |
| 电池 ADC | 3 | ADC1_CH3，使用 NVS Q16 分压倍率换算 |
| 电池分压使能 | 2 | 采样时开漏输出低，结束后高阻 |
| 状态灯 | 10 | 低电平点亮，深睡前切换高阻 |

GPIO7 通过飞线与 GPIO5 同网。所有输入偏置均使用外部电阻，固件不启用内部上下拉。

## 产品行为

- 短按 GPIO4：立即反馈，松开后发送 `OFF`。
- 长按 GPIO4：按住满 1 秒立即闪灯并发送 `ON`，无需等待松开。
- 电池模式完成一次按键事务后进入深度休眠。
- GPIO4 低电平和 GPIO5 高电平均可唤醒。
- 仅 GPIO5 为高时初始化 Shell；配对入口仅保留 `espnow pair`。
- 每次唤醒记录唤醒源、电池电压、输入状态、固件版本和编译时间。
- 电池模式显示电量只减不增，USB 充电模式只增不减。
- USB 满电稳定 10 分钟后自动校准板级分压倍率。

## 工程架构

```text
main/                         启动编排，只组织组件，不承载驱动细节
components/app/               产品业务与应用策略
  battery_voltage/            电池采样和分压门控
  battery_level/              曲线插值、显示策略和 RTC 冗余
  blackbox_service/           I/W/E 日志捕获与唤醒事件
  button_input/               按键判定、灯光反馈和发送任务
  espnow_remote/              遥控器业务接口
  power_manager/              唤醒判定和深睡入口
  shell_command/              Shell 命令注册
  status_led/                 低有效状态灯
components/middleware/        可复用协议与存储中间件
components/common/            无业务依赖的公共算法模板
components/bsp/               ADC、NVS、Shell 等硬件/平台封装
scripts/                      构建后固件合并
```

依赖方向保持为 `main -> app -> middleware/bsp`。低功耗 GPIO 策略由
`power_manager` 统一收口，业务组件不直接进入深睡。

## 分区表

| 分区 | 偏移 | 大小 | 说明 |
|------|------|------|------|
| `nvs` | `0x9000` | 24 KB | 配对和业务配置 |
| `app0` | `0x10000` | 1 MB | 单 factory 应用 |
| `blackbox` | `0x110000` | 3008 KB | 循环日志分区 |

合并固件不包含 `blackbox` 分区，因此不会覆盖历史日志；但其 `0x0` 至
APP 末尾之间的空白区域会写入 `0xFF`，所以会清除 `nvs` 分区。

## 组件文档

| 模块 | 文档 |
|------|------|
| 黑匣子服务 | [components/app/blackbox_service/README.md](components/app/blackbox_service/README.md) |
| Shell 命令 | [components/app/shell_command/README.md](components/app/shell_command/README.md) |
| 电池采样 | [components/app/battery_voltage/README.md](components/app/battery_voltage/README.md) |
| 电量估算 | [components/app/battery_level/README.md](components/app/battery_level/README.md) |
| 电源管理 | [components/app/power_manager/README.md](components/app/power_manager/README.md) |
| 黑匣子中间件 | [components/middleware/blackbox/README.md](components/middleware/blackbox/README.md) |
| ESP-NOW 服务 | [components/app/espnow_service/README.md](components/app/espnow_service/README.md) |
| ESP-NOW 链路 | [components/middleware/espnow_link/README.md](components/middleware/espnow_link/README.md) |

## 版本规则

版本格式为 `MAJOR.MINOR.PATCH`：

- 开发者只维护顶层 `CMakeLists.txt` 中的 `VERSION_MAJOR` 和 `VERSION_MINOR`。
- 本地构建固定使用 `PATCH=99`，Shell 会标记为非正式固件。
- 标签发布由 CI 使用 `PATCH=0` 构建，例如标签 `v0.2.0`。
- `BUILD_TIME` 固定按 UTC+8 生成，避免构建机时区导致显示不一致。

USB 模式下执行 `version` 可查询固件版本和编译时间。

执行 `battery status` 可查询校准后电压、曲线估算电量、方向约束后的显示
电量、RTC 恢复状态和满电校准进度。

## 构建与本地烧录

```powershell
idf.py set-target esp32c3
idf.py build
```

构建完成后生成：

- `build/Wireless_power_switch_button.bin`
- `Wireless_power_switch_button_merged.bin`

### APP 固件

APP 固件只写入 `app0` 分区，适合 bootloader 和分区表没有变化的日常升级：

```powershell
esptool.py --chip esp32c3 write_flash 0x10000 build/Wireless_power_switch_button.bin
```

APP 烧录不会覆盖 `0x9000-0xEFFF` 的 NVS，配对、校准和业务配置会保留。
button 没有 OTA 功能，APP 固件同样需要通过 USB/串口手动烧录。

### merged 固件

merged 固件包含 bootloader、分区表和 APP，适合首次烧录、故障恢复，以及
bootloader 或分区布局发生变化的版本：

```powershell
esptool.py --chip esp32c3 write_flash 0x0 Wireless_power_switch_button_merged.bin
```

merged 固件会清除 NVS 中的配对、校准和业务配置，但不会覆盖
`0x110000` 开始的 `blackbox` 分区。执行整片擦除仍会清除所有分区。

## 最新固件在线烧录

下面两个链接始终读取 `firmware-dist` 分支上的最新发布配置。ESP Launchpad
需要支持 Web Serial 的 Chromium 系浏览器，并通过 USB/串口连接设备。

| 入口 | 起始地址 | 说明 |
|------|----------|------|
| [APP 固件在线烧录](https://espressif.github.io/esp-launchpad/?flashConfigURL=https://cdn.jsdelivr.net/gh/qingmeijiupiao/Wireless_power_switch_button@firmware-dist/launchpad/latest.toml&app=Wireless_power_switch_button_app&exact=true) | `0x10000` | 保留 NVS；仅用于 bootloader 和分区表未变化的常规升级 |
| [merged 固件在线烧录](https://espressif.github.io/esp-launchpad/?flashConfigURL=https://cdn.jsdelivr.net/gh/qingmeijiupiao/Wireless_power_switch_button@firmware-dist/launchpad/latest.toml&app=Wireless_power_switch_button_merged&exact=true) | `0x0` | 完整安装或恢复；会清除 NVS |

固定的最新固件下载地址：

- [下载最新 APP 固件](https://cdn.jsdelivr.net/gh/qingmeijiupiao/Wireless_power_switch_button@firmware-dist/latest/app.bin)
- [下载最新 merged 固件](https://cdn.jsdelivr.net/gh/qingmeijiupiao/Wireless_power_switch_button@firmware-dist/latest/merged.bin)
- [查看最新版本元数据](https://cdn.jsdelivr.net/gh/qingmeijiupiao/Wireless_power_switch_button@firmware-dist/last.toml)

> 不确定时：已有正常运行设备优先使用 APP；首次烧录或发布说明明确要求完整
> 更新时使用 merged。不要把 APP 固件写到 `0x0`，也不要把 merged 固件写到
> `0x10000`。

## CI/CD

- CI：推送或提交 PR 到 `main` 时使用 ESP-IDF v6.0 编译 ESP32-C3 固件。
- CD：推送 `vMAJOR.MINOR` 或 `vMAJOR.MINOR.0` 标签时构建正式版本。
- Release 同时发布 APP、merged、SHA256 校验文件和 Launchpad 配置。
- `firmware-dist` 保存历史固件，并维护 README 使用的固定最新链接。
- 发布流水线会验证 APP 大小、`0x10000` 对齐、merged 的 NVS 空白区和两个
  Launchpad `offset`，校验失败时不会创建 Release。
