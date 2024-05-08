#include <Adafruit_NeoPixel.h>
#include <Adafruit_SSD1306.h>
#include "TRSensors.h"
#include <Wire.h>

#define PWMA   6           //Left Motor Speed pin (ENA)
#define AIN2   A0          //Motor-L forward (IN2).
#define AIN1   A1          //Motor-L backward (IN1)
#define PWMB   5           //Right Motor Speed pin (ENB)
#define BIN1   A2          //Motor-R forward (IN3)
#define BIN2   A3          //Motor-R backward (IN4)
#define PIN 7
#define NUM_SENSORS 5
#define OLED_RESET 9
#define OLED_SA0   8
#define Addr  0x20
#define RGB1 

Adafruit_SSD1306 display(OLED_RESET, OLED_SA0);


TRSensors trs = TRSensors();
unsigned int sensorValues[NUM_SENSORS];
unsigned int position;
uint16_t i, j;
byte value;
unsigned long lasttime = 0;
Adafruit_NeoPixel RGB = Adafruit_NeoPixel(4, PIN, NEO_GRB + NEO_KHZ800);

void PCF8574Write(byte data);
byte PCF8574Read();
uint32_t Wheel(byte WheelPos);

void RGB_setup();
void follow_segment();
void turn(unsigned char dir);
unsigned char select_turn(unsigned char found_left, unsigned char found_straight, unsigned char found_right);
void simplify_path();

void setup() {

  RGB_setup();
  delay(100);

  Wire.begin();
  pinMode(PWMA,OUTPUT);                     
  pinMode(AIN2,OUTPUT);      
  pinMode(AIN1,OUTPUT);
  pinMode(PWMB,OUTPUT);       
  pinMode(AIN1,OUTPUT);     
  pinMode(AIN2,OUTPUT); 

  SetSpeeds(0,0);

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(5,0);
  display.println("RoboEmpire");
  display.setCursor(10,18);
  display.println("Fulger");
  display.setCursor(20,35);
  display.println("McQueen");
  display.setTextSize(1);
  display.setCursor(10,55);
  display.println("Press to calibrate");
  display.display();
  
  while(value != 0xEF)  //se asteapta apasarea butonului
  {
    PCF8574Write(0x1F | PCF8574Read());
    value = PCF8574Read() | 0xE0;
  }
  
  delay(500);
  for (int i = 0; i < 100; i++) //Calibrare
  {
    if(i<25 || i >= 75)
      SetSpeeds(50,-50);
    else
      SetSpeeds(-50,50);
    trs.calibrate();
  }

  SetSpeeds(0,0); 
  
  value = 0;
  while(value != 0xEF)  //se asteapta apasarea butonului
  {
    PCF8574Write(0x1F | PCF8574Read());
    value = PCF8574Read() | 0xE0;
    position = trs.readLine(sensorValues)/200;
    display.clearDisplay();
    display.setCursor(0,25);
    display.println("Calibration Done !");
    display.setCursor(0,55);
    for (int i = 0; i < 21; i++)
    {
      display.print('_');
    }
    display.setCursor(position*6,55);
    display.print("**");
    display.display();
  }

  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.println("RoboEmpire");
  display.setTextSize(3);
  display.setCursor(40,30);
  display.println("Go!");
  display.display();
  delay(500);
}

void loop() {
  goto jump;
  jump:
  while (1)
  {
    follow_segment();
    SetSpeeds(30, 30);
    delay(40);
    unsigned char found_left = 0;
    unsigned char found_straight = 0;
    unsigned char found_right = 0;
    trs.readLine(sensorValues);
    if (sensorValues[0] > 600)
      found_left = 1;
    if (sensorValues[4] > 600)
      found_right = 1;
    SetSpeeds(30, 30);
    delay(100);
    trs.readLine(sensorValues);
    if (sensorValues[1] > 600 || sensorValues[2] > 600 || sensorValues[3] > 600)
      found_straight = 1;
    if (sensorValues[0] > 600 && sensorValues[1] > 600 && sensorValues[2] > 600 && sensorValues[3] > 600 && sensorValues[4] > 600)
    {
      delay(500);
      SetSpeeds(0, 0);
      break;
    }
    unsigned char dir = select_turn(found_left, found_straight, found_right);
    turn(dir);
  }
  // Labirint rezolvat
  while (1)
  {
    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(WHITE);
    display.setCursor(0,0);
    display.println("RoboEmpire");
    display.setCursor(10,25);
    display.println("THE END!");
    display.display();
    delay(5000);
    goto jump;
  }
}

void SetSpeeds(int Aspeed,int Bspeed)
{
  if(Aspeed < 0)
  {
    digitalWrite(AIN1,HIGH);
    digitalWrite(AIN2,LOW);
    analogWrite(PWMA,-Aspeed);      
  }
  else
  {
    digitalWrite(AIN1,LOW); 
    digitalWrite(AIN2,HIGH);
    analogWrite(PWMA,Aspeed);  
  }
  
  if(Bspeed < 0)
  {
    digitalWrite(BIN1,HIGH);
    digitalWrite(BIN2,LOW);
    analogWrite(PWMB,-Bspeed);      
  }
  else
  {
    digitalWrite(BIN1,LOW); 
    digitalWrite(BIN2,HIGH);
    analogWrite(PWMB,Bspeed);  
  }
}

void PCF8574Write(byte data)
{
  Wire.beginTransmission(Addr);
  Wire.write(data);
  Wire.endTransmission(); 
}

byte PCF8574Read()
{
  int data = -1;
  Wire.requestFrom(Addr, 1);
  if(Wire.available()) {
    data = Wire.read();
  }
  return data;
}

void RGB_setup()
{
  RGB.begin();
  RGB.setPixelColor(0,0xffffff );
  RGB.setPixelColor(1,0xffffff );
  RGB.setPixelColor(2,0xffffff );
  RGB.setPixelColor(3,0xffffff );
  RGB.show();
}

void follow_segment()
{
  int last_proportional = 0;
  long integral=0;

  while(1)
  {
    unsigned int position = trs.readLine(sensorValues);
    int proportional = ((int)position) - 2000;
    int derivative = proportional - last_proportional;
    integral += proportional;
    last_proportional = proportional;
    int power_difference = proportional/20 + integral/10000 + derivative*10;

    const int maximum = 60; // VITEZA MAXIMA
    if (power_difference > maximum)
      power_difference = maximum;
    if (power_difference < -maximum)
      power_difference = - maximum;

    if (power_difference < 0)
    {
      analogWrite(PWMA,maximum + power_difference);
      analogWrite(PWMB,maximum);
    }
    else
    {
      analogWrite(PWMA,maximum);
      analogWrite(PWMB,maximum - power_difference);
    }

   if(millis() - lasttime > 100)
    if ((sensorValues[1] < 150 && sensorValues[2] < 150 && sensorValues[3] < 150) || (sensorValues[0] > 600 || sensorValues[4] > 600))
      return;
  }
}

void turn(unsigned char dir)
{
    switch(dir)
    {
    case 'R':
      // STANGA
      SetSpeeds(100, -100);
      delay(120);
      break;
    case 'L':
      // DREAPTA
      SetSpeeds(-100, 100);
      delay(120);
      break;
    case 'B':
      // INTOARCE
      SetSpeeds(100, -100);
      delay(270);
      break;
    case 'S':
      // NU FACE NIMIC
      break;
    }
  SetSpeeds(0, 0);
  lasttime = millis();
}

unsigned char select_turn(unsigned char found_left, unsigned char found_straight, unsigned char found_right)
{
  if (found_right)
    return 'R';
  else if (found_straight)
    return 'S';
  else if (found_left)
    return 'L';
  else
    return 'B';
}