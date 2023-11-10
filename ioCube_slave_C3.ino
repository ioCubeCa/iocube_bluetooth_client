#include "BLEDevice.h"
#include <Wire.h>
//ESP32_C3 ioCube
//LED io_10
//IIC SCL io_4
//IIC SDA io_5
//uart1 TXD io_19
//uart1 RXD io_18

#define bleStateLed     10
bool ledState=false;

#define uartBufferSize  136
static uint8_t uartRecvBuffer[uartBufferSize]={};
static uint8_t uartRecvIndex=0;
static byte buffer_sum=0;

static byte myId=0x10;
static byte keyH=0;
static byte keyL=0;
static byte getIdRetry=3;
static bool onRequestID = false;
static byte ledToggleCount = 0;
//=====================================================================
static BLEUUID SERVICE_UUID("00001101-0000-1000-8000-00805F9B34FB");
static BLEUUID CHARACTERISTIC_UUID_RX("ac94d26c-4bff-11ec-81d3-0242ac130003");
static BLEUUID CHARACTERISTIC_UUID_TX("ce85b106-4c00-11ec-81d3-0242ac130003");

static BLEAdvertisedDevice* myDevice;
static BLERemoteCharacteristic* pRemoteCharacteristic;
static BLERemoteCharacteristic* pSendCharacteristic;
static boolean doConnect = false;
static boolean doScan = false;
static boolean serverConnected = false;
static uint8_t reScanCount = 0;
//---------------------------------------------------------------------
static void notifyCallback(
  BLERemoteCharacteristic* pBLERemoteCharacteristic,
  uint8_t* pData,
  size_t length,
  bool isNotify) {
    
    byte bChar;
    byte bSum=0;

    Serial.print("BLE Recv(");
    Serial.print(length);
    Serial.print("):");
    
    for (byte i = 0; i < length; i++){
      bChar = *pData;
      
      if(onRequestID==false)Serial1.write(bChar);
      
      if(bChar<16)Serial.print(0,HEX);
      Serial.print(bChar,HEX);
      pData++;
      uartRecvBuffer[i]=bChar;
      if(i <= (length-2))bSum+=bChar;
    }
    Serial.println();  

    //checksum correct
    if(bSum == uartRecvBuffer[length-1]){
      //command = setID
      if(uartRecvBuffer[4] == 0x09){
        //key value equal
        if((uartRecvBuffer[7]==keyH)&&(uartRecvBuffer[8]==keyL)){
          //targetId == myId
          if(uartRecvBuffer[2]==myId){
            myId=uartRecvBuffer[5];
            onRequestID=false;
            ledToggleCount = (myId+1)*2;
          }
        }
      }
    }
}
//---------------------------------------------------------------------
class MyClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) {
  }

  void onDisconnect(BLEClient* pclient) {
    serverConnected = false;
    Serial.println("BLE server Disconnected.");
  }
};
//---------------------------------------------------------------------
bool connectToServer() {
    Serial.print("Forming a connection to ");
    Serial.println(myDevice->getAddress().toString().c_str());
    
    BLEClient*  pClient  = BLEDevice::createClient();
    Serial.println(" - Created client");

    pClient->setClientCallbacks(new MyClientCallback());

    // Connect to the remove BLE Server.
    pClient->connect(myDevice);  // if you pass BLEAdvertisedDevice instead of address, it will be recognized type of peer device address (public or private)
    Serial.println(" - Connected to server");

    // Obtain a reference to the service we are after in the remote BLE server.
    BLERemoteService* pRemoteService = pClient->getService(SERVICE_UUID);
    if (pRemoteService == nullptr) {
      Serial.print("Failed to find our service UUID: ");
      Serial.println(SERVICE_UUID.toString().c_str());
      pClient->disconnect();
      return false;
    }
    Serial.println(" - Found our service");


    // Obtain a reference to the characteristic in the service of the remote BLE server.
    pRemoteCharacteristic = pRemoteService->getCharacteristic(CHARACTERISTIC_UUID_TX);
    if (pRemoteCharacteristic == nullptr) {
      Serial.print("Failed to find our characteristic UUID: ");
      Serial.println(CHARACTERISTIC_UUID_TX.toString().c_str());
      pClient->disconnect();
      return false;
    }
    Serial.println(" - Found our characteristic");

    // Read the value of the characteristic.
    if(pRemoteCharacteristic->canRead()) {
      std::string value = pRemoteCharacteristic->readValue();
      Serial.print("The characteristic value was: ");
      Serial.println(value.c_str());
    }

    if(pRemoteCharacteristic->canNotify())
      pRemoteCharacteristic->registerForNotify(notifyCallback);

   // Obtain a reference to the characteristic in the service of the remote BLE server.
    pSendCharacteristic = pRemoteService->getCharacteristic(CHARACTERISTIC_UUID_RX);
    if (pSendCharacteristic == nullptr) {
      Serial.print("Failed to find our characteristic UUID: ");
      Serial.println(CHARACTERISTIC_UUID_RX.toString().c_str());
      pClient->disconnect();
      return false;
    }

    serverConnected = true;
    digitalWrite(bleStateLed,HIGH);
    return true;
}
//---------------------------------------------------------------------
/**
 * Scan for BLE servers and find the first one that advertises the service we are looking for.
 */
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
 /**
   * Called for each advertising BLE server.
   */
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    Serial.println("BLE Advertised Device found.");
    toggleBleLed();
    //Serial.print("BLE Advertised Device found: ");
    //Serial.println(advertisedDevice.toString().c_str());

    // We have found a device, let us now see if it contains the service we are looking for.
    if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(SERVICE_UUID)) {

      BLEDevice::getScan()->stop();
      myDevice = new BLEAdvertisedDevice(advertisedDevice);
      doConnect = true;
      doScan = true;

    } // Found our server
  } // onResult
}; // MyAdvertisedDeviceCallbacks

//=====================================================================
void setup() {
  Serial.begin(115200);
  Serial1.begin(57600, SERIAL_8N1, 18, 19);
  Wire.begin(5,4);
  Serial.println("BLE Client application.");

  pinMode(bleStateLed,OUTPUT);
  //create BLE device  
  BLEDevice::init("");
  // scan to run for 5 seconds.
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setInterval(1349);
  pBLEScan->setWindow(449);
  pBLEScan->setActiveScan(true);
  pBLEScan->start(5, false);
} // End of setup.
//---------------------------------------------------------------------
// This is the Arduino main loop function.
void loop() {

  byte bChar;

  if (doConnect == true) {
    if (connectToServer()) {
      Serial.println("Connect to server sucess.");
      getMyId();
    } else {
      Serial.println("Connect to server failure.");
    }
    doConnect = false;
  }

  while(Serial.available()>0){
   uartRecvBuffer[uartRecvIndex] = Serial.read();
   uartRecvIndex++;
   delay(5);
  }

  while(Serial1.available()>0){
   uartRecvBuffer[uartRecvIndex] = Serial1.read();
   uartRecvIndex++;
   delay(5);
  }
  
  if (serverConnected) {
    if(uartRecvIndex){
      pSendCharacteristic->writeValue((uint8_t*)&uartRecvBuffer[0],uartRecvIndex);
      //print log
      Serial.print("BLE Send(");
      Serial.print(uartRecvIndex);
      Serial.print("):");
      for (byte i = 0; i < uartRecvIndex; i++){
        bChar = uartRecvBuffer[i];
        if(bChar<16)Serial.print(0,HEX);
        Serial.print(bChar,HEX);
      }
      Serial.println();  
            
      uartRecvIndex=0;
    }
  }else if(doScan){
    BLEDevice::getScan()->start(1);
    doScan=false;
    toggleBleLed();
    delay(1900); // Delay a second between loops.
  }else{
    toggleBleLed();
    delay(400);
    reScanCount++;
    if(reScanCount > 40){
      reScanCount=0;
      doScan=true;
    }

    if(uartRecvIndex){
      Serial.print("Throw away(");
      Serial.print(uartRecvIndex);
      Serial.println(").");
      uartRecvIndex=0;
    }
    
  }

  cap1293_read();
  if(uartRecvIndex >= uartBufferSize)uartRecvIndex=0;

  if(ledToggleCount){
    toggleBleLed();
    ledToggleCount--;
    if(ledToggleCount==0)digitalWrite(bleStateLed,HIGH);
  }
  delay(100);
  
} // End of loop
//=====================================================================
void toUartBuffer(byte bChar){
   uartRecvBuffer[uartRecvIndex] = bChar;
   buffer_sum+=bChar;
   uartRecvIndex++;
}
//---------------------------------------------------------------------
void toUartSum(){
  uartRecvBuffer[uartRecvIndex] = buffer_sum;
  uartRecvIndex++;
}
//---------------------------------------------------------------------
void toggleBleLed(){
  ledState = !ledState;
  digitalWrite(bleStateLed,ledState);  
}
//---------------------------------------------------------------------
void getMyId(){
  if(myId==0x10){
    if(getIdRetry){
      buffer_sum   = 0;
      uartRecvIndex = 0;
      toUartBuffer(0);             //IIC Address(broadcast)
      toUartBuffer(5);             //length
      toUartBuffer(0xFE);          //TargetId:0x00~0xFE 0xFF->broadcast
      toUartBuffer(myId);          //sourceId
      toUartBuffer(0x19);          //command request ID
      keyH = rand();
      toUartBuffer(keyH);          //random key high byte
      keyL = rand();
      toUartBuffer(keyL);          //random key low byte
      toUartSum();        
      getIdRetry--;
      onRequestID=true;
    }
  }
}
//=====================================================================
//CAP1293
static byte cap1293_old=0;
static byte cap1293_count[3]={};
//---------------------------------------------------------------------
void cap1293_read(){
  byte tmp;
  byte rIndex;
  byte inputValue;
  byte recvBuffer[3]={};
  byte button_state;
  byte button_index;
  byte error;

  Wire.beginTransmission(0x28);
  Wire.write(0x10);
  delay(1);
  error = Wire.endTransmission(true);

  uint8_t bytesReceived = Wire.requestFrom(0x28, 3);
  if((bool)bytesReceived){   //If received more than zero bytes
    Wire.readBytes(recvBuffer, bytesReceived);
  } 
  for(uint8_t i=0;i<3;i++)if(recvBuffer[i] > 0x7F)recvBuffer[i]=0;

  inputValue=0;
  if(recvBuffer[0] > 0x40){
    inputValue += 0x01;
  }
  if(recvBuffer[1] > 0x40){
    inputValue += 0x02;
  }
  if(recvBuffer[2] > 0x40){
    inputValue += 0x04;
  }

    if(inputValue != cap1293_old){
      buffer_sum   = 0;
      uartRecvIndex = 0;

    if(inputValue > cap1293_old){
      switch(inputValue){
        case 0x01:
          cap1293_count[0]++;
          rIndex = cap1293_count[0];
          break;
        case 0x02:
          cap1293_count[1]++;
          rIndex = cap1293_count[1];
          break;
        case 0x04:
          cap1293_count[2]++;
          rIndex = cap1293_count[2];
          break;
      }
      button_index = inputValue;
      button_state = 1;
    }else{
      button_index = cap1293_old;
      button_state = 0;
    }
      
      toUartBuffer(0);             //IIC Address(broadcast)
      toUartBuffer(10);            //length
      toUartBuffer(0xFF);          //TargetId:0x00~0xFE 0xFF->broadcast
      toUartBuffer(myId);          //sourceId
      toUartBuffer(0x10);          //command:input_event 
      toUartBuffer(button_index);  //button index
      toUartBuffer(button_state);  //button state 0:off 1:on
      tmp = rand();
      toUartBuffer(tmp);           //random value
      toUartBuffer(rIndex);        //button Count
      toUartBuffer(recvBuffer[0]); //square value
      toUartBuffer(recvBuffer[1]); //circle value
      toUartBuffer(recvBuffer[2]); //cross  value
      toUartSum();  
      //send data to Serial1
      for(byte i=0;i<uartRecvIndex;i++)Serial1.write(uartRecvBuffer[i]);
    }
  
  cap1293_old = inputValue;
}
//=====================================================================
