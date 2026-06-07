# espnow_service

`espnow_service` 只实现 Wireless Power 产品业务协议：

- 开关控制请求与响应
- 实时数据读取请求与响应
- 周期数据上报
- 业务 payload 编解码

可靠收发、ACK、重传、去重、ESP-NOW 驱动回调、配对、peer/LMK 持久化、信道恢复和
消息 ID 分发全部由 `espnow_link` 负责。

## 结构

```text
espnow_service/
├── include/espnow_service.h
├── private_include/espnow_service_internal.h
└── src/
    ├── espnow_service_business.cpp
    └── espnow_service_business_protocol.cpp
```

`init()` 为每个业务消息 ID 直接向 `espnow_link` 注册独立回调，不创建额外业务队列或
二次分发任务。每个接收回调内部直接完成校验、解码和业务处理。回调由 `espnow_link`
消息分发任务调用，因此业务处理必须快速返回。

## 消息

| ID | 语义 |
|---|---|
| `0x0200` | 可靠开关控制请求 |
| `0x0201` | 可靠开关控制响应 |
| `0x0210` | 可靠实时数据请求 |
| `0x0211` | 可靠实时数据响应 |
| `0x0212` | 尽力周期数据上报 |

请求和响应使用非零 `request_id` 关联。协议字段通过 `espnow_codec.h` 按明确的小端格式
读写，不使用可能产生未对齐访问、严格别名违规和本机端序依赖的 `reinterpret_cast`。

## 初始化

```cpp
ESP_ERROR_CHECK(EspNowLink::init());
ESP_ERROR_CHECK(EspNowService::init());
```

按键产品在 `espnow_remote` 中注册控制响应和数据接收业务回调。

## 依赖

- `espnow_link`
