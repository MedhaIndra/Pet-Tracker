#include <RadioLib.h>

// SX1278 pin mapping for XIAO MG24
// NSS/CS = D2
// DIO0   = D3
// RESET  = D4
// SPI    = default SPI (SCK=D8, MISO=D9, MOSI=D10)
SX1278 radio = new Module(D2, D3, D4, RADIOLIB_NC);  

//Server Code
#define RF_SW_PW_PIN PB5
#define RF_SW_PIN PB4

bool notification_enabled = false;

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LED_BUILTIN_INACTIVE);
  Serial.begin(115200);

  // turn on the antenna function
  pinMode(RF_SW_PW_PIN, OUTPUT);
  digitalWrite(RF_SW_PW_PIN, HIGH);

  delay(100);

  // HIGH -> Use external antenna / LOW -> Use built-in antenna
  pinMode(RF_SW_PIN, OUTPUT);
  digitalWrite(RF_SW_PIN, LOW);

  Serial.println("SX1278 LoRa - XIAO MG24 Rx");

  // Begin radio at your frequency (e.g., 433.0 MHz)
  int state = radio.begin(433.0);
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println("Radio initialized successfully!");
  } else {
    Serial.print("Radio init failed, code: ");
    Serial.println(state);
    while (true);
  }

  state = radio.startReceive();
  if (state != RADIOLIB_ERR_NONE) {
    Serial.print("startReceive() failed, code: ");
    Serial.println(state);
  }
}

void loop() {
  // ===== LoRa RECEIVE MODE =====
  String str;
  int state = radio.receive(str);

  if (state == RADIOLIB_ERR_NONE) {
    Serial.print("Received: ");
    Serial.println(str);

    Serial.print("RSSI: ");
    Serial.print(radio.getRSSI());
    Serial.println(" dBm");

    // ===== PARSE LAT/LON =====
    float latitude = 0.0, longitude = 0.0;
    int commaIndex = str.indexOf(',');

    if (commaIndex > 0) {
      String latStr = str.substring(0, commaIndex);
      String lonStr = str.substring(commaIndex + 1);
      latitude = latStr.toFloat();
      longitude = lonStr.toFloat();

      Serial.print("Parsed Latitude: ");
      Serial.println(latitude, 6);
      Serial.print("Parsed Longitude: ");
      Serial.println(longitude, 6);

      // ===== SEND VIA BLUETOOTH =====
      if (notification_enabled) {
        send_gps_notification(latitude, longitude);
        Serial.println("Bluetooth notification sent!");
      } else {
        Serial.println("Notification disabled.");
      }
    } else {
      Serial.println("Invalid LoRa packet format (missing comma).");
    }
  } 
  else if (state != RADIOLIB_ERR_NONE && state != RADIOLIB_ERR_RX_TIMEOUT) {
    Serial.print("Receive failed, code ");
    Serial.println(state);
  }

  // Continue listening
  radio.startReceive();
  delay(5);
}

// ===== BLE HANDLERS =====
static void ble_initialize_gatt_db();
static void ble_start_advertising();

static const uint8_t advertised_name[] = "XIAO_MG24 Server";
static uint16_t gattdb_session_id;
static uint16_t generic_access_service_handle;
static uint16_t name_characteristic_handle;
static uint16_t my_service_handle;
static uint16_t notify_characteristic_handle;

/**************************************************************************/ 
void sl_bt_on_event(sl_bt_msg_t *evt) {
  switch (SL_BT_MSG_ID(evt->header)) {

    case sl_bt_evt_system_boot_id:
      Serial.println("BLE stack booted");
      ble_initialize_gatt_db();
      ble_start_advertising();
      Serial.println("BLE advertisement started");
      break;

    case sl_bt_evt_connection_opened_id:
      Serial.println("BLE connection opened");
      break;

    case sl_bt_evt_connection_closed_id:
      Serial.println("BLE connection closed");
      ble_start_advertising();
      Serial.println("BLE advertisement restarted");
      break;

    case sl_bt_evt_gatt_server_characteristic_status_id:
      if (evt->data.evt_gatt_server_characteristic_status.characteristic == notify_characteristic_handle) {
        if (evt->data.evt_gatt_server_characteristic_status.client_config_flags & sl_bt_gatt_notification) {
          Serial.println("Notifications enabled");
          notification_enabled = true;
        } else {
          Serial.println("Notifications disabled");
          notification_enabled = false;
        }
      }
      break;

    default:
      break;
  }
}

/**************************************************************************/ 
// Sends a BLE notification with GPS data
static void send_gps_notification(float latitude, float longitude) {
  char gps_str[32];
  snprintf(gps_str, sizeof(gps_str), "%.6f,%.6f", latitude, longitude);

  sl_status_t sc = sl_bt_gatt_server_notify_all(
      notify_characteristic_handle,
      strlen(gps_str),
      (const uint8_t*)gps_str
  );

  if (sc == SL_STATUS_OK) {
      Serial.print("Sent GPS notification: ");
      Serial.println(gps_str);
  } else {
      Serial.print("Failed to send GPS notification, status: ");
      Serial.println(sc, HEX);
  }
}

/**************************************************************************/ 
static void ble_start_advertising() {
  static uint8_t advertising_set_handle = 0xff;
  static bool init = true;
  sl_status_t sc;

  if (init) {
    sc = sl_bt_advertiser_create_set(&advertising_set_handle);
    app_assert_status(sc);

    sc = sl_bt_advertiser_set_timing(
      advertising_set_handle, 160, 160, 0, 0);
    app_assert_status(sc);

    init = false;
  }

  sc = sl_bt_legacy_advertiser_generate_data(advertising_set_handle, sl_bt_advertiser_general_discoverable);
  app_assert_status(sc);

  sc = sl_bt_legacy_advertiser_start(advertising_set_handle, sl_bt_advertiser_connectable_scannable);
  app_assert_status(sc);
}

/**************************************************************************/ 
static void ble_initialize_gatt_db() {
  sl_status_t sc;
  sc = sl_bt_gattdb_new_session(&gattdb_session_id);
  app_assert_status(sc);

  // Generic Access Service
  const uint8_t generic_access_service_uuid[] = { 0x00, 0x18 };
  sc = sl_bt_gattdb_add_service(gattdb_session_id,
                                sl_bt_gattdb_primary_service,
                                SL_BT_GATTDB_ADVERTISED_SERVICE,
                                sizeof(generic_access_service_uuid),
                                generic_access_service_uuid,
                                &generic_access_service_handle);
  app_assert_status(sc);

  // Device Name Characteristic
  const sl_bt_uuid_16_t device_name_characteristic_uuid = { .data = { 0x00, 0x2A } };
  sc = sl_bt_gattdb_add_uuid16_characteristic(gattdb_session_id,
                                              generic_access_service_handle,
                                              SL_BT_GATTDB_CHARACTERISTIC_READ,
                                              0x00,
                                              0x00,
                                              device_name_characteristic_uuid,
                                              sl_bt_gattdb_fixed_length_value,
                                              sizeof(advertised_name) - 1,
                                              sizeof(advertised_name) - 1,
                                              advertised_name,
                                              &name_characteristic_handle);
  app_assert_status(sc);
  sc = sl_bt_gattdb_start_service(gattdb_session_id, generic_access_service_handle);
  app_assert_status(sc);

  // Custom BLE Service
  const uuid_128 my_service_uuid = {
    .data = { 0x24, 0x12, 0xb5, 0xcb, 0xd4, 0x60, 0x80, 0x0c,
              0x15, 0xc3, 0x9b, 0xa9, 0xac, 0x5a, 0x8a, 0xde }
  };
  sc = sl_bt_gattdb_add_service(gattdb_session_id,
                                sl_bt_gattdb_primary_service,
                                SL_BT_GATTDB_ADVERTISED_SERVICE,
                                sizeof(my_service_uuid),
                                my_service_uuid.data,
                                &my_service_handle);
  app_assert_status(sc);

  // Notify Characteristic (for GPS data)
  const uuid_128 notify_characteristic_uuid = {
    .data = { 0x9c, 0xd2, 0x70, 0x2a, 0x65, 0x6d, 0x53, 0x9a,
              0xd0, 0x60, 0xc3, 0x41, 0xa4, 0x85, 0xa8, 0x61 }
  };
  uint8_t notify_char_init_value = 0;
  sc = sl_bt_gattdb_add_uuid128_characteristic(gattdb_session_id,
                                               my_service_handle,
                                               SL_BT_GATTDB_CHARACTERISTIC_READ | SL_BT_GATTDB_CHARACTERISTIC_NOTIFY,
                                               0x00,
                                               0x00,
                                               notify_characteristic_uuid,
                                               sl_bt_gattdb_fixed_length_value,
                                               1,
                                               sizeof(notify_char_init_value),
                                               &notify_char_init_value,
                                               &notify_characteristic_handle);
  app_assert_status(sc);

  sc = sl_bt_gattdb_start_service(gattdb_session_id, my_service_handle);
  app_assert_status(sc);

  sc = sl_bt_gattdb_commit(gattdb_session_id);
  app_assert_status(sc);
}
