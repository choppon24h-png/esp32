#include "operaBLE.h"

#ifdef USAR_ESP32_UART_BLE

// ============================================================
// Variaveis globais BLE
// ============================================================

BLEServer         *pServer           = NULL;
BLECharacteristic *pTxCharacteristic = NULL;
bool               deviceConnected    = false;
bool               oldDeviceConnected = false;

// ============================================================
// Advertising — sem seguranca, conexao direta
// ============================================================

static void iniciaAdvertising() {
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMaxPreferred(0x12);
    BLEDevice::startAdvertising();
    DBG_PRINT(F("\n[BLE] Advertising iniciado"));
}

// ============================================================
// Callbacks de conexao
// ============================================================

class MyServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer *pServer) {
        deviceConnected = true;
        digitalWrite(PINO_STATUS, LED_STATUS_ON);
        DBG_PRINT(F("\n[BLE] Conectado"));
    }

    void onDisconnect(BLEServer *pServer) {
        deviceConnected = false;
        digitalWrite(PINO_STATUS, !LED_STATUS_ON);
        DBG_PRINT(F("\n[BLE] Desconectado"));
    }
};

// ============================================================
// Callbacks de escrita RX — sem autenticacao
// ============================================================

class MyCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
        std::string rxValue = pCharacteristic->getValue();
        if (rxValue.length() > 0) {
            String cmd(rxValue.c_str(), rxValue.length());
            cmd.trim();
            DBG_PRINT(F("\n[BLE] Recebido: "));
            DBG_PRINT(cmd);
            executaOperacao(cmd);
        }
    }
};

// ============================================================
// Task FreeRTOS — reinicia advertising apos desconexao
// ============================================================

void taskBLE(void *pvParameters) {
    DBG_PRINT(F("\n[BLE] Task de gerenciamento iniciada"));
    for (;;) {
        if (!deviceConnected && oldDeviceConnected) {
            vTaskDelay(pdMS_TO_TICKS(500));
            iniciaAdvertising();
            oldDeviceConnected = false;
            DBG_PRINT(F("\n[BLE] Aguardando nova conexao"));
        }
        if (deviceConnected && !oldDeviceConnected) {
            oldDeviceConnected = true;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// ============================================================
// Nome BLE dinamico: CHOPP_XXXX com MAC
// ============================================================

static String geraNomeBle() {
    const uint64_t efuseMac = ESP.getEfuseMac() & 0xFFFFFFFFFFFFULL;
    char macHex[13];
    snprintf(macHex, sizeof(macHex), "%012llX", efuseMac);
    String bleName = String(BLE_NAME_PREFIX) + String(macHex).substring(0, 4);
    bleName.toUpperCase();
    return bleName;
}

// ============================================================
// Setup BLE — SEM bond, SEM PIN, conexao direta
// ============================================================

void setupBLE() {
    String bleName = geraNomeBle();
    DBG_PRINT(F("\n[BLE] Nome: "));
    DBG_PRINT(bleName);

    BLEDevice::init(bleName.c_str());

    // SEM seguranca — conexao direta igual ao nRF Connect
    // SEM setEncryptionLevel, SEM setSecurityCallbacks
    // SEM esp_ble_gap_set_security_param

    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    BLEService *pService = pServer->createService(SERVICE_UUID);

    pTxCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_TX,
        BLECharacteristic::PROPERTY_NOTIFY
    );
    pTxCharacteristic->addDescriptor(new BLE2902());

    BLECharacteristic *pRxCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_RX,
        BLECharacteristic::PROPERTY_WRITE
    );
    pRxCharacteristic->setCallbacks(new MyCallbacks());

    pService->start();
    iniciaAdvertising();

    xTaskCreate(taskBLE, "taskBLE", 4096, NULL, 1, NULL);

    DBG_PRINT(F("\n[BLE] Setup concluido — aguardando conexao direta"));
}

// ============================================================
// Envio BLE
// ============================================================

void enviaBLE(String msg) {
    if (!deviceConnected || pTxCharacteristic == NULL) {
        DBG_PRINT(F("\n[BLE] enviaBLE ignorado: sem conexao"));
        return;
    }
    msg += '\n';
    pTxCharacteristic->setValue(msg.c_str());
    pTxCharacteristic->notify();
}

#endif
