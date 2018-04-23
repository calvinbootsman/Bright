/*
 Name:		Bright.ino
 Created:	4/7/2018 11:28:45 AM
 Author:	calvi
*/

#include <EEPROM.h>
#include <stdio.h>
#include <stdlib.h>

#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiUdp.h>

#include <AzureIoTHub.h>
#include <AzureIoTProtocol_MQTT.h>
#include <AzureIoTUtility.h>

#include "config.h"

static bool messagePending = false;
static bool messageSending = true;

static char *connectionString;
static char *ssid;
static char *pass;

static int interval = INTERVAL;

void blinkLED()
{
	digitalWrite(LED_BUILTIN, HIGH);
	delay(500);
	digitalWrite(LED_BUILTIN, LOW);
}

void initWifi()
{
	// Attempt to connect to Wifi network:
	Serial.printf("Attempting to connect to SSID: %s.\r\n", ssid);

	// Connect to WPA/WPA2 network. Change this line if using open or WEP network:
	WiFi.begin(ssid, pass);
	while (WiFi.status() != WL_CONNECTED)
	{
		// Get Mac Address and show it.
		// WiFi.macAddress(mac) save the mac address into a six length array, but the endian may be different. The huzzah board should
		// start from mac[0] to mac[5], but some other kinds of board run in the oppsite direction.
		uint8_t mac[6];
		WiFi.macAddress(mac);
		Serial.printf("You device with MAC address %02x:%02x:%02x:%02x:%02x:%02x connects to %s failed! Waiting 10 seconds to retry.\r\n",
			mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], ssid);
		WiFi.begin(ssid, pass);
		delay(10000);
	}
	Serial.printf("Connected to wifi %s.\r\n", ssid);
}

void initTime()
{
	time_t epochTime;
	configTime(0, 0, "pool.ntp.org", "time.nist.gov");

	while (true)
	{
		epochTime = time(NULL);

		if (epochTime == 0)
		{
			Serial.println("Fetching NTP epoch time failed! Waiting 2 seconds to retry.");
			delay(2000);
		}
		else
		{
			Serial.printf("Fetched NTP epoch time is: %lu.\r\n", epochTime);
			break;
		}
	}
}

static IOTHUB_CLIENT_LL_HANDLE iotHubClientHandle;
void setup() {
	//initSerial();
	Serial.begin(115200);
	pinMode(GPIO2, INPUT);
	while (digitalRead(2) == HIGH) {
		delay(100);
		while (digitalRead(2) != LOW) {
			delay(10);
		}
		Serial.println("Serial is Working");
	}
	delay(2000);
	//digitalWrite(LED_BUILTIN, LOW);
	readCredentials();
	delay(2000);
	Serial.println("Credentials received");
	//digitalWrite(LED_BUILTIN, HIGH);
	initWifi();
	Serial.println("WiFi Inititalized");
	delay(2000);
	initTime();
	Serial.println("Time Inititalized");
	delay(2000);

	iotHubClientHandle = IoTHubClient_LL_CreateFromConnectionString(connectionString, MQTT_Protocol);
	if (iotHubClientHandle == NULL)
	{
		Serial.println("Failed on IoTHubClient_CreateFromConnectionString.");
		while (1);
	}

	IoTHubClient_LL_SetMessageCallback(iotHubClientHandle, receiveMessageCallback, NULL);
	IoTHubClient_LL_SetDeviceMethodCallback(iotHubClientHandle, deviceMethodCallback, NULL);
	//IoTHubClient_LL_SetDeviceTwinCallback(iotHubClientHandle, twinCallback, NULL);
}

static int messageCount = 1;
void loop() {
	if (digitalRead(GPIO2) == HIGH) {
		//digitalWrite(LED_BUILTIN, HIGH);
		delay(100);

		if (!messagePending && messageSending)
		{
			char messagePayload[MESSAGE_MAX_LEN];
			sendMessage(iotHubClientHandle, messagePayload, true);
			messageCount++;
			delay(interval);
		}
		IoTHubClient_LL_DoWork(iotHubClientHandle);
		delay(20);

		while (digitalRead(GPIO2) == HIGH) { delay(1); }
	}
	digitalWrite(LED_BUILTIN, LOW);
}





/*
#include <stdio.h>
#include <stdlib.h>

#include "iothub_client.h"
#include "iothub_message.h"
#include "iothubtransportmqtt.h"
#include "azure_c_shared_utility/threadapi.h"
#include "azure_c_shared_utility/crt_abstractions.h"
#include "azure_c_shared_utility/platform.h"
#include "azure_c_shared_utility/shared_util_options.h"
#include "iothub_client_options.h"

#define GPIO2 2

static void send_confirm_callback(IOTHUB_CLIENT_CONFIRMATION_RESULT , void*);

static const char* connectionString = "[device connection string]";
static size_t g_message_count_send_confirmations = 0;

IOTHUB_CLIENT_TRANSPORT_PROVIDER protocol;
IOTHUB_MESSAGE_HANDLE message_handle;
size_t messages_sent = 0;
const char* telemetry_msg = "test_message";
IOTHUB_CLIENT_LL_HANDLE iothub_ll_handle;


void setup() {
	pinMode(LED_BUILTIN, OUTPUT);
	pinMode(GPIO2, INPUT);

	if ((IoTHubClient_LL_CreateFromConnectionString(connectionString, MQTT_Protocol)) == NULL)
	{
		(void)printf("ERROR: iotHubClientHandle is NULL!\r\n");
	}
	else
	{
		// Used to initialize IoTHub SDK subsystem
		(void)platform_init();

		(void)printf("Creating IoTHub handle\r\n");
		// Create the iothub handle here
		iothub_ll_handle = IoTHubClient_LL_CreateFromConnectionString(connectionString, protocol);

	}
}

// the loop function runs over and over again until power down or reset
void loop() {
	if (digitalRead(GPIO2) == HIGH) {
		digitalWrite(LED_BUILTIN, HIGH);


		// Construct the iothub message from a string or a byte array
		message_handle = IoTHubMessage_CreateFromString(telemetry_msg);

		// Set message property
		(void)IoTHubMessage_SetMessageId(message_handle, "MSG_ID");
		(void)IoTHubMessage_SetCorrelationId(message_handle, "CORE_ID");
		(void)IoTHubMessage_SetContentTypeSystemProperty(message_handle, "application%2Fjson");
		(void)IoTHubMessage_SetContentEncodingSystemProperty(message_handle, "utf-8");

		MAP_HANDLE propMap = IoTHubMessage_Properties(message_handle);
		Map_AddOrUpdate(propMap, "property_key", "property_value");

		(void)printf("Sending message %d to IoTHub\r\n", (int)(messages_sent + 1));
		IoTHubClient_LL_SendEventAsync(iothub_ll_handle, message_handle, send_confirm_callback, NULL);

		// The message is copied to the sdk so the we can destroy it
		IoTHubMessage_Destroy(message_handle);

		messages_sent++;
	}
	else {
		digitalWrite(LED_BUILTIN, LOW);
	}
}

static void send_confirm_callback(IOTHUB_CLIENT_CONFIRMATION_RESULT result, void* userContextCallback)
{
	(void)userContextCallback;
	// When a message is sent this callback will get envoked
	g_message_count_send_confirmations++;
	(void)printf("Confirmation callback received for message %zu with result %s\r\n", g_message_count_send_confirmations, ENUM_TO_STRING(IOTHUB_CLIENT_CONFIRMATION_RESULT, result));
}
*/