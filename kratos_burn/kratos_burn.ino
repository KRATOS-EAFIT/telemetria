//Code from:
//https://github.com/sparkfun/SparkFun_CAN-Bus_Arduino_Library/blob/master/examples/CAN_Read_Demo/CAN_Read_Demo.ino
//https://github.com/sparkfun/SparkFun_CAN-Bus_Arduino_Library/blob/master/examples/SparkFun_SD_Demo/SparkFun_SD_Demo.ino
//https://www.arduino.cc/en/Tutorial/SoftwareSerialExample
/****************************************************************************
CAN Read Demo for the SparkFun CAN Bus Shield. 
Written by Stephen McCoy. 
Original tutorial available here: http://www.instructables.com/id/CAN-Bus-Sniffing-and-Broadcasting-with-Arduino
Used with permission 2016. License CC By SA. 
Distributed as-is; no warranty is given.
*************************************************************************/

//Biblotecas para leer del bus CAN
#include <Canbus.h>
#include <defaults.h>
#include <global.h>
#include <mcp2515.h>
#include <mcp2515_defs.h>

//Biblioteca para escribir en TX hacia la placa radio
#include <SoftwareSerial.h>
//Bibliotecas para escribir a la sd del Shield CAN
#include <SPI.h>
#include <SD.h>
//Biblioteca para codificar en base64 la trama de datos
#include <Base64.h>

//Si no compila asegurese de no tener pines de entrada en los puertos TX/RX
//Si no transmite puede ser porque necesite apretar # en el Baofeng para que ponga más potencia
//Conectar los cables RX/TX cruzados a la placa de Radio. En realidad usa solo pin 0:RX que va al TX de la placa
// Para hacer funcionar el CAN, conectar los mcp tierra con tierra y el bus a CAN_H y CAN_LOW

//Si escribe directamente en el monitor, lo enviara por el pin de RX
//Si es por programacion lo enviara por el pin definido como TX abajo en la linea de inicializacion del swSerial
//Si por casualidad no manda un mensaje en un id determinado, cambiarle la velocidad en el bms utility y poner a que envie el mensaje mas rapido, esto pasa porque todos los manda al mismo tiempo entonces el arduino no lo alcanza a leer, y como lo manda en un intervalo de tiempo muy largo(ej:cada 104ms), entonces no alcanza a leerlo.
//Se recomienda bajarle a todos los mensajes a que los envie a lo sumo cada 64 ms y cada uno en un diferente intervalo de tiempo
//********************************Setup Loop*********************************//

//Inicializar los pines donde se hara la comunicacion a la placa de Radio
SoftwareSerial swSerial(2, 3); // RX, TX

// Variable para contar tiempo
static unsigned long timeCounterTramaPpal = 0;
static unsigned long timeCounterTramaSec = 0;

// Pin para escribir a la SD
// Chip Select pin is tied to pin 9 on the SparkFun CAN-Bus Shield
const int chipSelect = 9;


enum CAN_0x03B
{
    packCurrent, //0
    packCurrent_byte2, //1
    packMomentaryVoltage, //2
    packMomentaryVoltage_byte2,
    packAmperesHour,
    packAmperesHour_byte2,
    packSOC
};

enum CAN_0x3CB
{
  cellIdMinVoltage,
  cellIdMaxVoltage,
  minCellVoltage,
  minCellVoltage_byte2,
  maxCellVoltage,
  maxCellVoltage_byte2,
  minTemp,
  maxTemp
};

struct trama0x03B {
  float packCurrentMeanValue;   
  int packCurrentTotalValues = 0;      

  float packMomentaryVoltageMeanValue;
  int packMomentaryVoltageTotalValues = 0;

  float packAmperesHourMeanValue;
  int packAmperesHourTotalValues = 0;

  float packSOCMeanValue;
  int packSOCTotalValues = 0;

} trama_03B_data;

struct trama0x3CB {

  int cellIdMinVoltage;
  int cellIdMaxVoltage;


  float minCellVoltageMeanValue;
  int minCellVoltageTotalValues = 0;  
  
  float maxCellVoltageMeanValue;
  int maxCellVoltageTotalValues = 0;

  float minTempMeanValue;
  int minTempTotalValues = 0;

  float maxTempMeanValue;
  int maxTempTotalValues = 0;

} trama_3CB_data;

void setup() {
  Serial.begin(115200); // For debug use
  // Open serial communications and wait for port to open:
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }

  //************************ SD ***************************

  Serial.print("Initializing SD card... ");
   // make sure that the default chip select pin is set to
  // output, even if you don't use it:
  //pinMode(chipSelect, OUTPUT);
  //digitalWrite(chipSelect, HIGH);
  //pinMode(10, OUTPUT);
  // see if the card is present and can be initialized:
  if (!SD.begin(chipSelect)) {
    Serial.println("Card failed, or not present");
    // don't do anything more:
    //return;
  }else{
    Serial.println("card initialized.");
  }

  //************************ </SD> ***************************
  
  Serial.println("CAN Read - Testing receival of CAN Bus message");  
  delay(100);
  
  if(Canbus.init(CANSPEED_250))  //Initialise MCP2515 CAN controller at the specified speed
    Serial.println("CAN Init ok");
  else
    Serial.println("Can't init CAN");
    
  Serial.println("Goodnight moon!");

  // set the data rate for the SoftwareSerial port
  //La placa de radio funciona a 115200 baud
  swSerial.begin(115200);
  //La M al inicio es para que la placa entienda que se va a transmitir un mensaje
  char* data = "MhellokRATOS";
  swSerial.print(data);
  

  delay(150);

  //Tiene que ser la ultima instruccion del setup antes de un delay
  timeCounterTramaPpal = millis();
  timeCounterTramaSec = millis();
}

//********************************Main Loop*********************************//

void loop(){
  //Con el siguiente if puede escribir en cualquier momento a la placa de radio. Solo es poner el monitor en 115200 baudios
  while (Serial.available()) {
    char data = Serial.read();
    Serial.print("caracter: ");
    Serial.println(data);
    swSerial.write(data);
  }

  bool allowSendTramaPpal = checkTimeToSendTramaPpal();
      bool allowSendTramaSec = checkTimeToSendTramaSec();
      if (allowSendTramaPpal == true){
        //Empaquetar la trama
        Serial.println("Enviando trama ppal");
        int LEN_TRAMA_PPAL = 2;
        char* tramaPpal_paquete = empaquetarTramaPpal(LEN_TRAMA_PPAL);
        //delay(100);
        char* tramaPpal_encoded = codificarTrama(tramaPpal_paquete , LEN_TRAMA_PPAL);
        Serial.println("trama ppal antes de enviar");
        byte tramaTypeId = 0x50; //=> 'P'
        //Enviar la trama despues de empaquetar
        sendData(tramaTypeId, tramaPpal_encoded );
        //clean trama ppal:
        Serial.println("Limpiando trama");

        //Se parte en dos bytes porque el dato es demasiado grande
        //Se parte en dos bytes porque 0 dato es demasiado grande        

        
        trama_03B_data.packCurrentMeanValue = 0;
        trama_03B_data.packCurrentTotalValues = 0;      
        trama_03B_data.packMomentaryVoltageMeanValue = 0;
        trama_03B_data.packMomentaryVoltageTotalValues = 0;
        trama_03B_data.packAmperesHourMeanValue = 0;
        trama_03B_data.packAmperesHourTotalValues = 0;
        trama_03B_data.packSOCMeanValue = 0;
        trama_03B_data.packSOCTotalValues = 0;

        trama_3CB_data.minCellVoltageMeanValue = 0;
        trama_3CB_data.minCellVoltageTotalValues = 0;
        trama_3CB_data.maxCellVoltageMeanValue  = 0;
        trama_3CB_data.maxCellVoltageTotalValues = 0;
        trama_3CB_data.minTempMeanValue  = 0;
        trama_3CB_data.minTempTotalValues  = 0;
        trama_3CB_data.maxTempMeanValue  = 0;
        trama_3CB_data.maxTempTotalValues  = 0;

        Serial.println("-----*******----------------********-----");


        //trama_651_data = {}; // reset        
        //trama_03B_data = {}; // reset
        //trama_3CB_data = {}; // reset
      }
      
  tCAN message;
  if (mcp2515_check_message()) 
  {
    if (mcp2515_get_message(&message)) 
    {

      //Serial.println("Llego mensaje");

      
      
      /*      
      if (swSerial.available()) {
        Serial.write(swSerial.read());
      }
      */

      

      Serial.print("ID: 0x");
      Serial.print(message.id,HEX);
      Serial.print(", ");
      Serial.print("Data: ");
      Serial.print(message.header.length,DEC);
      Serial.print(" | ");
      for(int i=0;i<message.header.length;i++) 
      { 
        //Serial.print("i:");
        //Serial.println(i);

        Serial.print(message.data[i],DEC);
        Serial.print(" , ");
      }
      Serial.println("");
      

      //***************************************************************************************************************************************************
    
      if(message.id == 0x03B){
        for(int i=0;i<message.header.length;i++) 
        { 
          switch((CAN_0x03B)i) {
            
            case packCurrent:
              //Se toman dos bytes para packCurrentMeanValue porque el dato es demasiado grande

              //Serial.println("Recalculando corriente del paquete"); 
              
              //Serial.println((int)(message.data[i] << 8) + (int)(message.data[i+1]));

              trama_03B_data.packCurrentTotalValues += 1; 
              trama_03B_data.packCurrentMeanValue = incrementalMeaning(trama_03B_data.packCurrentMeanValue,(float)((int)(message.data[i] << 8) + (int)(message.data[i+1])), trama_03B_data.packCurrentTotalValues); 
              Serial.println("Corriente del paquete:");
              Serial.println(trama_03B_data.packCurrentMeanValue);
              break;
            case packMomentaryVoltage:
              //Se toman dos bytes para packMomentaryVoltageMeanValue porque el dato es demasiado grande  

              //Serial.println("Voltaje del paquete:");
              //Serial.println(trama_03B_data.packMomentaryVoltageMeanValue);

              //(message.data[i]) (message.data[i+1])

              trama_03B_data.packMomentaryVoltageTotalValues += 1; 
              trama_03B_data.packMomentaryVoltageMeanValue = incrementalMeaning(trama_03B_data.packMomentaryVoltageMeanValue,(float)((int)(message.data[i] << 8) + (int)(message.data[i+1])), trama_03B_data.packMomentaryVoltageTotalValues);
              break;
            case packAmperesHour:


              trama_03B_data.packAmperesHourTotalValues += 1; 
              trama_03B_data.packAmperesHourMeanValue = incrementalMeaning(trama_03B_data.packAmperesHourMeanValue,(float)((int)(message.data[i] << 8) + (int)(message.data[i+1])), trama_03B_data.packAmperesHourTotalValues); 
              break;

            case packSOC:
              trama_03B_data.packSOCTotalValues += 1; 
              trama_03B_data.packSOCMeanValue = incrementalMeaning(trama_03B_data.packSOCMeanValue,((float)(message.data[i])), trama_03B_data.packSOCTotalValues);   // Este dato viene dividido por 2
              break;
  
            default: 
              //Serial.println("Unrecognized Option"); 
              break;

          } 
        }
      }

      //***************************************************************************************************************************************************


      if(message.id == 0x3CB){
        for(int i=0;i<message.header.length;i++) 
        { 
          switch((CAN_0x3CB)i) {
            case cellIdMinVoltage:
              trama_3CB_data.cellIdMinVoltage = message.data[i];
              break;
            
            case cellIdMaxVoltage:
              trama_3CB_data.cellIdMaxVoltage = message.data[i];
              break;
            
            case minCellVoltage:
              trama_3CB_data.minCellVoltageTotalValues += 1; 
              trama_3CB_data.minCellVoltageMeanValue = incrementalMeaning(trama_3CB_data.minCellVoltageMeanValue,(float)((int)(message.data[i] << 8) + (int)(message.data[i+1])), trama_3CB_data.minCellVoltageTotalValues); 
              break;
            
            case maxCellVoltage:
              trama_3CB_data.maxCellVoltageTotalValues += 1; 
              trama_3CB_data.maxCellVoltageMeanValue = incrementalMeaning(trama_3CB_data.maxCellVoltageMeanValue,(float)((int)(message.data[i] << 8) + (int)(message.data[i+1])), trama_3CB_data.maxCellVoltageTotalValues); 
              break;
            
            case minTemp:
              trama_3CB_data.minTempTotalValues += 1; 
              trama_3CB_data.minTempMeanValue = incrementalMeaning(trama_3CB_data.minTempMeanValue,(float)(message.data[i]), trama_3CB_data.minTempTotalValues); 
            
            
            case maxTemp:
              trama_3CB_data.maxTempTotalValues += 1; 
              trama_3CB_data.maxTempMeanValue = incrementalMeaning(trama_3CB_data.maxTempMeanValue,(float)(message.data[i]), trama_3CB_data.maxTempTotalValues); 
              break;

            default: 
              //Serial.println("Unrecognized Option"); 
              break;
          }
        }
      }

      

      //***************************************************************************************************************************************************
      
      
      /*
      if (allowSendTramaSec == true){
        Serial.println("Enviando trama secundaria");  
        //Empaquetar la trama
        int LEN_TRAMA_SEC = 7;
        char* tramaSec_paquete =  empaquetarTramaSec(LEN_TRAMA_SEC);
        delay(100);
        char* tramaPpal_encoded = codificarTrama(tramaSec_paquete, LEN_TRAMA_SEC);
        Serial.println("trama ppal antes de enviar");
        byte tramaTypeId = 0x53; //=> 'S'
        //Enviar la trama despues de empaquetar
        sendData(tramaTypeId, tramaPpal_encoded );
        //clean trama ppal:
        Serial.println("Limpiando trama");
        
        trama_651_data.lowestVoltageMeanValue = 0;
        trama_651_data.highestVoltageMeanValue = 0;
        trama_651_data.meanVoltageMeanValue = 0;
        trama_651_data.maxCellsMeanValue = 0;
        trama_651_data.occupiedCellsMeanValue = 0;
        trama_3CB_data.limiteCorrDescargaMeanValue = 0;
        trama_3CB_data.limiteCorrCargaMeanValue = 0;

        trama_651_data.lowestVoltageTotalValues = 0;
        trama_651_data.highestVoltageTotalValues = 0;
        trama_651_data.meanVoltageTotalValues = 0;
        trama_651_data.maxCellsTotalValues = 0;
        trama_651_data.occupiedCellsTotalValues = 0;
        trama_3CB_data.limiteCorrDescargaTotalValues = 0;
        trama_3CB_data.limiteCorrCargaTotalValues = 0;

      }
      */
    }
  }
}


bool checkTimeToSendTramaPpal(){
  //Minimo cada 3 segundos enviar la trama
  return checkTime(&timeCounterTramaPpal, 4000);
}
bool checkTimeToSendTramaSec(){
  //Minimo cada 3 segundos enviar la trama
  return checkTime(&timeCounterTramaSec, 20000);
}


bool checkTime(unsigned long *timeCounter, int delta){
  //Serial.println("timeCounter");
  //Serial.println((unsigned long)*timeCounter);
  //Serial.println(millis() - (unsigned long)*timeCounter);
  if(millis() - (unsigned long)*timeCounter > delta){
    //Serial.println("Reinicia contador");
    //Reiniciar el contador de tiempo
    *timeCounter = millis();
    return true;
  }
  return false;
}

char* empaquetarTramaPpal(int len){
  //Verificar que todos los datos que se vayan a enviar sean debidamente limpiados

  
  //Transaformaciones:
  trama_03B_data.packCurrentMeanValue *= 100;
  trama_03B_data.packSOCMeanValue *= 10;
  
  Serial.println("DATA:");
  
  Serial.println((trama_03B_data.packCurrentMeanValue));
  Serial.println(trama_03B_data.packSOCMeanValue);
  Serial.println(trama_03B_data.packMomentaryVoltageMeanValue);
  Serial.println(trama_03B_data.packAmperesHourMeanValue);

  Serial.println(trama_3CB_data.cellIdMinVoltage);
  Serial.println(trama_3CB_data.cellIdMaxVoltage);
  

  Serial.println(trama_3CB_data.minTempTotalValues );
  Serial.println(trama_3CB_data.maxTempMeanValue );
  Serial.println(trama_3CB_data.minCellVoltageMeanValue);
  Serial.println(trama_3CB_data.maxCellVoltageMeanValue );
  
  


  //Big endian
  int frstBytePackCurrent = ((int)trama_03B_data.packCurrentMeanValue) >> 8;
  int scndBytePackCurrent = ((int)trama_03B_data.packCurrentMeanValue) & 0x00ff;

  //Big endian
  int frstBytePackSOC = ((int)trama_03B_data.packSOCMeanValue) >> 8;
  int scndBytePackSOC = ((int)trama_03B_data.packSOCMeanValue) & 0x00ff;


  //Big endian
  //Se parte packMomentaryVoltageMeanValue en dos bytes porque el dato es demasiado grande  
  int frstByteMomentaryPackVoltage = ((int)trama_03B_data.packMomentaryVoltageMeanValue) >> 8;
  int scndByteMomentaryPackVoltage = ((int)trama_03B_data.packMomentaryVoltageMeanValue) & 0x00ff;


  //Big endian
  int frstBytePackAmperesHour = ((int)trama_03B_data.packAmperesHourMeanValue) >> 8;
  int scndBytePackAmperesHour = ((int)trama_03B_data.packAmperesHourMeanValue) & 0x00ff;
  //Big endian
  int frstByteMinCellVoltage  = ((int)trama_3CB_data.minCellVoltageMeanValue) >> 8;
  int scndByteMinCellVoltage = ((int)trama_3CB_data.minCellVoltageMeanValue)  & 0x00ff;

  //Big endian
  int frstByteMaxCellVoltage = ((int)trama_3CB_data.maxCellVoltageMeanValue) >> 8;
  int scndByteMaxCellVoltage = ((int)trama_3CB_data.maxCellVoltageMeanValue) & 0x00ff;
  
  byte byteArray[] = {
    2,
    3
/*
    frstBytePackCurrent, //frstBytePackCurrent,
    scndBytePackCurrent, //scndBytePackCurrent,
    frstBytePackSOC,
    scndBytePackSOC,
    frstByteMomentaryPackVoltage,
    scndByteMomentaryPackVoltage,
    frstBytePackAmperesHour,              //2
    scndBytePackAmperesHour,               //3
    (int)trama_3CB_data.minTempMeanValue,
    (int)trama_3CB_data.maxTempMeanValue,
    (int)trama_3CB_data.cellIdMinVoltage,
    (int)trama_3CB_data.cellIdMaxVoltage,
    frstByteMinCellVoltage,               //7
    scndByteMinCellVoltage,
    frstByteMaxCellVoltage,               //5   //Creo que no es enviar el maximo sino el mas alto, porque el maximo creo que es el maximo registro en el tiempo
    scndByteMaxCellVoltage               //6
*/
  };
  
  return empaquetarTrama(len, byteArray);
}

char* empaquetarTramaSec(int len){
  byte byteArray[] = {
    /*
    (int)trama_651_data.lowestVoltageMeanValue,
    (int)trama_651_data.highestVoltageMeanValue,
    (int)trama_651_data.meanVoltageMeanValue,
    (int)trama_651_data.maxCellsMeanValue,
    (int)trama_651_data.occupiedCellsMeanValue,
    (int)trama_3CB_data.limiteCorrDescargaMeanValue,
    (int)trama_3CB_data.limiteCorrCargaMeanValue
    */
  };
  
  return empaquetarTrama(len, byteArray);

}

char* empaquetarTrama(int len, byte* byteArray){

  char tramaPaquete[len];
  
  Serial.println("Empaquetando Trama");
  for (int i = 0; i<len; i++)
  {
    tramaPaquete[i] = (char)byteArray[i];
    //Serial.print(byteArray[i],HEX);
    //Serial.print(',');
  }
  //Serial.println("");
  Serial.println("trama empaquetada");
  Serial.println(tramaPaquete);

  return tramaPaquete;
}


char* codificarTrama(char* trama, int len){
  Serial.println("Codificando trama entrante");
  //Serial.println(numero de datos en trama);
  int inputLen = len;
  Serial.println(inputLen);
  
  int encodedLen = base64_enc_len(inputLen);
  char encoded[encodedLen];

  // note input is consumed in this step: it will be empty afterwards
  base64_encode(encoded, trama, inputLen);
  Serial.println("Codificado:");
  Serial.println(encoded);
  
  return encoded;
  
}
void sendData(byte tramaTypeId, char* trama){
  Serial.println("Enviando datos");

  // 0x4d => M => para que la placa radio interprete que se quiere enviar un mensaje
  swSerial.print((char)0x4d);
  //Id de tramas KRATOS
  // 0x4b => K => para que en pits sepan que es una trama de Kratos
  swSerial.print((char)0x4b);
  swSerial.print((char)tramaTypeId);
  
  int tamano = strlen(trama);
  //Serial.println("size of encoded dataset");
  //Serial.println(tamano);
  
  for (int i = 0; i < tamano; i++)
  {
    //Codigo para enviar al shield
    swSerial.print(trama[i]);
    Serial.print(trama[i]);
  }
  Serial.println("");
}

/*
 * Algoritmo sacado de:
 * https://math.stackexchange.com/a/106720
 * m:mean
 * m_: mean sub
 * n: total numbers
 * m_n= m_(n−1) + {(a_n−m_(n−1)) / n }
*/
float incrementalMeaning(float pastMean, float newValue, int totalValues){
  //unsigned long timeC = millis();
  float division = ((newValue - pastMean) / totalValues);
  division = division + pastMean;
  //Serial.println("millis() - timeC");
  //Serial.println(millis() - timeC);
  // toma 1 miliseg
  return division;
}
