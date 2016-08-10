//(c) xupypr https://vk.com/xupypr_grodno
#include <Arduino.h>
#include <TM1637Display.h>
#define CLK 6 //Пин CLK дисплея (можно заменить на другой)
#define DIO 5 //Пин DIO дисплея (можно заменить на другой)
TM1637Display display(CLK, DIO);
uint8_t data_clr[] = {0x00, 0x00, 0x00, 0x00}; //Чистый дисплей
uint8_t data[] = {0xff, 0xff, 0xff, 0xff}; //Заполненный дисплей
uint8_t SEG_ERR[] = { //Массив для вывода слова Err на экран
  SEG_A | SEG_D | SEG_E | SEG_F | SEG_G,           // E
  SEG_E | SEG_G ,                                  // r
  SEG_E | SEG_G                                    // r
  };
volatile unsigned long photocell1=0, photocell2=0, firstShot=0; //значения в микросекундах пролета шарика мимо датчика, и время первого выстреля для вычисления скорострельности
volatile int shot1=0,shot2=0; // Кол-во шаров пролетевших мимо 1 и 2 датчиков
volatile bool newShot=false; // Новый выстрел
float photoDistance=0.091; //  СЮДА ВПИСЫВАЕМ СВОЕ ЗНАЧЕНИЕ В МЕТРАХ МЕЖДУ ВХОДНЫМИ/ВЫХОДНЫМИ ДАТЧИКАМИ
int lastShot=0; // Номер последнего выстрела
int speedBB=0; // Скорость выстрела
bool err=false, dispFlag=false; // Флаги ошибки и вывода на дисплей
bool blocked2=false, blocked3=false; // Флаги настройки чувствительности датчиков
bool dispClear; // Очищен ли дисплей
int analog2, analog3; //Для записи значений с аналоговых входов
unsigned long rateFire=0; // Скорострельность
unsigned long prMicros2=0,prMicros3=0; //Для перехода в режим настройки чувствительности в случае перекрытия датчиков больше, чем на 1.5 сек
unsigned long clearDisplayTime=5000000, prMicrosClear=0; //Если хотим, чтобы дисплей стирал значения меньше или больше чем, через 5 сек - меняем значение clearDisplayTime. 1млн=1 сек.

void setup() {
attachInterrupt(0,Photo1,RISING); //Прерывание на входном датчике
attachInterrupt(1,Photo2,RISING); //Прерывание на выходном датчике
Serial.begin(9600);
display.setBrightness(0x0f); //Яркость дисплей
SEG_ERR[3]=0; 
display.setSegments(data);
}
//функция, выполняемая при пролете шара через 1 барьер
void Photo1() {
photocell1=micros();
if (!newShot) firstShot=photocell1; //для расчета скорострельности
shot1++;
newShot=true;
}
//функция, выполняемая при пролете шара через 2 барьер
void Photo2() {
photocell2=micros();
shot2++;
}
void PrintDisplay() {
  Serial.print (speedBB);
  Serial.println (" m/s");
  Serial.print (rateFire);
  Serial.println (" rpm"); //Вывод скорости и скоростельности в COM порт
    dispClear=false;
    prMicrosClear=micros();
    if (!err && rateFire==0) display.showNumberDec(speedBB, false, 4, 0);  //Вывод скорости
    if (!err && rateFire!=0) { //Вывод скоростельности
    display.showNumberDec(rateFire, false, 4, 0);
    rateFire=0;}
    if (err) {display.setSegments(SEG_ERR);err=false;} //Вывод ошибки
    dispFlag=false;
}
void Setting2() { //Настройка 1 фотодатчиков
   analog2=analogRead(A3); //Значения с фотоприемников1
   Serial.println(analog2); // Вывод значения в COM порт
   data[0] = 0b00000110; //1-ца
   data[1]=0;
   data[2]=0;
   data[3]=0;
   if (analog2>410) data[1]=0b00000001;
   if (analog2<390) data[1]=0b00001000;
   if (analog2>390 && analog2<410) data[1]=0b01000000;
   display.setSegments(data); //Вывод на дисплей
}

void Setting3() { //Настройка 1 фотодатчиков
  analog3=analogRead(A2); //Значения с фотоприемников2
   Serial.println(analog3); // Вывод значения в COM порт
   data[0] = 0b01011011; //2ка
   data[1]=0;
   data[2]=0;
   data[3]=0;
   if (analog3>410) data[1]=0b00000001;
   if (analog3<390) data[1]=0b00001000;
   if (analog3>390 && analog3<410) data[1]=0b01000000;
   display.setSegments(data);
}

void ClearDisplay() { //Очистка дисплея
 display.setSegments(data_clr);
  dispClear=true;
}

void loop() {
if (analogRead(A3)<=450) prMicros2=micros(); 
if (analogRead(A2)<=450) prMicros3=micros();
if (micros()-prMicros2>1500000) blocked2=true;
if (micros()-prMicros3>1500000 && !blocked2) blocked3=true;
if (blocked2) Setting2();
if (blocked3) Setting3();
// Если 1 из датчиков засорен или значения превышают установленные - переход в режим настройки

if (newShot) { //Если проскочил новый выстрел
  ClearDisplay();
  if (micros()-photocell1 > 1000000) { // Если после последнего выстрела прошла 1 сек.
  if (photocell2>photocell1 && shot1==shot2) { // Если все нормал
    speedBB=(10000000*(photoDistance)/(photocell2-photocell1)); // Скорость
    if (shot1-lastShot>4) rateFire=60000000/((photocell1-firstShot)/(shot1-lastShot)); //Если 5 или больше выстрелов - считаем скорострельность
  } else err=true; // Если что-то не так, ставим флаг ошибки
    dispFlag=true;
    newShot=false;
    if (!err) lastShot=shot1; else {shot1=lastShot;shot2=lastShot;}  // Если не было ошибки, задаем значение посл. выстрела.
  }
}

if (dispFlag && !blocked2 && !blocked3) PrintDisplay(); // Вывод скорости или скорострельности на экран. Ну или ошибки.

if (micros()-prMicrosClear>clearDisplayTime && !blocked2 && !blocked3 && !dispFlag && !dispClear) ClearDisplay(); // Очистка дисплея после заданного времени
delay(300); //Задержка 0.3 сек перед повторением.
}
