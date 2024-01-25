#include <BLEDevice.h>
#include <BLEClient.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <PubSubClient.h>
#include <WiFi.h>

enum BtConnState
{
  IDLE,
  CONNECTING,
  CONNECTED,
  ERROR,
  READY
};

typedef struct
{
  int ctr_id;
  String mac;
  BLEClient *client;
  BLEAdvertisedDevice *device;
  BLERemoteCharacteristic *pRemoteCharacteristic;
  BtConnState state;
  
} BtConn;

// Structure representing a key-value pair
struct KeyValuePair {
   String key;
    BtConn *value;
};

// Class representing a dictionary
class Dictionary {
public:
    static const int MAX_ENTRIES = 10; // Maximum number of entries in the dictionary
    KeyValuePair entries[MAX_ENTRIES];
    int numEntries;


    Dictionary() : numEntries(0) {}

    // Insert a key-value pair into the dictionary
    void insert(String key, BtConn * value) {
        if (numEntries < MAX_ENTRIES) {
            entries[numEntries].key = key;
            entries[numEntries].value = value;
            numEntries++;
        } else {
            Serial.println("Dictionary is full, cannot insert more entries");
        }
    }

    // Retrieve the value associated with a key from the dictionary
    BtConn * get(String key) const {
        for (int i = 0; i < numEntries; ++i) {
            if (entries[i].key.equals(key)) {
                return entries[i].value;
            }
        }

        return NULL; // Return a default value (you may want to choose a different strategy)
    }

    // Remove an entry with the given key from the dictionary
    void remove(const String key) {
        for (int i = 0; i < numEntries; ++i) {
            if (entries[i].key.equals(key)) {
                // Shift elements to fill the gap
                for (int j = i; j < numEntries - 1; ++j) {
                    entries[j] = entries[j + 1];
                }
                numEntries--;
                return; // Key found and removed
            }
        }
        // You may want to handle the case where the key is not found
        Serial.println("Key not found, cannot remove");
    }
};

static Dictionary thingys;

// BLE Client
BLEClient* pClient = NULL;

// MQTT Settings
const char* mqtt_server = "192.168.0.52"; // Replace with your MQTT broker IP address
const int mqtt_port = 1884;

// MQTT Client
WiFiClient espClient;
PubSubClient client(espClient);

// Function to convert raw data to float value
float convertRawDataToFloat(const uint8_t* pData) {
  float value = (float)(((uint32_t)pData[3] << 16) | ((uint32_t)pData[2] << 8) | ((uint32_t)pData[1])) * std::pow(10, (int8_t)pData[4]);
  return value;
}

// static void notifyCallback(
//   BLERemoteCharacteristic* pBLERemoteCharacteristic,
//   uint8_t* pData,
//   size_t length,
//   bool isNotify) {
//     Serial.print("Notify callback for characteristic ");
//     Serial.print(pBLERemoteCharacteristic->getUUID().toString().c_str());
//     Serial.print(" of data length ");
//     Serial.println(length);
//     Serial.print("data: ");
//     float convertedValue = (float)(((uint32_t)pData[3] << 16) | ((uint32_t)pData[2] << 8) | ((uint32_t)pData[1])) * std::pow(10, (int8_t)pData[4]);
//     char message[50];
//     String mac = pBLERemoteCharacteristic->remoteAddress().toString();
//     sprintf(message, "Temperature: %.2f; MAC: %s", convertedValue, mac.c_str());

//     Serial.print("Preparing message: ");
//     Serial.println(message);

//     // Send converted data to MQTT broker
//     if (client.connect("ESP32_Client")) {
//       client.publish(mqtt_topic, message);
//       Serial.println("******ESP32 has connected to MQTT broker******");
//     } else {
//       Serial.println("Failed to connect to MQTT broker");
//     }
// }

bool connectClient(BtConn *conn) {
  if (!conn->client->isConnected()) {
    bool connected = conn->client->connect(conn->device->getAddress());
    if (!connected) {
      Serial.println("Failed to connect to Thingy52.");
      return false; 
    }
  }
  Serial.println("******Connected to Thingy52!******");

  // Publish temperature
  BLERemoteCharacteristic* pCharacteristic = conn->client->getService("00001809-0000-1000-8000-00805f9b34fb")->getCharacteristic("00002a1c-0000-1000-8000-00805f9b34fb");
  if (pCharacteristic) {
    conn->pRemoteCharacteristic = pCharacteristic;
    std::string value = pCharacteristic->readValue();
    if (value.length() > 0) {
      // Serial.print("Received data: ");
      // for (int i = 0; i < value.length(); i++) {
      //   Serial.print(String(value[i], HEX));
      //   Serial.print(" ");
      // }
      // Serial.println();

      // Convert raw data to float
      float convertedValue = convertRawDataToFloat(reinterpret_cast<const uint8_t*>(value.c_str()));

      char message[50];
      sprintf(message, "Temperature: %.2f; MAC: %s", convertedValue, conn->mac.c_str());

      Serial.print("Preparing message: ");
      Serial.println(message);

      char topic[50];
      // sprintf(topic, "%s/temperature", conn->device->getName());
      sprintf(topic, "%d/temperature", conn->ctr_id);

      // Send converted data to MQTT broker
      if (client.connect("ESP32_Client")) {
        Serial.print("Publising to topic");
        Serial.println(topic);
        // client.publish(topic, String(convertedValue, 2).c_str());
        client.publish(topic, message);
        Serial.println("******ESP32 has connected to MQTT broker******");
      } else {
        Serial.println("Failed to connect to MQTT broker");
      }
    }

    // if(conn->pRemoteCharacteristic->canIndicate())
    //   conn->pRemoteCharacteristic->registerForNotify(notifyCallback, false);

    return true;
    
    }


  //   // Publish humidity 
  //     pCharacteristic = conn->client->getService("00001809-0000-1000-8000-00805f9b34fb")->getCharacteristic("00002a6f-0000-1000-8000-00805f9b34fb");
  // if (pCharacteristic) {
  //   conn->pRemoteCharacteristic = pCharacteristic;
  //   std::string value = pCharacteristic->readValue();
  //   if (value.length() > 0) {
  //     // Serial.print("Received data: ");
  //     // for (int i = 0; i < value.length(); i++) {
  //     //   Serial.print(String(value[i], HEX));
  //     //   Serial.print(" ");
  //     // }
  //     // Serial.println();

  //     // Convert raw data to float
  //     uint8_t convertedValue = reinterpret_cast<const uint8_t*>(value.c_str())[1];

  //     char message[50];
  //     sprintf(message, "Humidity: %d; MAC: %s", convertedValue, conn->mac.c_str());

  //     Serial.print("Preparing message: ");
  //     Serial.println(message);

  //     char topic[50];
  //     sprintf(topic, "%s/humidity", conn->device->getName());

  //     // Send converted data to MQTT broker
  //     if (client.connect("ESP32_Client")) {
  //       client.publish(topic, message);
  //       Serial.println("******ESP32 has connected to MQTT broker******");
  //     } else {
  //       Serial.println("Failed to connect to MQTT broker");
  //     }
  //   }

    // if(conn->pRemoteCharacteristic->canIndicate())
    //   conn->pRemoteCharacteristic->registerForNotify(notifyCallback, false);

    // return true;
    
    // }

    else {
      Serial.println("Failed to find characterstic for Thingy52.");
      return false;
    }
}

void setup() {
  Serial.begin(115200);
  BLEDevice::init("ESP32_BLE_Client");

  // Connect to Wi-Fi
  // Add your Wi-Fi credentials here
  WiFi.begin("Draculas", "puwgex-1dUcge-gywpej");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("******Connected to WiFi******");
  
  // Set MQTT broker
  client.setServer(mqtt_server, mqtt_port);
}

void loop() {
    // Scan for Thingy52
    BLEScan* pBLEScan = BLEDevice::getScan();
    pBLEScan->setActiveScan(true);

    BLEScanResults results = pBLEScan->start(5);

    for (int i = 0; i < results.getCount(); i++) {
      BLEAdvertisedDevice device = results.getDevice(i);
      String mac = device.getAddress().toString().c_str();

      if (thingys.get(mac) != NULL)
      {
        Serial.println("Device already connected");
        continue;
      }

      // Check if the device has the Thingy52 service UUID
      if (device.haveServiceUUID() && device.isAdvertisingService(BLEUUID("00001809-0000-1000-8000-00805f9b34fb"))) {
        // Found Thingy52, connect to it
         BtConn *conn = new BtConn();
        if (conn == NULL) {
            Serial.println(F("Memory allocation error\n"));
            exit(EXIT_FAILURE);
        }
        conn->state = IDLE;
        conn->device = new BLEAdvertisedDevice(device);
        // conn->device = new BLEAdvertisedDevice(advertisedDevice);
        conn->mac = device.getAddress().toString().c_str();
        conn->client = BLEDevice::createClient();
        // conn->client->setClientCallbacks(new ClientCallback(conn));
        if (connectClient(conn)) {
          thingys.insert(conn->mac, conn);
          conn->ctr_id = thingys.numEntries;
        }
      }
    }

    for (int i = 0; i < thingys.numEntries; ++i) {
      BtConn *conn = thingys.entries[i].value;
      String mac = thingys.entries[i].key;
      if (!(connectClient(conn)))
      {
        thingys.remove(mac);
      }
    }
    
  // else {
  //   // Connected, check for data
  //   BLERemoteCharacteristic* pCharacteristic = pClient->getService("00001809-0000-1000-8000-00805f9b34fb")->getCharacteristic("00002a1c-0000-1000-8000-00805f9b34fb");
  //   if (pCharacteristic) {
  //     std::string value = pCharacteristic->readValue();
  //     if (value.length() > 0) {
  //       Serial.print("Received data: ");
  //       for (int i = 0; i < value.length(); i++) {
  //         Serial.print(String(value[i], HEX));
  //         Serial.print(" ");
  //       }
  //       Serial.println();

  //       // Convert raw data to float
  //       float convertedValue = convertRawDataToFloat(reinterpret_cast<const uint8_t*>(value.c_str()));

  //       // Send converted data to MQTT broker
  //       if (client.connect("ESP32_Client")) {
  //         client.publish(mqtt_topic, String(convertedValue).c_str());
  //         Serial.println("******ESP32 has connected to MQTT broker******");
  //       } else {
  //         Serial.println("Failed to connect to MQTT broker");
  //       }
  //     }
  //   }
  // }

  delay(1000); // Adjust the delay as needed
  client.loop(); // Keep the MQTT connection alive
}