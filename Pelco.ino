#include <SoftwareSerial.h>
#include <ESP8266WiFi.h>

#define PelcorxPin D0
#define PelcotxPin D1 //Should be D1
#define ViscaSSerialRX        D5  //Serial Receive pin
#define ViscaSSerialTX        D6  //Serial Transmit pin
#define ViscaTSerialRX        D2  //Serial Receive pin
#define ViscaTSerialTX        D3  //Serial Transmit pin

SoftwareSerial PelcoSerial(PelcorxPin, PelcotxPin);
SoftwareSerial ViscaSerialRX(ViscaSSerialRX, ViscaSSerialTX); // RX, TX
SoftwareSerial ViscaSerialTX(ViscaTSerialRX, ViscaTSerialTX); // RX, TX
// These assume D1 Mini - redefine as needed
#define LED D4
#define PelcoTXEnablePin D4
#define PelcoTXEnable HIGH
#define PelcoRXEnable LOW

byte speed;
// PELCO Commands
const byte C_STOP = 0x00;
const byte C_UP = 0x08;
const byte C_DOWN = 0x10;
const byte C_LEFT = 0x04;
const byte C_RIGHT = 0x02;

const byte C_ZOOMIN = 0x20;
const byte C_ZOOMOUT = 0x40;

const byte C_SET_PAN_POSITION = 0x4B;  // 1/100ths of degree
const byte C_SET_TILT_POSITION = 0x4D; // 1/100ths of degree


void sendPelcoDFrame(byte address, byte command, byte data1, byte data2);
void blinkLED();
int panTiltSpeed(int speed);
void viscaBroadcast();
void viscaZoomResponse(int camera);
void viscaPTResponse(int camera);
void viscaACK(int camera);
void viscaComplete(int camera);

int byteSend;

void setup()
{
   pinMode(LED, OUTPUT);

  WiFi.mode(WIFI_OFF); 

  Serial.begin(115200);
  PelcoSerial.begin(9600);
  ViscaSerialRX.begin(9600);   // set the data rate   
  ViscaSerialTX.begin(9600);   // set the data rate   
  Serial.println("SerialRemote");  // Can be ignored

  pinMode(PelcoTXEnablePin, OUTPUT);

  // Start the software serial port, to another device

  Serial.println("Visca->Pelco D controller");
  delay(1000);
}

byte packet[100];

void printPacket() {
  int ii = 0;

   while (ii < sizeof(packet) && packet[ii] != 0xFF) {
//        Serial.print(packet[ii++], HEX);
        serial_printf(Serial, "%2x:", packet[ii++]);
//        Serial.print(" ");
      }
   Serial.println();     
}

bool assemblePacket() {
  static int i = 0;
    //Copy input data to output  
  while (ViscaSerialRX.available()) {
 //   Serial.println("available");
    byteSend = ViscaSerialRX.read();   // Read the byte 
    if (byteSend == 0xFF && i < sizeof(packet)) {
      packet[i] = byteSend;
      i = 0;
      return true;
    }
    else if (i < sizeof(packet))
      packet[i++] = byteSend;
    else if (i == sizeof(packet))
      i = 0;
  delay(0);
  }
  return false;
}


void loop()
{
  if (assemblePacket()) {
    bool ACKRequired = true;
    byte myAddress = packet[0] & 0X0F; //Camera  ID

    // PAN & TILT - this logic result in only Up/Down/Left/Right turning the Visca UpLeft (eg) to Up
    // if Tilt Speed < Pan Speed & Pan Left is Pan Speed >= Tilt Speed
    if (packet[1] == 1 && packet[2] == 6 && packet[3] == 1) { // PAN/TILT
      Serial.print("Camera " + String(myAddress) + " ");
      if ((packet[6] == 3 && packet[7] == 1) || //Up
          (((packet[6] == 1 && packet[7] == 1) || (packet[6] == 2 && packet[7] == 1)) && packet[5] > packet[4])) { //UpLeft,UpRight->Up
        speed = panTiltSpeed(packet[5]);
        Serial.print(String("Tilt Up Speed 0x") + String(speed, HEX));
        sendPelcoDFrame(myAddress, C_UP, speed, speed);
      } else if ((packet[6] == 3 && packet[7] == 2) || 
          (((packet[6] == 1 && packet[7] == 2) || (packet[6] == 2 && packet[7] == 2)) && packet[5] > packet[4])) { //Down
        speed = panTiltSpeed(packet[5]);
        Serial.print(String("Tilt Down Speed 0x") + String(speed, HEX));
        sendPelcoDFrame(myAddress, C_DOWN, speed, speed);
      } else if ((packet[6] == 1 && packet[7] == 3) || 
          (((packet[6] == 1 && packet[7] == 1) || (packet[6] == 1 && packet[7] == 2)) && packet[4] >= packet[5])) { //Left
        speed = panTiltSpeed(packet[4]);
        Serial.print(String("Pan Left Speed 0x") + String(speed, HEX));
        sendPelcoDFrame(myAddress, C_LEFT, speed, speed);
      } else if ((packet[6] == 2 && packet[7] == 3) ||
          (((packet[6] == 2 && packet[7] == 1) || (packet[6] == 2 && packet[7] == 2)) && packet[4] >= packet[5])) { //Right
        speed = panTiltSpeed(packet[4]);
        Serial.print(String("Pan Right Speed 0x") + String(speed, HEX));
        sendPelcoDFrame(myAddress, C_RIGHT, speed, speed);
      } else if (packet[6] == 3 && packet[7] == 3) { //Stop
        Serial.print(String("Pan/Tilt Stop"));
        sendPelcoDFrame(myAddress, C_STOP, 0, 0);  
      } else {
        printPacket();
        ACKRequired = false;
      }
    } 
    
    //Zoom
    
    else if (packet[1] == 1 && packet[2] == 4 && packet[3] == 7) { // ZOOM
      Serial.print("Zoom ");
      if ((packet[4] & 0xF0) == 0x20) { //Tele
            Serial.print(String("in Camera ") + String(myAddress) + " ");
            sendPelcoDFrame(myAddress, C_ZOOMIN, 0, 0);
      } else if ((packet[4] & 0xF0) == 0x30) { //Wide
            Serial.print(String("Out Camera ") + String(myAddress) + " ");
            sendPelcoDFrame(myAddress, C_ZOOMOUT, 0, 0);
      } else if ((packet[4] & 0xFF) == 0) { //Stop
            Serial.print(String("Stop Camera ") + String(myAddress) + " ");
            sendPelcoDFrame(myAddress, C_STOP, 0, 0);
      } else
        printPacket();
    } else if (packet[0] == 0x88 && packet[1] == 0x30 && packet[2] == 0x01) { //Broadcast
        viscaBroadcast(4);
        Serial.println("Broadcast Set Address -- 4 cameras");
        ACKRequired = false; // No need to ACK this, just broadcast back!
    } else if (((packet[0] & 0xF0) == 0x80 && packet[1] == 0x09 && packet[2] == 0x04 && packet[3] == 0x47)) { //Zoom enquiry
      viscaZoomResponse(packet[0] & 0x0F);
      ACKRequired = false;
    } else if (((packet[0] & 0xF0) == 0x80 && packet[1] == 9 && packet[2] == 6 && packet[3] == 0x12)) { //Pan/Tilt equiry
      viscaPTResponse(packet[0] & 0x0F);
      ACKRequired = false;
    }
      else {
       printPacket();
       ACKRequired = false;
    }
    if (ACKRequired) {
      viscaACK(myAddress);
      viscaComplete(myAddress); //Quick completion!
    }
  }
}

void sendPelcoDFrame(byte address, byte command, byte data1, byte data2)
{
  byte bytes[7] = {0xFF, address, 0x00, command, data1, data2, 0x00};
  byte crc = (bytes[1] + bytes[2] + bytes[3] + bytes[4] + bytes[5]) % 0x100;
  bytes[6] = crc;
  digitalWrite(PelcoTXEnablePin, PelcoTXEnable);
  Serial.print("->Pelco ");
  for (int i = 0; i < 7; i++)
  {
    PelcoSerial.write(bytes[i]);
    serial_printf(Serial, "%2X:", bytes[i]);
//     Serial.print(bytes[i], HEX); Serial.print(":"); //debug
  }
    digitalWrite(PelcoTXEnablePin, PelcoRXEnable);
    Serial.println();
}

void blinkLED()
{ 
  for (int i = 0; i < 4; i++)
  {
    digitalWrite(LED, HIGH);
    delay(100);
    digitalWrite(LED, LOW);
    delay(100);
  }
}

//Translates between Visca speed (as sent by BM ATEM) and Pelco speed
int panTiltSpeed(int speed) {

  switch (speed) {
    case 0x01:  return 0x01;
    case 0x02:  return 0x05;
    case 0x07:  return 0x10;
    case 0x0C: return 0x20;
    case 0x18: return 0x3F;

    default: return 0x20; //In the middle of the range
  }
}

void viscaBroadcast(int cameras) { //Returns 1 broadcast per camera - this sets up the ATEM to recognise the number of cameras it can control
  for (int i=1; i<=cameras; i++) {
    ViscaSerialTX.write(0x88); 
    ViscaSerialTX.write(0x30); 
    ViscaSerialTX.write(i+1); // <-- Camera# + 1
    ViscaSerialTX.write(0xFF); 
  }
} 

void viscaZoomResponse(int camera) {//Response to Zoom position query (failing to do this slows the ATEM while it waits for a response
  // y0 50 0p 0q 0r 0s FF
  ViscaSerialTX.write(0x80 | (camera << 4));
  ViscaSerialTX.write(0x50);
  for (int i=0; i<4; i++)
    ViscaSerialTX.write(0x04); // Make up some value
  ViscaSerialTX.write(0xFF);
}

void viscaPTResponse(int camera) { //Response to Pan Tilt position query (failing to do this slows the ATEM while it waits for a response from each camera
  // y0 50 0w 0w 0w 0w 0z 0z 0z 0z FF
  ViscaSerialTX.write(0x80 | (camera << 4));
  ViscaSerialTX.write(0x50);
  for (int i=0; i<8; i++)
    ViscaSerialTX.write(0x04); // Make up some value
  ViscaSerialTX.write(0xFF);
}

void viscaACK(int camera) {
  // z0 4y FF
  ViscaSerialTX.write(0x80 | (camera << 4));  
  ViscaSerialTX.write(0x40);
  ViscaSerialTX.write(0xFF);
}

void viscaComplete(int camera) {
  // z0 5y FF
  ViscaSerialTX.write(0x80 | (camera << 4));  
  ViscaSerialTX.write(0x50);
  ViscaSerialTX.write(0xFF);
}

void serial_printf(HardwareSerial& serial, const char* fmt, ...) { 
    va_list argv;
    va_start(argv, fmt);

    for (int i = 0; fmt[i] != '\0'; i++) {
        if (fmt[i] == '%') {
            // Look for specification of number of decimal places
            int places = 2;
            if (fmt[i+1] == '.') i++;  // alw1746: Allows %.4f precision like in stdio printf (%4f will still work).
            if (fmt[i+1] >= '0' && fmt[i+1] <= '9') {
                places = fmt[i+1] - '0';
                i++;
            }
            
            switch (fmt[++i]) {
                case 'B':
                    serial.print("0b"); // Fall through intended
                case 'b':
                    serial.print(va_arg(argv, int), BIN);
                    break;
                case 'c': 
                    serial.print((char) va_arg(argv, int));
                    break;
                case 'd': 
                case 'i':
                    serial.print(va_arg(argv, int), DEC);
                    break;
                case 'f': 
                    serial.print(va_arg(argv, double), places);
                    break;
                case 'l': 
                    serial.print(va_arg(argv, long), DEC);
                    break;
                case 'o':
                    serial.print(va_arg(argv, int) == 0 ? "off" : "on");
                    break;
                case 's': 
                    serial.print(va_arg(argv, const char*));
                    break;
                case 'X':
                    serial.print("0x"); // Fall through intended
                case 'x':
                    serial.print(va_arg(argv, int), HEX);
                    break;
                case '%': 
                    serial.print(fmt[i]);
                    break;
                default:
                    serial.print("?");
                    break;
            }
        } else {
            serial.print(fmt[i]);
        }
    }
    va_end(argv);
}
