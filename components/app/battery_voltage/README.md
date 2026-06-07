# battery_voltage

单节锂电池电压采集应用组件。

## 硬件

- GPIO3：ADC1_CH3，读取电池二分之一分压。
- GPIO2：分压采样使能，采样期间配置为开漏输出低电平。
- 返回电压：GPIO3 校准电压乘以 2。

GPIO2 在初始化后及每次采样结束后都会恢复为无上下拉的输入高阻状态，
避免分压网络持续导通造成额外功耗。

## 采样流程

采样由后台任务执行：

1. GPIO2 开漏拉低，接通分压网络。
2. 每 1 ms 读取一次，连续 4 次变化不超过 25 mV 后认为外部 RC 已稳定。
3. 最长等待 30 ms；若波动始终未满足阈值，达到上限后仍继续平均采样。
4. 稳定后以 1 ms 间隔采样 16 次并取四舍五入平均值。
5. GPIO2 恢复高阻，再通知调用方。

启动流程会立即发起异步采样，并继续处理按键反馈和 ESP-NOW 初始化。进入
深睡前等待采样结束，确保每次唤醒的电池电压日志完整。

## API

```cpp
ESP_ERROR_CHECK(BatteryVoltage::init());

ESP_ERROR_CHECK(BatteryVoltage::start_async(callback, context));

int battery_mv = 0;
ESP_ERROR_CHECK(BatteryVoltage::wait_mv(battery_mv));
```

`read_mv()` 是兼容同步接口，内部仍使用相同的异步任务和平均算法。
