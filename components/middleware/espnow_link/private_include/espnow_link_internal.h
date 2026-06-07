#ifndef ESPNOW_LINK_INTERNAL_H
#define ESPNOW_LINK_INTERNAL_H

#include "esp_now.h"
#include "espnow_link.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

namespace EspNowLink::Internal {

constexpr size_t MAX_HANDLERS = 16;
constexpr size_t MAX_PEERS = 7;
constexpr size_t RX_QUEUE_LENGTH = 16;
constexpr size_t TX_QUEUE_LENGTH = 16;
constexpr size_t MAC_QUEUE_LENGTH = 8;
constexpr size_t ACK_QUEUE_LENGTH = 8;
constexpr uint32_t TASK_STACK_SIZE = 5120;
constexpr UBaseType_t TASK_PRIORITY = 4;
constexpr uint16_t DEFAULT_ACK_TIMEOUT_MS = 20;
constexpr uint16_t MIN_ACK_TIMEOUT_MS = 8;
constexpr uint16_t MAX_ACK_TIMEOUT_MS = 100;

struct HandlerEntry {
    bool used;
    uint16_t message_id;
    MessageHandler handler;
    void* context;
};

struct PeerEntry {
    bool used;
    PeerConfig config;
    PeerMetrics metrics;
    uint32_t last_rx_session;
    uint32_t last_rx_sequence;
    bool has_rx_sequence;
};

struct RxEvent {
    MacAddress source;
    MacAddress destination;
    int8_t rssi;
    uint8_t channel;
    uint16_t size;
    uint8_t data[250];
};

struct SendRequest {
    MacAddress destination;
    uint16_t message_id;
    uint16_t payload_size;
    uint8_t payload[MAX_PAYLOAD_SIZE];
    SendOptions options;
    SendCallback callback;
    void* context;
};

struct MacResultEvent {
    MacAddress destination;
    bool success;
};

struct AckRequest {
    MacAddress destination;
    uint32_t session;
    uint32_t sequence;
};

struct PendingTransmission {
    bool active;
    bool waiting_ack;
    bool retransmitted;
    SendRequest request;
    uint32_t sequence;
    uint8_t attempts;
    TickType_t first_send_tick;
    TickType_t deadline_tick;
    size_t frame_size;
    uint8_t frame[250];
};

extern bool initialized;
extern bool active;
extern QueueHandle_t rx_queue;
extern QueueHandle_t tx_queue;
extern QueueHandle_t mac_queue;
extern QueueHandle_t ack_queue;
extern TaskHandle_t task_handle;
extern HandlerEntry handlers[MAX_HANDLERS];
extern PeerEntry peers[MAX_PEERS];
extern PendingTransmission pending;
extern SendOptions default_reliable_options;
extern LinkStatistics statistics;
extern uint32_t next_sequence;
extern uint32_t local_session_id;
extern portMUX_TYPE statistics_lock;
extern portMUX_TYPE state_lock;

PeerEntry* find_peer(const MacAddress& address);
void increment_counter(uint32_t* counter);
void process_received_event(const RxEvent& event);
void process_mac_result(const MacResultEvent& event);
void process_timeout();
void link_task(void*);
esp_err_t activate();
void deactivate();

} // namespace EspNowLink::Internal

#endif
