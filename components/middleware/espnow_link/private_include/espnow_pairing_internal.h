#ifndef ESPNOW_PAIRING_INTERNAL_H
#define ESPNOW_PAIRING_INTERNAL_H

#include "espnow_link.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

namespace EspNowLink::Internal {

constexpr uint16_t MSG_CHANNEL_PROBE = 0x0001;
constexpr uint16_t MSG_CHANNEL_PROBE_RESPONSE = 0x0002;
constexpr uint16_t MSG_DISCOVERY_PING = 0x0100;
constexpr uint16_t MSG_DISCOVERY_RESPONSE = 0x0101;
constexpr uint16_t MSG_PAIR_REQUEST = 0x0102;
constexpr uint16_t MSG_PAIR_RESPONSE = 0x0103;
constexpr uint16_t MSG_PAIR_CONFIRM = 0x0104;
constexpr size_t MAX_SAVED_PEERS = 3;
constexpr uint32_t STORE_MAGIC = 0x4e4f5750;
constexpr uint8_t STORE_VERSION = 1;

struct StoredPeer {
    uint8_t used;
    uint8_t reserved_role; /**< 保留旧存储布局，不再参与链路逻辑。 */
    uint8_t mac[MAC_ADDRESS_SIZE];
    uint8_t lmk[KEY_SIZE];
    uint8_t last_channel;
    uint8_t reserved[3];
};

struct PeerStore {
    uint32_t magic;
    uint8_t version;
    uint8_t count;
    uint8_t reserved[2];
    StoredPeer peers[MAX_SAVED_PEERS];
    uint32_t checksum;
};

enum class PairEventType : uint8_t {
    START_PAIRING,
    START_CHANNEL_RECOVERY,
    DISCOVERY_PING,
    DISCOVERY_RESPONSE,
    PAIR_REQUEST,
    PAIR_RESPONSE,
    PAIR_CONFIRM,
    PAIR_RESPONSE_SENT,
    CONFIRM_RESULT,
    CHANNEL_PROBE,
    CHANNEL_PROBE_RESPONSE,
};

struct PairEvent {
    PairEventType type;
    MacAddress source;
    uint32_t nonce;
    uint8_t channel;
    uint8_t lmk[KEY_SIZE];
    SendResult send_result;
};

uint32_t calculate_checksum(const PeerStore& store);
PeerStore load_store();
esp_err_t save_peer(const PeerConfig& peer, uint8_t channel);
esp_err_t update_peer_channel(const MacAddress& address, uint8_t channel);
esp_err_t erase_peer(const MacAddress& address);
esp_err_t erase_all_peers();
size_t saved_peer_count();
esp_err_t read_saved_peer(size_t index, SavedPeer* output);
void restore_peers();

bool decode_nonce(const Message& message, uint32_t* nonce);
bool decode_pair_response(const Message& message, uint32_t* nonce, uint8_t lmk[KEY_SIZE]);
size_t encode_nonce(uint32_t nonce, uint8_t* output, size_t capacity);
size_t encode_discovery_response(const MacAddress& target, uint32_t nonce,
                                 uint8_t* output, size_t capacity);
bool decode_discovery_response(const Message& message, const MacAddress& local,
                               uint32_t* nonce);
size_t encode_pair_response(uint32_t nonce, const uint8_t lmk[KEY_SIZE],
                            uint8_t* output, size_t capacity);

esp_err_t init_pairing();

} // namespace EspNowLink::Internal

#endif
