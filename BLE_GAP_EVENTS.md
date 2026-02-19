# BLE GAP Event Types Reference

This document lists all BLE GAP (Generic Access Profile) event types defined in NimBLE.

## Event Definitions

| Event Code | Event Name | Description |
|------------|------------|-------------|
| 0 | `BLE_GAP_EVENT_CONNECT` | Connection attempt completed |
| 1 | `BLE_GAP_EVENT_DISCONNECT` | Connection terminated |
| 2 | *Reserved* | |
| 3 | `BLE_GAP_EVENT_CONN_UPDATE` | Connection parameters updated |
| 4 | `BLE_GAP_EVENT_CONN_UPDATE_REQ` | Connection parameter update request from peer |
| 5 | `BLE_GAP_EVENT_L2CAP_UPDATE_REQ` | L2CAP connection parameter update request |
| 6 | `BLE_GAP_EVENT_TERM_FAILURE` | Connection termination attempt failed |
| 7 | `BLE_GAP_EVENT_DISC` | Discovery report received |
| 8 | `BLE_GAP_EVENT_DISC_COMPLETE` | Discovery procedure completed |
| 9 | `BLE_GAP_EVENT_ADV_COMPLETE` | Advertising procedure completed |
| 10 | `BLE_GAP_EVENT_ENC_CHANGE` | Encryption state changed |
| 11 | `BLE_GAP_EVENT_PASSKEY_ACTION` | Passkey query for pairing |
| 12 | `BLE_GAP_EVENT_NOTIFY_RX` | Notification or indication received |
| 13 | `BLE_GAP_EVENT_NOTIFY_TX` | Notification or indication transmitted |
| 14 | `BLE_GAP_EVENT_SUBSCRIBE` | Peer subscription status changed (CCCD) |
| 15 | `BLE_GAP_EVENT_MTU` | MTU updated |
| 16 | `BLE_GAP_EVENT_IDENTITY_RESOLVED` | Peer identity address resolved |
| 17 | `BLE_GAP_EVENT_REPEAT_PAIRING` | Repeat pairing attempt detected |
| 18 | `BLE_GAP_EVENT_PHY_UPDATE_COMPLETE` | PHY update completed |
| 19 | `BLE_GAP_EVENT_EXT_DISC` | Extended advertising discovery report |
| 20 | `BLE_GAP_EVENT_PERIODIC_SYNC` | Periodic advertising sync established |
| 21 | `BLE_GAP_EVENT_PERIODIC_REPORT` | Periodic advertising report received |
| 22 | `BLE_GAP_EVENT_PERIODIC_SYNC_LOST` | Periodic advertising sync lost |
| 23 | `BLE_GAP_EVENT_SCAN_REQ_RCVD` | Scan request received (extended advertising) |
| 24 | `BLE_GAP_EVENT_PERIODIC_TRANSFER` | Periodic advertising sync transfer received |
| 25 | `BLE_GAP_EVENT_PATHLOSS_THRESHOLD` | Path loss threshold crossed |
| 26 | `BLE_GAP_EVENT_TRANSMIT_POWER` | Transmit power changed |
| 27 | `BLE_GAP_EVENT_PARING_COMPLETE` | Pairing complete |
| 28 | `BLE_GAP_EVENT_SUBRATE_CHANGE` | Connection subrate changed |
| 29 | `BLE_GAP_EVENT_VS_HCI` | Vendor-specific HCI event |
| 30 | `BLE_GAP_EVENT_BIGINFO_REPORT` | BIG Info advertising report |
| 31 | `BLE_GAP_EVENT_REATTEMPT_COUNT` | Connection reattempt count |
| 32 | `BLE_GAP_EVENT_AUTHORIZE` | GATT authorization request |
| 33 | `BLE_GAP_EVENT_TEST_UPDATE` | DTM test update |
| 34 | `BLE_GAP_EVENT_DATA_LEN_CHG` | Data length changed |
| 35 | `BLE_GAP_EVENT_CONNLESS_IQ_REPORT` | Connectionless IQ report (AoA/AoD) |
| 36 | `BLE_GAP_EVENT_CONN_IQ_REPORT` | Connection IQ report (AoA/AoD) |
| 37 | `BLE_GAP_EVENT_CTE_REQ_FAILED` | CTE request failed |
| 38 | `BLE_GAP_EVENT_LINK_ESTAB` | Link establishment completed |

## Source

Defined in: `ble_gap.h` (NimBLE stack)

**Path:** `esp-idf-v5.4/components/bt/host/nimble/nimble/nimble/host/include/host/ble_gap.h`

## Usage in This Project

Events are handled in the GAP event callback function `ble_prph_gap_event()` in [ble-main.c](nimble/ble-main.c).

### Events Currently Handled:
- `BLE_GAP_EVENT_CONNECT` (0) - Connection establishment
- `BLE_GAP_EVENT_DISCONNECT` (1) - Connection termination  
- `BLE_GAP_EVENT_CONN_UPDATE` (3) - Connection parameter updates
- `BLE_GAP_EVENT_ADV_COMPLETE` (9) - Advertising completion
- `BLE_GAP_EVENT_MTU` (15) - MTU negotiation
- `BLE_GAP_EVENT_SUBSCRIBE` (14) - CCCD subscription changes
- `BLE_GAP_EVENT_LINK_ESTAB` (38) - Link establishment with parameter validation
- `BLE_GAP_EVENT_DATA_LEN_CHG` (34) - Data length extension
