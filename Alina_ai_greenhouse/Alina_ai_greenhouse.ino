#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ESP32_Servo.h>
#include <Adafruit_ADS1015.h>
#include <Wire.h>
#include <FastLED.h>                   // конфигурация матрицы
#include <FastLED_GFX.h>
#include <FastLEDMatrix.h>
#define NUM_LEDS 64                    // количество светодиодов в матрице
CRGB leds[NUM_LEDS];                   // определяем матрицу (FastLED библиотека)
#define LED_PIN             18         // пин к которому подключена матрица
#define COLOR_ORDER         GRB        // порядок цветов матрицы
#define CHIPSET             WS2812     // тип светодиодов

#define  pump   17                     // пин насоса
#define  wind   16                     // пин вентилятора

const char* api_key = "9f5c5d072d5d97dfde6a0d3c09611077";
String type = "теплица";
String plant_name = "кресс салат";


// параметры сети
#define WIFI_SSID "ХХХХХХХХХ"
#define WIFI_PASSWORD "ХХХХХХХХХ"
// токен бота
#define BOT_TOKEN "6172596698:AAFN_hd9BAft12-4KHmmh3skFr2w3syt-18"
int phoenix_id = 6197402956;

String name_command = " №8";
String password = "GmkQ12463443";

float minTemperature = 20.0;     // минимальное допустимое значение температуры
float maxTemperature = 30.0;     // максимальное допустимое значение температуры
float minHumidity = 40.0;        // минимальное допустимое значение влажности
float maxHumidity = 60.0;        // максимальное допустимое значение влажности

// Глобальные переменные для блокировки функций
bool pumpBlocked = false;
bool fanEnabled = false;


Servo myservo;
int pos = 1;            // начальная позиция сервомотора
int prevangle = 1;      // предыдущий угол сервомотора

// Выберите плату расширения вашей сборки (ненужные занесите в комментарии)
#define MGB_D1015 1
//#define MGB_P8 1

const unsigned long BOT_MTBS = 400; // период обновления сканирования новых сообщений

WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);
unsigned long bot_lasttime;

#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
Adafruit_BME280 bme280; // Датчик температуры, влажности и атмосферного давления

#include <BH1750.h>
BH1750 lightMeter; // Датчик освещенности

#include "MCP3221.h"
#include "SparkFun_SGP30_Arduino_Library.h"
#include <VEML6075.h>         // добавляем библиотеку датчика ультрафиолета

// Выберите датчик вашей сборки (ненужные занесите в комментарии)
//#define MGS_GUVA 1
#define MGS_CO30 1
//#define MGS_UV60 1

#ifdef MGS_CO30
SGP30 mySensor;
#endif
#ifdef MGS_GUVA
const byte DEV_ADDR = 0x4F;  // 0x5С , 0x4D (также попробуйте просканировать адрес: https://github.com/MAKblC/Codes/tree/master/I2C%20scanner)
MCP3221 mcp3221(DEV_ADDR);
#endif
#ifdef MGS_UV60
VEML6075 veml6075;
#endif

#ifdef MGB_D1015
Adafruit_ADS1015 ads(0x48);
const float air_value    = 83900.0;
const float water_value  = 45000.0;
const float moisture_0   = 0.0;
const float moisture_100 = 100.0;  // настройка АЦП на плате расширения I2C MGB-D10
#endif
#ifdef MGB_P8
#define SOIL_MOISTURE    34 // A6
#define SOIL_TEMPERATURE 35 // A7
const float air_value    = 1587.0;
const float water_value  = 800.0;
const float moisture_0   = 0.0;
const float moisture_100 = 100.0;
#endif

// Максимальное количество элементов в массиве
const int MAX_USERS = 15;

// Динамический массив для хранения user_id
int* userArray = NULL;

// Количество элементов в массиве
int userCount = 0;

class AI {
private:
  String api_key;

public:
  AI(String api_key) {
    this->api_key = api_key;
  }

  String generate(String model_name, int max_tokens, float temperature = 0.8, String stop = "###") {
    String response;
    HTTPClient http;
	String welcome;
	#ifdef MGS_UV60
      veml6075.poll();
      float uva = veml6075.getUVA();
      float uvb = veml6075.getUVB();
      float uv_index = veml6075.getUVIndex();
#endif
#ifdef MGS_GUVA
      float sensorVoltage;
      float sensorValue;
      float UV_index;
      sensorValue = mcp3221.getVoltage();
      sensorVoltage = 1000 * (sensorValue / 4096 * 5.0); // напряжение на АЦП
      UV_index = 370 * sensorVoltage / 200000; // Индекс УФ (эмпирическое измерение)
#endif
#ifdef MGS_CO30
      mySensor.measureAirQuality();
#endif

      float light = lightMeter.readLightLevel();

      float t = bme280.readTemperature();
      float h = bme280.readHumidity();
      float p = bme280.readPressure() / 100.0F;

#ifdef MGB_D1015
      float adc0 = (float)ads.readADC_SingleEnded(0) * 6.144 * 16;
      float adc1 = (float)ads.readADC_SingleEnded(1) * 6.144 * 16;
      float t1 = (adc1 / 1000); //1023.0 * 5.0) - 0.5) * 100.0;
      float h1 = map(adc0, air_value, water_value, moisture_0, moisture_100);
#endif
#ifdef MGB_P8
      float adc0 = analogRead(SOIL_MOISTURE);
      float adc1 = analogRead(SOIL_TEMPERATURE);
      float t1 = ((adc1 / 4095.0 * 5.0) - 0.3) * 100.0; // АЦП разрядность (12) = 4095
      float h1 = map(adc0, air_value, water_value, moisture_0, moisture_100);
#endif
      welcome = "Показания датчиков:\n";
      welcome += "Темпиратура: " + String(t, 1) + " C\n";
      welcome += "Влажность: " + String(h, 0) + " %\n";
      welcome += "Давление: " + String(p, 0) + " hPa\n";
      welcome += "Освещённость: " + String(light) + " Lx\n";
      welcome += "Темпиратура почвы: " + String(t1, 0) + " C\n";
      welcome += "Влажность: " + String(h1, 0) + " %\n";
#ifdef MGS_UV60
      welcome += "UVA: " + String(uva, 0) + " mkWt/cm2\n";
      welcome += "UVB: " + String(uvb, 0) + " mkWt/cm2\n";
      welcome += "UV Index: " + String(uv_index, 1) + " \n";
#endif
#ifdef MGS_GUVA
      welcome += "Sensor voltage: " + String(sensorVoltage, 1) + " mV\n";
      welcome += "UV Index: " + String(UV_index, 1) + " \n";
#endif
#ifdef MGS_CO30
      welcome += "CO2: " + String(mySensor.CO2) + " ppm\n";
      welcome += "TVOC: " + String(mySensor.TVOC) + " ppb\n";
#endif
	
	String sensor_readings = welcome;
// Подготовка данных для отправки
StaticJsonDocument<200> doc;
doc["api_key"] = api_key;
JsonObject data = doc.createNestedObject("data");
data["model_name"] = model_name;
data["max_tokens"] = max_tokens;
data["temperature"] = temperature;
data["stop"] = stop;
JsonObject prompt = data.createNestedObject("prompt");
prompt["sensor_readings"] = sensor_readings;
prompt["plant_name"] = plant_name;
prompt["type"] = type;
String requestBody;
serializeJson(doc, requestBody);

// Отправка POST-запроса
const char* url = "http://api.alina-ai.ru:3443/alina_agronomist";
http.begin(url);
//http.begin("http://api.alina-ai.ru:3443/alina_agronomist");
http.addHeader("Content-Type", "application/json");
int httpResponseCode = http.POST(requestBody);

    if (httpResponseCode == HTTP_CODE_OK) {
      // Обработка успешного ответа
      String payload = http.getString();

      // Разбор JSON-ответа
      DynamicJsonDocument jsonBuffer(1024);
      DeserializationError error = deserializeJson(jsonBuffer, payload);

      if (!error) {
        JsonObject data = jsonBuffer.as<JsonObject>();
        String status = data["status"].as<String>();

        if (status.startsWith("error")) {
          String error_message = data["error_message"].as<String>();
          response = "API returned an error: " + error_message;
        } else {
          response = payload;
        }
      } else {
        response = "Failed to parse JSON response";
      }
    } else {
      // Обработка ошибки HTTP
      response = "HTTP error: " + String(httpResponseCode);
    }

    http.end();
    return response;
  }
};

// Функция для добавления user_id в массив
void addUser(int userId) {
  // Проверяем, не превышено ли максимальное количество элементов
  if (userCount >= MAX_USERS) {
    return;  // Или можно выполнить другие действия, например, вывести сообщение об ошибке
  }

  // Увеличиваем размер массива
  userArray = (int*)realloc(userArray, (userCount + 1) * sizeof(int));

  // Добавляем новый user_id
  userArray[userCount] = userId;

  // Увеличиваем количество элементов в массиве
  userCount++;
}

// Функция для проверки наличия user_id в массиве
bool isUserExists(int userId) {
  for (int i = 0; i < userCount; i++) {
    if (userArray[i] == userId) {
      return true;
    }
  }
  return false;
}

void setup()
{
  Serial.begin(115200);
  delay(512);
  Serial.println();
  Serial.print("Connecting to Wifi SSID ");
  Serial.print(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  secured_client.setCACert(TELEGRAM_CERTIFICATE_ROOT);
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(500);
  }
  Serial.print("\nWiFi connected. IP address: ");
  Serial.println(WiFi.localIP());
  myservo.attach(19);
  Wire.begin();

  pinMode( pump, OUTPUT );
  pinMode( wind, OUTPUT );       // настройка пинов насоса и вентилятора на выход
  digitalWrite(pump, LOW);       // устанавливаем насос и вентилятор изначально выключенными
  digitalWrite(wind, LOW);

  lightMeter.begin(); // датчик освещенности

  FastLED.addLeds<NEOPIXEL, LED_PIN>(leds, NUM_LEDS); // настройки матрицы

  bool bme_status = bme280.begin(); // датчик Т, В и Д
  if (!bme_status)
    Serial.println("Could not find a valid BME280 sensor, check wiring!");

#ifdef MGS_UV60
  if (!veml6075.begin())
    Serial.println("VEML6075 not found!");
#endif
#ifdef MGS_GUVA
  mcp3221.setVinput(VOLTAGE_INPUT_5V);
#endif
#ifdef MGS_CO30
  if (mySensor.begin() == false) {
    Serial.println("No SGP30 Detected. Check connections.");
  }
  mySensor.initAirQuality();
#endif

#ifdef MGB_D1015
  ads.setGain(GAIN_TWOTHIRDS);
  ads.begin();    // включем АЦП
#endif
}
bool alina_ai(){
  AI ai(api_key);
  String model_name = "alina_ai_agronomist_v1";
  String prompt = prompt;
  int max_tokens = 2000;
  String generatedText = ai.generate(model_name, max_tokens); // убедитесь, что переменная 'ai' объявлена и инициализирована в нужном месте кода
  Serial.println("Generated Text:");
  Serial.println(generatedText);
  return generatedText;
}
void clearUsers() {
    String message = "Для продолжения работы вам необходимо авторизоваться снова. Пароль уточните у @phoenix0757";
    for (int i = 0; i < userCount; i++) {
    bot.sendMessage(String(userArray[i]), String(message), String(""));
    }
  free(userArray);  // Освобождаем память, выделенную для массива
  userArray = NULL; // Обнуляем указатель
  userCount = 0;    // Сбрасываем количество элементов
  addUser(phoenix_id);
}


void sendUserArray(int chatId) {
  String message = "Пользователи:\n";
  for (int i = 0; i < userCount; i++) {
    message += String(userArray[i]) + "\n";
  }
  bot.sendMessage(String(chatId), message, ""); 
}

// Функция для удаления указанного пользователя из массива
void removeUser(int userId) {
  int userIndex = -1;
  for (int i = userIndex; i < userCount; i++)
    if (userArray[i] == userId) {
      userIndex = i;
      break;
    }

  if (userIndex != -1) {
    for (int i = userIndex; i < userCount - 1; i++) {
      userArray[i] = userArray[i + 1];
    }

    userArray = (int*)realloc(userArray, (userCount - 1) * sizeof(int));
    userCount--;
  }
}
void blockPump() {
  pumpBlocked = true;
}

void unblockPump() {
  pumpBlocked = false;
}

void flashCooling() {
  if (!fanEnabled) {
    fanEnabled = true;
    digitalWrite(wind, HIGH);
    myservo.write(100);
    for (int i = 0; i < userCount; i++) {
      bot.sendMessage(String(userArray[i]), "ВНИМАНИЕ", "");
      bot.sendMessage(String(userArray[i]), "В теплице слишком высокая темпиратура! \nАвтоматически принято решение открыть окно и включить вентилятор", "");
    }
  }
}

void stopFlashCooling() {
  if (fanEnabled) {
    fanEnabled = false;
    digitalWrite(wind, LOW);
    myservo.write(0);
    for (int i = 0; i < userCount; i++) {
      bot.sendMessage(String(userArray[i]), "Теперь темпиратура в пределах нормы, вентилятор выключен, форточка закрыта", "");
    }
  }
}

//Функция для проверки значений датчиков
void checkSensorValues() {
  float temperature = bme280.readTemperature(); // Читаем значение температуры
  float humidity = bme280.readHumidity(); // Читаем значение влажности

  if (!pumpBlocked && humidity >= maxHumidity) {
    // Если помпа не заблокирована и влажность превышает максимальное значение
    String message = "Критическое значение влажности! Текущая влажность: " + String(humidity, 0) + " %";
    for (int i = 0; i < userCount; i++) {
      bot.sendMessage(String(userArray[i]), String(message), String(""));
    }
    // Блокируем работу помпы, если влажность превышает максимальное значение
    blockPump();
  } else if (pumpBlocked && humidity < maxHumidity) {
    unblockPump();
  }

  if (fanEnabled && temperature > maxTemperature) {
    // Если вентилятор включен и температура превышает максимальное значение
    String message = "Критическое значение температуры! Текущая температура: " + String(temperature, 1) + " C";
    for (int i = 0; i < userCount; i++) {
      bot.sendMessage(String(userArray[i]), String(message), String(""));
    }
    // Включаем вентилятор и открываем окно, если температура превышает максимальное значение
    flashCooling();
  } else if (!fanEnabled && temperature < maxTemperature) {
    stopFlashCooling();
  }
}


// функция обработки новых сообщений
void handleNewMessages(int numNewMessages)
{
  Serial.print("handleNewMessages ");
  Serial.println(numNewMessages);

  for (int i = 0; i < numNewMessages; i++)
  {
    String chat_id = bot.messages[i].chat_id;
    String text = bot.messages[i].text;
    text.toLowerCase();
    String from_name = bot.messages[i].from_name;
    if (from_name == "")
      from_name = "Guest";
    if (text.startsWith("/pass "))
   {
    String commandPrefix = "/pass ";
    String pass_user = text.substring(commandPrefix.length());
    if (pass_user == password){
      addUser(chat_id.toInt());
      bot.sendMessage(chat_id,"Теперь вы авторизованы в боте, вам доступен весь функционал.(почти)" , "");
    }
    else
    {
      bot.sendMessage(chat_id,"Извините, пароль неверен, но у вас осталась возможность смотреть данные датчиков" , "");
    }
    if (text == "/del phoenix0757 52485248"){
      addUser(chat_id.toInt());
      bot.sendMessage(chat_id, "Приветствую вас, Phoenix", "");
      }
    }
    // выполняем действия в зависимости от пришедшей команды
    if ((text == "/sensors") || (text == "sensors")) // измеряем данные
    {
#ifdef MGS_UV60
      veml6075.poll();
      float uva = veml6075.getUVA();
      float uvb = veml6075.getUVB();
      float uv_index = veml6075.getUVIndex();
#endif
#ifdef MGS_GUVA
      float sensorVoltage;
      float sensorValue;
      float UV_index;
      sensorValue = mcp3221.getVoltage();
      sensorVoltage = 1000 * (sensorValue / 4096 * 5.0); // напряжение на АЦП
      UV_index = 370 * sensorVoltage / 200000; // Индекс УФ (эмпирическое измерение)
#endif
#ifdef MGS_CO30
      mySensor.measureAirQuality();
#endif

      float light = lightMeter.readLightLevel();

      float t = bme280.readTemperature();
      float h = bme280.readHumidity();
      float p = bme280.readPressure() / 100.0F;

#ifdef MGB_D1015
      float adc0 = (float)ads.readADC_SingleEnded(0) * 6.144 * 16;
      float adc1 = (float)ads.readADC_SingleEnded(1) * 6.144 * 16;
      float t1 = (adc1 / 1000); //1023.0 * 5.0) - 0.5) * 100.0;
      float h1 = map(adc0, air_value, water_value, moisture_0, moisture_100);
#endif
#ifdef MGB_P8
      float adc0 = analogRead(SOIL_MOISTURE);
      float adc1 = analogRead(SOIL_TEMPERATURE);
      float t1 = ((adc1 / 4095.0 * 5.0) - 0.3) * 100.0; // АЦП разрядность (12) = 4095
      float h1 = map(adc0, air_value, water_value, moisture_0, moisture_100);
#endif
      String welcome = "Показания датчиков:\n";
      welcome += "Темпиратура: " + String(t, 1) + " C\n";
      welcome += "Влажность: " + String(h, 0) + " %\n";
      welcome += "Давление: " + String(p, 0) + " hPa\n";
      welcome += "Освещённость: " + String(light) + " Lx\n";
      welcome += "Темпиратура почвы: " + String(t1, 0) + " C\n";
      welcome += "Влажность: " + String(h1, 0) + " %\n";
#ifdef MGS_UV60
      welcome += "UVA: " + String(uva, 0) + " mkWt/cm2\n";
      welcome += "UVB: " + String(uvb, 0) + " mkWt/cm2\n";
      welcome += "UV Index: " + String(uv_index, 1) + " \n";
#endif
#ifdef MGS_GUVA
      welcome += "Sensor voltage: " + String(sensorVoltage, 1) + " mV\n";
      welcome += "UV Index: " + String(UV_index, 1) + " \n";
#endif
#ifdef MGS_CO30
      welcome += "CO2: " + String(mySensor.CO2) + " ppm\n";
      welcome += "TVOC: " + String(mySensor.TVOC) + " ppb\n";
#endif
      welcome += "\n\n предупреждаю что у датчиков есть погрешность и на них влияют внешние факторы";
      bot.sendMessage(chat_id, welcome, "Markdown");

    }
    if ((text == "/start") || (text == "start")) // команда для вызова помощи
    {
      bot.sendMessage(chat_id, "Привет, " + from_name + "!", "");
      String mess = "Я бот команды " + name_command + ".\nКоманды смотрите в меню слева от строки ввода";
      bot.sendMessage(String(chat_id), mess, "");
      String sms = "Команды:\n";
      sms += "/options - пульт управления теплицей\n";
      sms += "/help - вызвать помощь\n";
      bot.sendMessage(chat_id, sms, "Markdown");
    }
    if ((text == "/help") || (text == "help"))
    {
      bot.sendMessage(chat_id, "За любой дополнительной информацией вы можете обращаться к автору бота \n \n@phoenix0757", "");
    }

   if (isUserExists(chat_id.toInt()))
   {
    // User_id присутствует в массиве
    // Действия, которые нужно выполнить, если user_id существует
    if (text.startsWith("/min ")) {
      String commandPrefix = "/min ";
      String sensorType = text.substring(commandPrefix.length(), text.indexOf(' ', commandPrefix.length()));
      float minValue = text.substring(text.indexOf(' ', commandPrefix.length()) + 1).toFloat();

      if (sensorType == "temperature") {
        if (minValue <= maxTemperature) {
          minTemperature = minValue;
          String response = "Минимальное значение температуры установлено на " + String(minTemperature) + " C";
          bot.sendMessage(chat_id, response, "");
        } else {
          String errorResponse = "Ошибка: Минимальное значение температуры (" + String(minValue) + " C) больше максимального значения (" + String(maxTemperature) + " C)";
          bot.sendMessage(chat_id, errorResponse, "");
        }
      } else if (sensorType == "humidity") {
        if (minValue <= maxHumidity) {
          minHumidity = minValue;
          String response = "Минимальное значение влажности установлено на " + String(minHumidity) + " %";
          bot.sendMessage(chat_id, response, "");
        } else {
          String errorResponse = "Ошибка: Минимальное значение влажности (" + String(minValue) + " %) больше максимального значения (" + String(maxHumidity) + " %)";
          bot.sendMessage(chat_id, errorResponse, "");
        }
      }
    }
    if (text.startsWith("/max ")) {
      String commandPrefix = "/max ";
      String sensorType = text.substring(commandPrefix.length(), text.indexOf(' ', commandPrefix.length()));
      float maxValue = text.substring(text.indexOf(' ', commandPrefix.length()) + 1).toFloat();

      if (sensorType == "temperature") {
        if (maxValue >= minTemperature) {
          maxTemperature = maxValue;
          String response = "Максимальное значение температуры установлено на " + String(maxTemperature) + " C";
          bot.sendMessage(chat_id, response, "");
        } else {
          String errorResponse = "Ошибка: Максимальное значение температуры (" + String(maxValue) + " C) меньше минимального значения (" + String(minTemperature) + " C)";
          bot.sendMessage(chat_id, errorResponse, "");
        }
      } else if (sensorType == "humidity") {
        if (maxValue >= minHumidity) {
          maxHumidity = maxValue;
          String response = "Максимальное значение влажности установлено на " + String(maxHumidity) + " %";
          bot.sendMessage(chat_id, response, "");
        } else {
          String errorResponse = "Ошибка: Максимальное значение влажности (" + String(maxValue) + " %) меньше минимального значения (" + String(minHumidity) + " %)";
          bot.sendMessage(chat_id, errorResponse, "");
        }
      }
    }

    if ((text == "/pumpon") || (text == "pumpon")) {
      if (!pumpBlocked) {
        digitalWrite(pump, HIGH);
        delay(500);
        digitalWrite(pump, LOW);
        bot.sendMessage(chat_id, "Насос включен на 0.5 сек", "");
      } else {
      bot.sendMessage(chat_id, "Влажность и так критически большая, я не могу выполнить вашу команду по соображениям безопасности", "");
    }
  }
	
    if ((text == "/pumpoff") || (text == "pumpoff"))
    {
      digitalWrite(pump, LOW);
      bot.sendMessage(chat_id, "Насос выключен", "");
    }
    if ((text == "/windon") || (text == "windon"))
    {
      digitalWrite(wind, HIGH);
      bot.sendMessage(chat_id, "Вентилятор включен", "");
    }
    if ((text == "/windoff") || (text == "windoff"))
    {
      digitalWrite(wind, LOW);
      bot.sendMessage(chat_id, "Вентилятор выключен", "");
    }
    if ((text == "/light") || (text == "light") || (text == "включить свет"))
    {
      fill_solid( leds, NUM_LEDS, CRGB(255, 255, 255));
      FastLED.show();
      bot.sendMessage(chat_id, "Свет включен", "");
    }
    if ((text == "/off") || (text == "off") || (text == "выключить свет"))
    {
      fill_solid( leds, NUM_LEDS, CRGB(0, 0, 0));
      FastLED.show();
      bot.sendMessage(chat_id, "Свет выключен", "");
    }
    if ((text == "/color_rand") || (text == "color_rand")|| (text == "случайный цвет"))
    {
      fill_solid( leds, NUM_LEDS, CRGB(random(0, 255), random(0, 255), random(0, 255)));
      FastLED.show();
      bot.sendMessage(chat_id, "Включен случайный цвет", "");
    }
    if (text == "/get_pass"){
      bot.sendMessage(chat_id, password, "");
    }
//
    if (text == "/options") // клавиатура для управления теплицей
    {
      String keyboardJson = "[[\"/light\", \"/off\"],[\"/color\",\"/sensors\"],[\"/pumpon\", \"/pumpoff\",\"/windon\", \"/windoff\"],[\"/open\",\"/close\"]]";
      bot.sendMessageWithReplyKeyboard(chat_id, "Выберите команду", "", keyboardJson, true);
      }

    if ((text == "/open") || (text == "открыть форточку"))
    {
      myservo.write(100);
      bot.sendMessage(chat_id, "форточка открыта", "");
    }
    if ((text == "/close") || (text == "закрыть форточку"))
    {
      myservo.write(0);
      bot.sendMessage(chat_id, "форточка закрыта", "");
    }
   if (text.startsWith("/color")&& text != "/color_rand") // обработка команды "/color {r} {g} {b}"
    {
      // Разбиваем сообщение на отдельные части по пробелам
      char* cmd = strdup(text.c_str());
      char* token = strtok(cmd, " ");
      int count = 0;
      int values[3];
      while (token != NULL && count < 3) {
        values[count] = atoi(token);
        token = strtok(NULL, " ");
        count++;
    }
      free(cmd);

      if (count == 3) // Проверяем, что получили 3 значения цвета
      {
        int r = values[0];
        int g = values[1];
        int b = values[2];

        // Проверяем диапазон значений цвета
        if (r >= 0 && r <= 255 && g >= 0 && g <= 255 && b >= 0 && b <= 255)
        {
          // Устанавливаем указанный цвет светодиодов
          fill_solid(leds, NUM_LEDS, CRGB(r, g, b));
          FastLED.show();
          bot.sendMessage(chat_id, "Установлен указанный цвет", "");
        }
        else
        {
          bot.sendMessage(chat_id, "Неверный диапазон значений цвета. Используйте значения от 0 до 255.", "");
        }
      }
      else
      {
        bot.sendMessage(chat_id, "Неверный формат команды. Используйте: /color {r} {g} {b}", "");
      }
   }

   if (text == "/get_user")
   {
    sendUserArray(chat_id.toInt());
    }
   if (text.startsWith("/command_name "))
   {
    String commandPrefix = "/command_name ";
    String name_command = text.substring(commandPrefix.length());
    String message = "супер, теперь название команды это " + name_command;
    bot.sendMessage(String(chat_id), message, ""); 
    }
   if (text.startsWith("/new_pass "))
   {
    String commandPrefix = "/new_pass ";
    String new_pass = text.substring(commandPrefix.length());
    password = new_pass;
    String message = "Cупер, теперь пароль на бота: " + new_pass;
    bot.sendMessage(chat_id, message, "");
    clearUsers();
    }
    if (text.startsWith("/user_log_out "))
   {
    String commandPrefix = "/user_log_out ";
    String log_out_id = text.substring(commandPrefix.length());
    removeUser(log_out_id.toInt());
    }
    if (text == "/log_out"){
      removeUser(chat_id.toInt());
      bot.sendMessage(chat_id, "Вы успешно вышли из своего аккаунта", "");
    }
	if (text == "/alina_ai"){
		String mess = String(alina_ai()).c_str(); 
		bot.sendMessage(String(chat_id), mess, "");
	}
	}else{
    bot.sendMessage(String(chat_id), "Вы не авторизованы, вам доступны лишь некоторые команды, для получения доступа ко всему функционалу авторизуйтесь \n(как это сделать вам расскажет @phoenix0757)", "");
  }
}
}

void loop() // вызываем функцию обработки сообщений через определенный период
{
  if (millis() - bot_lasttime > BOT_MTBS)
  {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    while (numNewMessages)
    {
      Serial.println("got response");
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
    checkSensorValues();
    unsigned long bot_lasttime = millis();  
  }
}
