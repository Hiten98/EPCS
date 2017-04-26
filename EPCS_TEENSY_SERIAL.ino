String serialBuffer;
String serial1Buffer;
void setup() {
  // put your setup code here, to run once:
  //set up main serial monitor
  Serial.begin(9600);

  //setup serial ports:
  Serial1.begin(9600);
  //Serial2 = XBEE
  //Remember to put in Serial2 everywhere to send data through xbee, or just change serial 1 with serial2 
  //Serial2.begin(57600);
  Serial.println("I am awake");
  Serial1.println("I am awake");
  Serial.flush();
  Serial.flush();
}

void loop() {
  // put your main code here, to run repeatedly:
  int sent = 0;
  int sent1 = 0;
  String message = "Please enter something";
  if(Serial.available())
  {
      Serial.send_now();
      //Serial1.send_now();
      serialBuffer = "";
      while(Serial.available())
      {
        //int byte = Serial.read();
        /*Serial.print(byte);
        Serial.flush();
        Serial1.print(byte);
        Serial1.flush();
        //Serial.send_now();
        */
        char ch = Serial.read();
        //Serial.print(ch);
        serialBuffer+=ch;
        sent = 1;
      }
      sent = 1;
      Serial.println("\tSerial2");
      Serial.println(serialBuffer);
      Serial1.println(serialBuffer);
      Serial.flush();
      Serial1.flush();
      Serial.send_now();
      delay(500);
  }
  else if(Serial1.available())
  {
      Serial.send_now();
      //Serial1.send_now();
      serial1Buffer="";
      while(Serial1.available())
      {
        serial1Buffer = Serial1.read();
      }
      Serial.println("\tSerial1\t");
      Serial.println(serial1Buffer);
      Serial1.println(serial1Buffer);
      Serial.flush();
      Serial1.flush();
      delay(500);
      sent1 = 1;
  }
  else
  {
    if(sent == 1 || sent1 == 1)
    {
      Serial.println("data sent");
      Serial1.println("data sent");
      sent = 2;
      sent = 2;
    }
    if(sent == 2 || sent1 == 2)
    {
      Serial.println("Please enter something");
      Serial1.println("Please enter something");
      Serial.flush();
      Serial1.flush();
    }
    delay(500);
  }
}
