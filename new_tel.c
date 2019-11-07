//Biblotecas para leer del bus CAN
#include <SPI.h>
#include <Canbus.h>
#include <defaults.h>
#include <global.h>
#include <mcp2515.h>
#include <mcp2515_defs.h>

//Biblioteca para escribir en TX hacia la placa radio
#include <SoftwareSerial.h>

//Biblioteca para codificar en base64 la trama de datos
#include "config.h"

//Inicializar los pines donde se hara la comunicacion a la placa de Radio
SoftwareSerial swSerial(RX_BAOFENG, TX_BAOFENG); // RX, TX

// Variable para contar tiempo
unsigned long currentMillis = 0;
unsigned long previousMillis = 0;

struct trama0x03B {
	float packCurrent;
	float packVoltage;
	float packSOC;

} trama_03B_data;

struct trama0x3CB {
	byte cellIdMinVoltage;
	byte cellIdMaxVoltage;

	float minCellVoltageMeanValue;
	float maxCellVoltageMeanValue;
	float minTempMeanValue;
	float maxTempMeanValue;
} trama_3CB_data;


void setup() {
	Serial. begin(BAURATE);
	swSerial.begin(BAURATE_BAOFENG);
	swSerial.println(F("MhellokRATOS"));

	if (Canbus.init(CANSPEED_250)) {
		Serial.println(F("CAN Init ok"));
		

	} else {
		Serial.println(F("Can't init CAN"));
	}
}

//********************************Main Loop*********************************//

void loop() {
	currentMillis = millis();
	//Con el siguiente if puede escribir en cualquier momento a la placa de radio. Solo es poner el monitor en 115200 baudios
	while (Serial.available()) {
		char data = Serial.read();
		Serial.print(data);
		swSerial.print(data);
	}

	tCAN message;
	if (mcp2515_check_message()) {
		if (mcp2515_get_message(&message)) {
			switch(message.id) {
				case 0x03B:
				trama_03B_data.packCurrent = (float)((int)(message.data[0] << 8) + (int)(message.data[1]));
				trama_03B_data.packVoltage = (float)((int)(message.data[2] << 8) + (int)(message.data[3]));
				trama_03B_data.packSOC = (float)(message.data[6]);
				break;

				case 0x3CB:
				trama_3CB_data.cellIdMinVoltage = 
				break;
			}
		}
	}
	if (currentMillis - previousMillis >= TRANSMIT_INTERVAL) {

	}
}

