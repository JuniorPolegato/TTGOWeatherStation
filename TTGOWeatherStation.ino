#include <TFT_eSPI.h>        // Hardware-specific: user must select board in library
#include <ESP32Time.h>       // replace #include <NTPClient.h>  // https://github.com/taranais/NTPClient
#include <HTTPClient.h>
#include <ArduinoJson.h>     // https://github.com/bblanchon/ArduinoJson.git
#include <ESPAsyncWebServer.h>  // https://github.com/JuniorPolegato/ESPAsyncWebServer
#include <pins_arduino.h>
#include <esp_adc_cal.h>

#include "ESPUserConnection.h"
#include "fs_operations.h"
#include "animation.h"
#include "icons.h"
#include "Orbitron_Bold_14.h"
#include "Orbitron_Medium_20.h"

#define TFT_GREY 0x5AEB
#define TFT_LIGHTBLUE 0x01E9
#define TFT_DARKRED 0xA041
#define TFT_BLUE 0x5D9B

#define USE_STRPTIME

#define BUTTON_PIN_BITMASK(RTC_GPIO) (1ULL << RTC_GPIO)  // RTC GPIO: 0,2,4,12-15,25-27,32-39

RTC_DATA_ATTR int bootCount = 0;
RTC_DATA_ATTR int curCity = 0;
RTC_DATA_ATTR int loopCount = 0;
RTC_DATA_ATTR byte curBright = 1;

typedef struct {
    String city;
    String country;
} Cities;

Cities *cities = NULL;
size_t qtd_cities = 0;

// Initialization

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite spr = TFT_eSprite(&tft);
ESP32Time rtc(0);  // GMT by default

const int pwmFreq = 5000;
const int pwmResolution = 8;
const int pwmLedChannelTFT = 0;
const int backlight[5] = {10, 30, 60, 120, 220};

String endpoint;

// Variables for loop operation
String curDate = "";
String curTime = "";
String curSeconds = "";
String curTemperature = "";
String curHumidity = "";
int curTimezone = 0;
int press1 = 0;
int press2 = 0;
int frame = 0;
int x_spr = 0;
int dir_spr = 1;
String ipinfo_io;
char* footer = NULL;
char* footer_pos;
char* footer_end;
char  footer_30;
bool ap_mode = false;
int vref = 1100;
float full_scale = 3.3;
float adjust_usb = 0.18;
float adjust_bat = -0.14;
uint32_t switch_millis = 0;
uint32_t sleep_millis = 0;
uint32_t time_to_switch_city = 5;  // 5 seconds default
uint32_t time_to_sleep =  2;       // 2 minutes default
uint32_t time_to_wakeup = 1;       // 1 minutes default


void load_adjusts() {
    String file_data = readFile("/adjusts.txt");

    Serial.print("------------ Adjusts ------------ ");

    if (file_data.length()) {
        Serial.println("Ok");
        JsonDocument doc;
        deserializeJson(doc, file_data);
        if (!doc["adjust_usb"].isNull()) adjust_usb = doc["adjust_usb"];
        if (!doc["adjust_bat"].isNull()) adjust_bat = doc["adjust_bat"];
        if (!doc["time_to_switch_city"].isNull()) time_to_switch_city = doc["time_to_switch_city"];
        if (!doc["time_to_sleep"].isNull()) time_to_sleep = doc["time_to_sleep"];
        if (!doc["time_to_wakeup"].isNull()) time_to_wakeup = doc["time_to_wakeup"];
    }

    else {
        Serial.println("Failed");
        Serial.println(file_data);
    }

    Serial.print("adjust_usb.......:");
    Serial.println(adjust_usb);
    Serial.print("adjust_bat.......:");
    Serial.println(adjust_bat);
    Serial.print("time_to_switch_city: ");
    Serial.println(time_to_switch_city);
    Serial.print("time_to_sleep....:");
    Serial.println(time_to_sleep);
    Serial.print("time_to_wakeup...:");
    Serial.println(time_to_wakeup);
    Serial.println("---------------------------------");
}

void load_cities() {
    int i, d;

    String file_data = readFile("/cities.txt");
    if (file_data.length() < 6)
        file_data = "Ribeirão Preto\tBR\n";
    else if (!file_data.endsWith("\n"))
        file_data += '\n';

    qtd_cities = 0;
    i = 0;
    while ((i = file_data.indexOf('\n', ++i)) != -1)
        qtd_cities++;

    if (cities)
        free(cities);
    cities = (Cities*)calloc(qtd_cities, sizeof(Cities));

    Cities *c = cities;
    i = 0;
    for (;;) {
        d = file_data.indexOf('\t', i);
        if (d == -1) break;
        c->city = file_data.substring(i, d++);
        i = file_data.indexOf('\n', d);
        if (i == -1) break;
        c->country = file_data.substring(i++, d);
        Serial.println(c->city + " - " + c->country);
        c++;
    }
}

void update_city(bool swap = true, bool ip=false) {
    String text;
    if (swap && !ip && ++curCity == qtd_cities)
        curCity = 0;
    tft.fillRect(0, 60, 135, 27, TFT_BLACK);
    tft.setCursor(6, 82);
    tft.setFreeFont(&Orbitron_Medium_20);
    text = ip && !swap ? WiFi.localIP().toString() : cities[curCity].city;
    if (tft.textWidth(text) > tft.width() - 12)
        tft.setFreeFont(&Orbitron_Bold_14);
    tft.println(text);
    loopCount += ip ? 0 : 3000;  // force to getData for current city
}

// Customizations from ESPUserConnection.cpp
extern AsyncWebServer webserver;

extern void user_request_data(AsyncWebServerRequest *request, bool restart=true);

void custom_user_request_data(AsyncWebServerRequest *request) {
    int params = request->params();

    if (params >= 3) {
        String key = request->getParam(2U)->value();

        if (key.length() == 32)
            writeFile("/key.txt", key, false, true);

        else if (key == "delete")
            deleteFile("/key.txt");

        user_request_data(request);
    }
    else
        request->send(500);
}

void append_to_webserver() {
    webserver.on("/get_adjusts", HTTP_GET, [](AsyncWebServerRequest *request) {
        JsonDocument doc;
        String file_data;
        doc["adjust_usb"] = adjust_usb;
        doc["adjust_bat"] = adjust_bat;
        doc["time_to_switch_city"] = time_to_switch_city;
        doc["time_to_sleep"] = time_to_sleep;
        doc["time_to_wakeup"] = time_to_wakeup;
        serializeJson(doc, file_data);
        request->send(200, "application/json", file_data);
    });

    webserver.on("/store_adjusts", HTTP_POST, [](AsyncWebServerRequest *request) {
        int params = request->params();

        if (params == 5) {
            JsonDocument doc;
            String file_data;
            doc["adjust_usb"] = request->getParam(0U)->value();
            doc["adjust_bat"] = request->getParam(1U)->value();
            doc["time_to_switch_city"] = request->getParam(2U)->value();
            doc["time_to_sleep"] = request->getParam(3U)->value();
            doc["time_to_wakeup"] = request->getParam(4U)->value();
            serializeJson(doc, file_data);

            renameFile("/adjusts.txt", "/adjusts.txt.bak");
            if (writeFile("/adjusts.txt", file_data, false, true))
                deleteFile("/adjusts.txt.bak");
            else
                renameFile("/adjusts.txt.bak", "/adjusts.txt");

            request->send(200, "text/html", go_back_html);

            load_adjusts();
        }
        else
            request->send(500);
    });

    webserver.on("/list_cities", HTTP_GET, [](AsyncWebServerRequest *request) {
        int i = 0, d;
        String city, country;
        JsonDocument doc;
        String file_data = readFile("/cities.txt");

        doc[0]["city"] = "<strong>Click here to add</strong>";
        doc[0]["country"] = "BR";

        for (int n = 1; ; n++) {
            d = file_data.indexOf('\t', i);
            if (d == -1) break;
            city = file_data.substring(i, d++);
            i = file_data.indexOf('\n', d);
            if (i == -1) break;
            country = file_data.substring(d, i++);
            doc[n]["city"] = city;
            doc[n]["country"] = country;
        }

        serializeJson(doc, file_data);
        request->send(200, "application/json", file_data);
    });

    webserver.on("/config_cities", HTTP_POST, [](AsyncWebServerRequest *request) {
        int params = request->params();

        if (params == 3) {
            String city = request->getParam(0U)->value();
            String country = request->getParam(1U)->value();
            String operation = request->getParam(2U)->value();
            int i = -1, n;

            if (operation != "add" && operation != "delete") {
                request->send(500);
                return;
            }

            String file_data = readFile("/cities.txt");
            renameFile("/cities.txt", "/cities.txt.bak");

            String line = city + '\t' + country + '\n';
            i = file_data.indexOf(line);
            if (i > -1)  // delete the line
                file_data = file_data.substring(0, i) + file_data.substring(i + line.length());

            if (operation == "add")
                file_data = line + file_data;

            if (writeFile("/cities.txt", file_data, false, true))
                deleteFile("/cities.txt.bak");
            else
                renameFile("/cities.txt.bak", "/cities.txt");

            request->send(200, "text/html", go_back_html);

            load_cities();
            update_city(false);
        }
        else
            request->send(500);
    });

    webserver.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request) {
        String img = readFile("/favicon.ico", true);
        request->send(200, "image/png", (const uint8_t*)img.c_str(), img.length());
    });
}

void cal_vref() {
    esp_adc_cal_characteristics_t adc_chars;
    esp_adc_cal_value_t val_type = esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, vref, &adc_chars);  // Check type of calibration value used to characterize ADC

    Serial.printf("ADC number: %u\r\nADC attenuation: %u\r\nADC bit width: %u\r\nGradient of ADC-Voltage curve: %u\r\nOffset of ADC-Voltage curve: %u\r\n"
                  "Vref used by lookup table: %u\r\nPointer to low Vref curve of lookup table: %u\r\nPointer to high Vref curve of lookup table: %u\r\n"
                  "Version: %u\r\n",
                  adc_chars.adc_num, adc_chars.atten, adc_chars.bit_width, adc_chars.coeff_a, adc_chars.coeff_a, adc_chars.vref,
                  *adc_chars.low_curve, *adc_chars.high_curve, adc_chars.version);

    switch (adc_chars.atten) {
        case ADC_ATTEN_DB_0:
            Serial.println("No input attenumation, ADC can measure up to approx. 800 mV.");
            full_scale = 1.1;
            break;
        case ADC_ATTEN_DB_2_5:
            Serial.println("The input voltage of ADC will be attenuated extending the range of measurement by about 2.5 dB (1.33 x).");
            full_scale = 1.5;
            break;
        case ADC_ATTEN_DB_6:
            Serial.println("The input voltage of ADC will be attenuated extending the range of measurement by about 6 dB (2 x).");
            full_scale = 2.2;
            break;
        case ADC_ATTEN_DB_11:
        default:
            Serial.println("The input voltage of ADC will be attenuated extending the range of measurement by about 11 dB (3.55 x).");
            full_scale = 3.3;
            break;
    }
    Serial.printf("Full Scale: %.2fV\r\n", full_scale);

    if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
        Serial.printf("eFuse Vref: %u mV\r\n", adc_chars.vref);
        vref = adc_chars.vref;
    } else if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP) {
        Serial.printf("Two Point --> coeff_a: %umV coeff_b: %umV\r\n", adc_chars.coeff_a, adc_chars.coeff_b);
    } else {
        Serial.printf("Default Vref: %umV\r\n", vref);
    }
}

String get_vbat() {
    static unsigned long timeStamp = 0;
    static float battery_voltage = 0.0;

    if (millis() - timeStamp > 1000) {
        timeStamp = millis();
        uint16_t v = analogRead(VBAT);
        float current_voltage = ((float)v / 4095.0) * 2.0 * full_scale * (vref / 1000.0);
        current_voltage += current_voltage > 4.5 ? adjust_usb : adjust_bat;
        battery_voltage = battery_voltage < 0.1 ? current_voltage : (battery_voltage * 9.0 + current_voltage) / 10.0;
    }
    return String(battery_voltage) + "V";
}

void setup(void) {
    Serial.begin(115200);
    while (!Serial);
    print_wakeup_reason();

    pinMode(LEFT_BUTTON, INPUT_PULLUP);
    pinMode(RIGHT_BUTTON, INPUT_PULLUP);
    load_adjusts();
    cal_vref();

    tft.init();
    tft.setRotation(0);
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextFont(1);
    tft.setTextSize(1);

    spr.setColorDepth(16);
    spr.createSprite(50, 50);
    spr.setSwapBytes(true);

#if ESP_ARDUINO_VERSION < ESP_ARDUINO_VERSION_VAL(3, 0, 0)
    ledcSetup(pwmLedChannelTFT, pwmFreq, pwmResolution);
    ledcAttachPin(TFT_BL, pwmLedChannelTFT);
    ledcWrite(pwmLedChannelTFT, backlight[curBright]);
#else
    ledcAttach(TFT_BL, pwmFreq, pwmResolution);
    ledcWrite(TFT_BL, backlight[curBright]);
#endif

    // Wi-Fi connection

#ifdef OUTPUT_IS_TFT
    if (!connect_wifi(&tft, ap_mode)) {
#else
    if (!connect_wifi(ap_mode)) {
#endif  // OUTPUT_IS_TFT
        ap_mode = true;
        return;
    }

    append_to_webserver();

    // Layout

    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(1);
    tft.fillScreen(TFT_BLACK);
    tft.setSwapBytes(true);

    tft.setCursor(2, 232, 1);
    tft.println(WiFi.localIP());

    tft.setCursor(95, 204, 1);
    tft.println("BRIGHT");

    tft.setCursor(95, 152, 2);
    tft.println("SEC");

    tft.setTextColor(TFT_WHITE, TFT_LIGHTBLUE);

    tft.setCursor(4, 152, 2);
    tft.println("TEMP         ");

    tft.setCursor(4, 192, 2);
    tft.println("HUM          ");

    tft.setTextColor(TFT_WHITE, TFT_BLACK);

    tft.fillRect(90, 152, 1, 74, TFT_GREY);

    tft.setCursor(6, 82);
    tft.setFreeFont(&Orbitron_Medium_20);

    load_cities();
    if (tft.textWidth(cities[curCity].city) > tft.width() - 12)
        tft.setFreeFont(&Orbitron_Bold_14);
    tft.println(cities[curCity].city);

    for(int i = 0; i <= curBright; i++)
        tft.fillRect(93 + (i * 7), 216, 3, 10, TFT_BLUE);

    String key = readFile("/key.txt").substring(0, 32);
    // Serial.println("[" + key + "]");
    endpoint = "http://api.openweathermap.org/data/2.5/weather?units=metric&APPID=" + key + "&q=";

    String api_error;
    for (;;) {
        api_error.clear();

        if (!getLocalInfo())
            api_error = "IP Info";

        if (!getData())
            if (api_error.length())
                api_error += "\nOpen Weather";
            else
                api_error = "Open Weather";

        if (!api_error.length()) break;

        key.clear();
        tft.fillRect(0, 0, tft.getViewportWidth(), tft.getViewportHeight(), TFT_DARKRED);
        tft.setFreeFont(&Orbitron_Bold_14);
        tft.setTextColor(TFT_WHITE);
        tft.setCursor(0, 20);
        api_error = "Error!\n\n"
                    + api_error + "\n\n"
                    "Please, check:\n"
                    "- internet\n"
                    "- cities\n"
                    "- countries\n"
                    "- OpenWeather\n"
                    "                    key\n\n\n";
        tft.println(api_error);
        tft.println(WiFi.localIP());
        Serial.println(api_error);
        Serial.println(WiFi.localIP());
        tft.print("10 s"); tft.flush();
        Serial.print("10 s"); Serial.flush();
        for (int i=0; i < 10; i++) {
            tft.print('.'); tft.flush();
            Serial.print('.'); Serial.flush();
            delay(1000);
        }
    }
    if (!key.length())
        setup();

    switch_millis = millis();
    sleep_millis = switch_millis;
}

void loop() {
    if (ap_mode) {
        if (WiFi.softAPgetStationNum() == 0) {

#ifdef OUTPUT_IS_TFT
            tft.print('.'); tft.flush();
#else
            Serial.print('.'); Serial.flush();
#endif  // OUTPUT_IS_TFT

            delay(1000);
            return;
        }

#ifdef OUTPUT_IS_TFT
        tft.println("\nConnected!\n");
#else
        Serial.println("\r\nConnected!\r\n");
#endif  // OUTPUT_IS_TFT

        vTaskDelete(NULL);  // Stop loop task, run just WebServer in AP mode
        return;
    }

    tft.pushImage(0, 88,  135, 65, animation[frame++]);
    if (frame == frames) frame = 0;

    spr.pushSprite(x_spr, 95, 0x94b2);
    x_spr += dir_spr;
    if (x_spr == 85) dir_spr = -1;
    else if (x_spr == 0) dir_spr = 1;

    if (digitalRead(RIGHT_BUTTON) == 0) {
        sleep_millis = millis();
        if (press2 < 5)
            press2++;
        else if (press2 == 5) {  // turn off back light of TFT
            press2++;
#if ESP_ARDUINO_VERSION < ESP_ARDUINO_VERSION_VAL(3, 0, 0)
            ledcWrite(pwmLedChannelTFT, 0);
#else
            ledcWrite(TFT_BL, 0);
#endif
        }
    }
    else if (press2 > 0) {
        if (press2 < 5) {
            tft.fillRect(93, 216, 44, 12, TFT_BLACK);
            if(++curBright == 5) curBright = 0;
            for(int i = 0; i <= curBright; i++)
                tft.fillRect(93 + (i * 7), 216, 3, 10, TFT_BLUE);
#if ESP_ARDUINO_VERSION < ESP_ARDUINO_VERSION_VAL(3, 0, 0)
            ledcWrite(pwmLedChannelTFT, backlight[curBright]);
#else
            ledcWrite(TFT_BL, backlight[curBright]);
#endif
        }
        else
            goto_sleep();
        press2 = 0;
    }

    if (digitalRead(LEFT_BUTTON) == 0) {
        sleep_millis = millis();
        if (press1 < 5)
            press1++;
        else if (press1 == 5) {
            update_city(false, true);  // show IP
            press1++;
        }
    }
    else if (press1 > 0) {
        if (press1 < 5)
            update_city();  // swap city
        else
            update_city(true, true);  // swap IP by city
        press1 = 0;
    }

    if (++loopCount > 3000) {  /// about 5 minutes
        while (!getData())
            delay(1000);
        loopCount = 0;
    }

    tft.setCursor(2, 232, 1);
    footer_pos += *footer_pos > 127;
    if (++footer_pos == footer_end)
        footer_pos = footer;
    footer_30 = *(footer_pos + 30);
    *(footer_pos + 30) = '\0';
    tft.println(get_vbat() + " " + footer_pos);
    // Serial.println(get_vbat() + " " + footer_pos);
    *(footer_pos + 30) = footer_30;

    String _date = rtc.getTime("%a, %d %b %Y ");
    String _time = rtc.getTime("%R");
    String _seconds = rtc.getTime("%S");

    if (curDate != _date) {
        curDate = _date;
        tft.fillRect(0, 44, 135, 18, TFT_BLACK);
        tft.setTextColor(TFT_ORANGE, TFT_BLACK);
        tft.setTextFont(2);
        tft.setCursor(6, 44);
        tft.println(curDate);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
    }

    if (curTime != _time) {
        curTime = _time;
        tft.setFreeFont(&Orbitron_Light_32);
        tft.fillRect(0, 0, 135, 40, TFT_BLACK);
        tft.setCursor(5, 40);
        tft.print(curTime);
        tft.setTextColor(TFT_ORANGE, TFT_BLACK);
        tft.setCursor(110, 0);
        tft.setTextFont(2);
        int _timezone = int(curTimezone / 3600);
        tft.print(_timezone < 0 ? (_timezone > -10 ? "-0" : "-") : (_timezone < 10 ? "+0" : "+"));
        tft.println(abs(_timezone));
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
    }

    if (curSeconds != _seconds) {
        curSeconds = _seconds;
        tft.fillRect(93, 170, 48, 28, TFT_DARKRED);
        tft.setFreeFont(&Orbitron_Medium_20);
        tft.setCursor(96, 192);
        tft.println(curSeconds);
    }

    if (time_to_switch_city > 0 && millis() - switch_millis > time_to_switch_city * 1000)
        update_city();

    if (time_to_sleep > 0 && millis() - sleep_millis > time_to_sleep * 60000)
        goto_sleep();

    delay(200);
}

bool getData() {
    String icon = "";
    if (WiFi.status() == WL_CONNECTED) {  // Check the current connection status
        HTTPClient http;
        int httpCode;

        String city(cities[curCity].city);
        city.replace(" ", "%20");
        String _endpoint = endpoint + city + "," + cities[curCity].country;
        // Serial.println("[" + _endpoint + "]");
        http.begin(_endpoint);
        const char *headerKeys[] = {"Date"};
        const size_t headerKeysCount = sizeof(headerKeys) / sizeof(headerKeys[0]);
        http.collectHeaders(headerKeys, headerKeysCount);
        httpCode = http.GET();  // Make the request

        if (httpCode == 200) {  // Check for the returning code
            String payload = http.getString();
            JsonDocument doc;
            deserializeJson(doc, payload);

            String _lon = doc["coord"]["lon"];
            String _lat = doc["coord"]["lat"];
            String _description = doc["weather"][0]["description"];
            String _icon = doc["weather"][0]["icon"];
            String _temp = doc["main"]["temp"];
            String _feels_like = doc["main"]["feels_like"];
            String _temp_min = doc["main"]["temp_min"];
            String _temp_max = doc["main"]["temp_max"];
            String _pressure = doc["main"]["pressure"];
            String _humidity = doc["main"]["humidity"];
            String _visibility = doc["visibility"];
            String _wind_speed = doc["wind"]["speed"];
            String _wind_degree = doc["wind"]["deg"];
            String _clouds = doc["clouds"]["all"];
            String _dt_capture = doc["dt"];
            String _sunrise = doc["sys"]["sunrise"];
            String _sunset = doc["sys"]["sunset"];
            String _timezone = doc["timezone"];

            curTimezone = atoi(_timezone.c_str());
            setRtcTime(http.header("Date"));

            icon = _icon;

            if (curTemperature != _temp) {
                curTemperature = _temp;
                tft.setFreeFont(&Orbitron_Medium_20);
                tft.fillRect(0, 173, 89, 14, TFT_BLACK);
                tft.setCursor(2, 187);
                _temp = curTemperature.substring(0, 4);
                if (_temp.length() == 4 and _temp[3] == '.') {
                    if (_temp[0] == '-' and _temp[1] == '1')
                        _temp = curTemperature.substring(0, 5);
                    else
                        _temp = curTemperature.substring(0, 3);
                }
                tft.println(_temp + "°C");

                spr.pushImage(0, 0, 50, 50, icons[icons_map(_icon)]);
            }

            if (curHumidity != _humidity) {
                curHumidity = _humidity;
                tft.setFreeFont(&Orbitron_Medium_20);
                tft.fillRect(0, 213, 89, 14, TFT_BLACK);
                tft.setCursor(2, 227);
                tft.println(curHumidity + "%");
            }

            char buffer[6];
            time_t _time;

            String _name = String("   ") + cities[curCity].city;

            _time = atoi(_dt_capture.c_str()) + curTimezone;
            strftime(buffer, 6, "%H:%M", gmtime(&_time));
            _dt_capture = buffer;

            _time = atoi(_sunrise.c_str()) + curTimezone;
            strftime(buffer, 6, "%H:%M", gmtime(&_time));
            _sunrise = buffer;

            _time = atoi(_sunset.c_str()) + curTimezone;
            strftime(buffer, 6, "%H:%M", gmtime(&_time));
            _sunset = buffer;

            updateFooter(_name + " weather: " + _description +
                         _name + " feels like: " + _feels_like + "°C" +
                         _name + " pressure: " + _pressure + "hPa" +
                         _name + " location: " + _lon + "," + _lat +
                         _name + " visibility: " + (atoi(_visibility.c_str()) / 1000.0) + "km" +
                         _name + " wind speed: " + _wind_speed + "m/s" +
                         _name + " wind degree: " + _wind_degree + "°" +
                         _name + " clouds: " + _clouds + "%" +
                         _name + " capture at: " + _dt_capture +
                         _name + " sunrise: " + _sunrise +
                         _name + " sunset: " + _sunset +
                         "   Weather from https://openweathermap.org/");

        }
        else {
            Serial.println("Error on\r\nHTTP OpenWeather\r\nrequest [" + String(httpCode) + "]");
            return false;
        }

        http.end(); //Free the resources
    }
    else {
        Serial.println("No internet connection!");
        return false;
    }

    Serial.println("_____________________________________________________________\r\n");
    Serial.println("City: [" + String(curCity) + "] " + cities[curCity].city + ", " + cities[curCity].country);
    Serial.println("Date and time: " + rtc.getDateTime());
    Serial.println("Temperature: " + curTemperature);
    Serial.println("Humidity: " + curHumidity);
    Serial.println("Icon: " + icon);
    Serial.println("_____________________________________________________________\r\n");
    switch_millis = millis();
    Serial.print("Waiting " + String(time_to_switch_city) + " seconds...");
    Serial.println("  |  About " + String(time_to_sleep - (millis() - sleep_millis) / 60000.) + " minutes to sleep...");
    return true;
}

#ifndef USE_STRPTIME
int numberOfMonth(const char* month) {
    String m3 = String((char) tolower(*month)) + (char) tolower(month[1]) + (char) tolower(month[2]);
    if (m3 == "jan") return 1;
    if (m3 == "fer") return 2;
    if (m3 == "mar") return 3;
    if (m3 == "apr") return 4;
    if (m3 == "may") return 5;
    if (m3 == "jun") return 6;
    if (m3 == "jul") return 7;
    if (m3 == "aug") return 8;
    if (m3 == "sep") return 9;
    if (m3 == "oct") return 10;
    if (m3 == "nov") return 11;
    if (m3 == "dec") return 12;
    return 0;
}
#endif // !USE_STRPTIME

void setRtcTime(String date_string) {
#ifdef USE_STRPTIME
    struct tm t;
    strptime(date_string.c_str(), "%a, %d %b %Y %H:%M:%S GMT", &t);
    rtc.setTime(mktime(&t) + curTimezone);
#else
    int day, month, year, hours, minutes, seconds;
    const char* c = date_string.c_str();
    while (*c++ != ' '); c--; while (*c++ == ' '); c--; day = atoi(c);
    while (*c++ != ' '); c--; while (*c++ == ' '); c--; month = numberOfMonth(c);
    while (*c++ != ' '); c--; while (*c++ == ' '); c--; year = atoi(c);
    while (*c++ != ' '); c--; while (*c++ == ' '); c--; hours = atoi(c);
    while (*c++ != ':'); minutes = atoi(c);
    while (*c++ != ':'); seconds = atoi(c);
    rtc.setTime(seconds, minutes, hours, day, month, year);
    rtc.setTime(rtc.getEpoch() + curTimezone);
#endif // USE_STRPTIME
}

bool getLocalInfo() {
    String icon = "";
    if (WiFi.status() == WL_CONNECTED) {  // Check the current connection status
        HTTPClient http;
        int httpCode;

        http.begin("http://ipinfo.io/");
        http.addHeader("Accept", "application/json");
        httpCode = http.GET();  // Make the request

        if (httpCode == 200) {  // Check for the returning code
            String payload = http.getString();
            StaticJsonDocument<1000> doc;
            deserializeJson(doc, payload.c_str());

            String _ip = doc["ip"];
            String _city = doc["city"];
            String _region = doc["region"];
            String _country = doc["country"];
            String _loc = doc["loc"];
            String _org = doc["org"];
            String _postal = doc["postal"];
            String _tz = doc["timezone"];

            ipinfo_io = "   Wi-Fi IP: " + WiFi.localIP().toString() +
                        "   Internet IP: " + _ip +
                        "   Local city: " + _city +
                        "   Local state: " + _region +
                        "   Local country: " + _country +
                        "   Local position: " + _loc +
                        "   Local Postal: " + _postal +
                        "   Local Timezone: " + _tz +
                        "   Local from http://ipinfo.io/   ";
        }
        else {
            Serial.println("Error on HTTP request [" + String(httpCode) + "]");
            return false;
        }

        http.end(); //Free the resources
    }
    else {
        Serial.println("No internet connection!");
        return false;
    }
    return true;
}

void updateFooter(String extraInfo) {
    String _footer = extraInfo + ipinfo_io;
    int _length = _footer.length();
    _footer += _footer.substring(0, 30);

    if (footer) free(footer);
    footer = (char*) malloc(_footer.length() + 1);

    strcpy(footer, _footer.c_str());
    footer_end = footer + _length ;
    footer_pos = footer;
}

void print_wakeup_reason() {
    esp_sleep_wakeup_cause_t wakeup_reason;

    Serial.println("Boot number: " + String(++bootCount));

    wakeup_reason = esp_sleep_get_wakeup_cause();

    switch(wakeup_reason) {
        case ESP_SLEEP_WAKEUP_EXT0:
            Serial.println("Wakeup caused by external signal using RTC_IO");
            break;
        case ESP_SLEEP_WAKEUP_EXT1:
            Serial.println("Wakeup caused by external signal using RTC_CNTL");
            break;
        case ESP_SLEEP_WAKEUP_TIMER:
            Serial.println("Wakeup caused by timer");
            break;
        case ESP_SLEEP_WAKEUP_TOUCHPAD:
            Serial.println("Wakeup caused by touchpad");
            break;
        case ESP_SLEEP_WAKEUP_ULP:
            Serial.println("Wakeup caused by ULP program");
            break;
        default:
            Serial.println("Wakeup was not caused by deep sleep: " + String(wakeup_reason));
            break;
    }
}

void goto_sleep() {
    Serial.println("I am going to sleep!");

    if (time_to_wakeup > 0) {
        Serial.println("I will wake up in " + String(time_to_wakeup) + " minutes...");
        esp_sleep_enable_timer_wakeup(time_to_wakeup * 60000000);
    }

    Serial.println("Press right button to wake me up...");
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_35, 0);

    esp_deep_sleep_start();
}
