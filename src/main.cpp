#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include "WiFi.h"
#include <Wire.h>
#include <Button2.h>
#include "esp_adc_cal.h"
#include <WiFiUdp.h>
#include "esp_wifi.h"

#ifndef TFT_DISPOFF
#define TFT_DISPOFF 0x28
#endif

#ifndef TFT_SLPIN
#define TFT_SLPIN 0x10
#endif

#define TFT_MOSI 19
#define TFT_SCLK 18
#define TFT_CS 5
#define TFT_DC 16
#define TFT_RST 23

#define TFT_BL 4	// Display backlight control pin
#define ADC_EN 14 //ADC_EN is the ADC detection enable port
#define ADC_PIN 34
#define BUTTON_1 35
#define BUTTON_2 0

#define MOTOR_1 13
#define MOTOR_2 15
#define MOTOR_3 2
#define MOTOR_4 17
#define MOTOR_5 22
#define MOTOR_6 21
#define MOTOR_7 27
#define MOTOR_8 26

const int motorFreq = 5000;
const int motorResolution = 8;
const int motorChannel1 = 0;
const int motorChannel2 = 1;
const int motorChannel3 = 2;
const int motorChannel4 = 3;
const int motorChannel5 = 4;
const int motorChannel6 = 5;
const int motorChannel7 = 6;
const int motorChannel8 = 7;

typedef struct s_data_task
{
	int duration;
	int pwm;
	int motor_id;
	TaskHandle_t thisTaskHandler;
} t_data_task;



int vref = 1100;
int timers_end[3] = {0, 0, 0};

#define BLACK 0x0000
#define WHITE 0xFFFF
TFT_eSPI tft = TFT_eSPI(); // Invoke custom library
Button2 btn1(BUTTON_1);
Button2 btn2(BUTTON_2);

const char *ssid = "Matrice";
const char *password = "QGmatrice";


WiFiUDP Udp;
unsigned int localUdpPort = 49141;
char incomingPacket[255];
String convertedPacket;

WiFiClass WiFiAP;

int pwmValues[8] = {0,0,0,0,0,0,0,0};
hw_timer_t *timers = NULL;

const char *wl_status_to_string(int ah)
{
	switch (ah)
	{
	case WL_NO_SHIELD:
		return "WL_NO_SHIELD";
	case WL_IDLE_STATUS:
		return "WL_IDLE_STATUS";
	case WL_NO_SSID_AVAIL:
		return "WL_NO_SSID_AVAIL";
	case WL_SCAN_COMPLETED:
		return "WL_SCAN_COMPLETED";
	case WL_CONNECTED:
		return "WL_CONNECTED";
	case WL_CONNECT_FAILED:
		return "WL_CONNECT_FAILED";
	case WL_CONNECTION_LOST:
		return "WL_CONNECTION_LOST";
	case WL_DISCONNECTED:
		return "WL_DISCONNECTED";
	default:
		return "ERROR NOT VALID WL";
	}
}


void button_init()
{
	btn1.setPressedHandler([](Button2 &b) {
		Serial.println("Bouton A pressed");

	});

	btn2.setPressedHandler([](Button2 &b) {
		Serial.println("Bouton B pressed");

	});
}

void button_loop()
{
	btn1.loop();
	btn2.loop();
}

void call_buttons(void)
{
	button_loop();
}

void sta_setup()
{
	WiFi.mode(WIFI_STA);
	WiFi.begin(ssid, password);
	tft.drawString("Connecting", tft.width() / 2, tft.height() / 2);
	uint64_t timeStamp = millis();

	ledcSetup(motorChannel1, motorFreq, motorResolution);
	ledcSetup(motorChannel2, motorFreq, motorResolution);
	ledcSetup(motorChannel3, motorFreq, motorResolution);
	ledcSetup(motorChannel4, motorFreq, motorResolution);
	ledcSetup(motorChannel5, motorFreq, motorResolution);
	ledcSetup(motorChannel6, motorFreq, motorResolution);
	ledcSetup(motorChannel7, motorFreq, motorResolution);
	ledcSetup(motorChannel8, motorFreq, motorResolution);

	ledcAttachPin(MOTOR_1, motorChannel1);
	ledcAttachPin(MOTOR_2, motorChannel2);
	ledcAttachPin(MOTOR_3, motorChannel3);
	ledcAttachPin(MOTOR_4, motorChannel4);
	ledcAttachPin(MOTOR_5, motorChannel5);
	ledcAttachPin(MOTOR_6, motorChannel6);
	ledcAttachPin(MOTOR_7, motorChannel7);
	ledcAttachPin(MOTOR_8, motorChannel8);

	Serial.println("Connecting");
	while (WiFi.status() != WL_CONNECTED)
	{
		if (millis() - timeStamp > 10000)
		{
			ESP.restart();
			tft.fillScreen(TFT_BLACK);
			tft.drawString("Restarting", tft.width() / 2, tft.height() / 2);
		}
		delay(500);
		Serial.println(wl_status_to_string(WiFi.status()));
		tft.fillScreen(TFT_BLACK);

		tft.drawString(wl_status_to_string(WiFi.status()), tft.width() / 2, tft.height() / 2);
	}
	Serial.print("Connected, IP address: ");
	Serial.println(WiFi.localIP());
	Udp.begin(localUdpPort);
	Serial.printf("Now listening at IP %s, UDP port %d\n", WiFi.localIP().toString().c_str(), localUdpPort);
}

void setup()
{
	// put your setup code here, to run once:
	Serial.begin(115200);
	WiFi.mode(WIFI_STA);
	WiFi.begin(ssid, password);

	// WiFiAP.begin(APssid, APpassword);

	timers = timerBegin(3, 80, true);
	timerAttachInterrupt(timers, &call_buttons, false);
	timerAlarmWrite(timers, 50 * 1000, true);
	timerAlarmEnable(timers);
	button_init();

	pinMode(ADC_EN, OUTPUT);
	digitalWrite(ADC_EN, HIGH);
	tft.init();
	tft.setRotation(0);
	tft.fillScreen(TFT_BLACK);

	if (TFT_BL > 0)
	{																					// TFT_BL has been set in the TFT_eSPI library in the User Setup file TTGO_T_Display.h
		pinMode(TFT_BL, OUTPUT);								// Set backlight pin to output mode
		digitalWrite(TFT_BL, TFT_BACKLIGHT_ON); // Turn backlight on. TFT_BACKLIGHT_ON has been set in the TFT_eSPI library in the User Setup file TTGO_T_Display.h
	}
	tft.setTextSize(1);
	tft.setTextColor(TFT_WHITE);
	tft.setCursor(0, 0);
	tft.setTextDatum(MC_DATUM);

	sta_setup();
}

void drawLoadingBar(TFT_eSprite *sprite, int x, int y, int amount)
{
	uint32_t color1 = TFT_GREEN;
	uint32_t color2 = TFT_WHITE;
	// uint32_t color3 = TFT_BLUE;
	// uint32_t color4 = TFT_RED;
	(*sprite).fillRect(x, y, map((int)(amount ), 0, 255, 0, 50), 10, color1);
	(*sprite).drawRect(x, y, 50, 10, color2);
}

void drawBatteryLevel(TFT_eSprite *sprite, int x, int y, float voltage)
{
	uint32_t color1 = TFT_GREEN;
	uint32_t color2 = TFT_WHITE;
	uint32_t color3 = TFT_BLUE;
	uint32_t color4 = TFT_RED;

	if (voltage > 4.33)
	{
		(*sprite).fillRect(x, y, 30, 10, color3);
	}
	else if (voltage > 3.2)
	{
		(*sprite).fillRect(x, y, map((long)(voltage * 100), 320, 430, 0, 30), 10, color1);
		(*sprite).setCursor(x + 7, y + 1);
		(*sprite).setTextColor(TFT_DARKGREY);
		(*sprite).printf("%02ld%%", map((long)(voltage * 100), 320, 432, 0, 100));
	}
	else
	{
		(*sprite).fillRect(x, y, 30, 10, color4);
	}

	(*sprite).drawRect(x, y, 30, 10, color2);
}

void drawMotorsActivity()
{
	TFT_eSprite drawing_sprite = TFT_eSprite(&tft);

	drawing_sprite.setColorDepth(8);
	drawing_sprite.createSprite(tft.width(), tft.height());

	drawing_sprite.fillSprite(TFT_BLACK);
	drawing_sprite.setTextSize(1);
	drawing_sprite.setTextFont(1);
	drawing_sprite.setTextColor(TFT_GREEN);
	drawing_sprite.setTextDatum(MC_DATUM);
	drawing_sprite.setCursor(0, 0);

	uint16_t v = analogRead(ADC_PIN);
	float battery_voltage = ((float)v / 4095.0) * 2.0 * 3.3 * (vref / 1000.0);
	drawing_sprite.setTextColor(TFT_RED);
	drawing_sprite.printf("Voltage : ");
	drawing_sprite.setTextColor(TFT_WHITE);
	drawing_sprite.printf("%.2fv\n\n", battery_voltage);

	drawing_sprite.setTextColor(TFT_RED);
	drawing_sprite.printf("Ssid : ");
	drawing_sprite.setTextColor(TFT_WHITE);
	drawing_sprite.printf("%s\n\n", ssid);

	drawing_sprite.setTextColor(TFT_RED);
	drawing_sprite.printf("Ip : ");
	drawing_sprite.setTextColor(TFT_WHITE);
	drawing_sprite.printf("%s\n\n", WiFi.localIP().toString().c_str());

	drawing_sprite.setTextColor(TFT_RED);
	drawing_sprite.printf("Udp port : ");
	drawing_sprite.setTextColor(TFT_WHITE);
	drawing_sprite.printf("%d\n\n", localUdpPort);

	drawing_sprite.setTextColor(TFT_RED);
	drawing_sprite.printf("Up time : ");
	drawing_sprite.setTextColor(TFT_WHITE);

	drawing_sprite.printf("%llds\n\n\n", esp_timer_get_time()/1000000);
	delay(100);
	drawBatteryLevel(&drawing_sprite, 100, 00, battery_voltage);

	for (int i = 0; i < 8; i++)
	{
		drawing_sprite.setCursor(0, 90 + i * 18);
		drawing_sprite.setTextColor(TFT_GREEN);
		drawing_sprite.printf("Motor %d : ", i);
		drawing_sprite.setTextColor(TFT_WHITE);
		drawing_sprite.printf("%d\n\n", pwmValues[i]);
		drawLoadingBar(&drawing_sprite, 80, 88 + i * 18, pwmValues[i]);
	}



	drawing_sprite.pushSprite(0, 0);
	drawing_sprite.deleteSprite();
}

void set_pwm(int pin, int pwm)
{
	ledcWrite(pin, pwm);
}

void look_for_udp_message()
{
	int packetSize = Udp.parsePacket();
	if (packetSize)
	{
		int len = Udp.read(incomingPacket, 255);
		if (len > 0)
		{
			incomingPacket[len] = 0;
		}
		Serial.printf("UDP packet contents: %s\n", incomingPacket);
		convertedPacket = String(incomingPacket);
		Serial.println(convertedPacket);
		Serial.printf("Debug indexof = P:%d, I:%d\n", convertedPacket.indexOf('P'), convertedPacket.indexOf("I"));

		if (convertedPacket.indexOf("P") > -1 && convertedPacket.indexOf("I") > -1)
		{
			int intensity = convertedPacket.substring(convertedPacket.indexOf("I") + 1).toInt();
			int pin = convertedPacket.substring(convertedPacket.indexOf("P") + 1).toInt();

			if (pin <= 8 && pin >= 0)
			{
				set_pwm(pin, intensity > 255 ? 255 : intensity);
				pwmValues[pin] = intensity > 255 ? 255 : intensity;
			}
		}
	}
}

void loop()
{
	look_for_udp_message();

	drawMotorsActivity();
	delay(25);
}