/*
#include "mbed.h"
#include "MQTTmbed.h"
#include "MQTTClient.h"
#include "MQTTNetwork.h"
#include "DHT11/Dht11.h"
#include <map>

Thread thread_messages;
Thread thread_timer_countdown;

// Dht11 sensor(D6);

WiFiInterface *wifi;

#define MQTTCLIENT_QOS2 1

MQTT::Client<MQTTNetwork, Countdown> *client_ptr;

char *timer_

topic = "Dan2l0s/feeds/smarttimer.timer";
// char *temperature_topic = "Dan2l0s/groups/SmartTimer/Temperature";
char *temperature_topic = "Dan2l0s/feeds/smarttimer.temperature";
char *timer_status_topic = "Dan2l0s/feeds/smarttimer.timerstatus";
char *timer_settings_topic = "Dan2l0s/feeds/smarttimer.timersettings";
char *temperature_settings_topic = "Dan2l0s/feeds/smarttimer.temperaturesettings";

bool connection = false;

bool timer_status = false;
bool timer_error = false;
int timer_left = 0;
int target_temp = 0;

void post_message(char *topic, char *msg);

char *get_timer_left()
{
    char ans[20];
    int hours = timer_left / 3600;
    int minutes = (timer_left % 3600) / 60;
    int seconds = timer_left % 60;
    printf("Hours %d minutes %d seconds\n\r", hours, minutes, seconds);
    sprintf(ans, "%02d:%02d:%02d", hours, minutes, seconds);
    printf("ans: %s\n\r", ans);
    char *msg = ans;
    return msg;
}

void countdown()
{
    printf("Started countdown!\n\r");
    char *message;
    timer_status = true;
    while (timer_left > 0)
    {
        if (!timer_error)
        {
            message = get_timer_left();
            printf("message in countdown: %s .\n\r", message);
            post_message(timer_topic, message);
            timer_left -= 5;
        }
        thread_sleep_for(5000);
    }
    timer_status = false;
    message = "0";
    timer_left = 0;
    post_message(timer_status_topic, message);
}

int str2_to_int(char *str)
{
    return ((str[0] - '0') * 10) + (str[1] - '0');
}

int str2_to_int(char s0, char s1)
{
    return ((s0 - '0') * 10) + (s1 - '0');
}

void timer_settings_listener(MQTT::MessageData &md)
{
    MQTT::Message &message = md.message;
    char *content = (char *)message.payload;

    printf("Message arrived: qos %d, retained %d, dup %d, packetid %d\r\n", message.qos, message.retained, message.dup, message.id);
    printf("Message: %.*s :\r\n", message.payloadlen, content);

    int hours = str2_to_int(content[0], content[1]);
    int minutes = str2_to_int(content[3], content[4]);
    int seconds = str2_to_int(content[6], content[7]);
    printf("Hours: %d minutes: %d seconds %d\n\r", hours, minutes, seconds);
    timer_left = hours * 3600 + minutes * 60 + seconds;
    printf("Timer left: %d\r\n", timer_left);
    post_message(timer_topic, content);
}

void temperature_settings_listener(MQTT::MessageData &md)
{
    MQTT::Message &message = md.message;
    char *content = (char *)message.payload;

    printf("Message arrived: qos %d, retained %d, dup %d, packetid %d\r\n", message.qos, message.retained, message.dup, message.id);
    printf("Message: %.*s :\r\n", message.payloadlen, content);

    int temp = str2_to_int(content);

    target_temp = temp;
    printf("Target temp = %d\n\r", temp);
}

void timer_status_listener(MQTT::MessageData &md)
{
    MQTT::Message &message = md.message;
    char *content = (char *)message.payload;

    printf("Message arrived: qos %d, retained %d, dup %d, packetid %d\r\n", message.qos, message.retained, message.dup, message.id);
    printf("Message: %.*s :\r\n", message.payloadlen, content);

    printf(". %s ., %d , %d .\r\n", content, timer_left, (int)timer_status);
    if (content[0] == '1' && timer_left > 0 && !timer_status)
    {
        countdown();
    }
    if (content[0] == '0' && timer_left > 0 && timer_status)
    {
        timer_left = 0;
        timer_status = false;
    }
}

void temperature_handler()
{
    while (!connection)
    {
        thread_sleep_for(1000);
    }
    char msg[10];
    int temperature;
    while (true)
    {
        // sensor.read();
        // temperature = sensor.getCelsius();
        temperature = 30;
        sprintf(msg, "%d", temperature);
        // printf("Temperature: %s\r\n", msg);
        post_message(temperature_topic, msg);
        if ((temperature - target_temp > 2))
        {
            if (timer_status)
            {
                timer_error = true;
                char *mesg = "0";
                post_message(timer_status_topic, mesg);
            }
        }
        else
        {
            if (timer_error)
            {
                timer_error = false;
                char *mesg = "1";
                post_message(timer_status_topic, mesg);
            }
        }
        thread_sleep_for(5000);
    }
}

void post_message(char *topic, char *msg)
{
    MQTT::Message message;
    // QoS 0
    char buf[100];
    sprintf(buf, msg, "\r\n");
    message.qos = MQTT::QOS0;
    message.retained = false;
    message.dup = false;
    message.payload = (void *)buf;
    message.payloadlen = strlen(buf) + 1;
    (*client_ptr).publish(topic, message);
    printf("Posted message: %s\r\n", msg);
}

void mqtt_demo(NetworkInterface *net)
{
    float version = 0.6;
    MQTTNetwork network(net);
    MQTT::Client<MQTTNetwork, Countdown> client = MQTT::Client<MQTTNetwork, Countdown>(network);
    client_ptr = &client;
    char *hostname = "52.54.110.50";
    int port = 1883;

    printf("Connecting to %s:%d\r\n", hostname, port);

    int rc = network.connect(hostname, port);

    if (rc != 0)
        printf("rc from TCP connect is %d\r\n", rc);
    else
        printf("Connected socket\n\r");

    MQTTPacket_connectData data = MQTTPacket_connectData_initializer;

    data.MQTTVersion = 3;
    data.clientID.cstring = "2a617592b917";
    data.username.cstring = "Dan2l0s";
    data.password.cstring = "aio_MXDF78jlrmMFSUoQcHPtLt93Zddx";

    if ((rc = client.connect(data)) != 0)
        printf("rc from MQTT connect is %d\r\n", rc);

    if ((rc = client.subscribe(timer_settings_topic, MQTT::QOS0, timer_settings_listener)) != 0)
        printf("rc from MQTT subscribe1 is %d\r\n", rc);

    if ((rc = client.subscribe(temperature_settings_topic, MQTT::QOS0, temperature_settings_listener)) != 0)
        printf("rc from MQTT subscribe2 is %d\r\n", rc);

    if ((rc = client.subscribe(timer_status_topic, MQTT::QOS0, timer_status_listener)) != 0)
        printf("rc from MQTT subscribe3 is %d\r\n", rc);

    printf("connection = true\n\r");
    connection = true;
    while (true)
    {
        (*client_ptr).yield(100);
    }

    return;
}

const char *sec2str(nsapi_security_t sec)
{
    switch (sec)
    {
    case NSAPI_SECURITY_NONE:
        return "None";
    case NSAPI_SECURITY_WEP:
        return "WEP";
    case NSAPI_SECURITY_WPA:
        return "WPA";
    case NSAPI_SECURITY_WPA2:
        return "WPA2";
    case NSAPI_SECURITY_WPA_WPA2:
        return "WPA/WPA2";
    case NSAPI_SECURITY_UNKNOWN:
    default:
        return "Unknown";
    }
}

DigitalIn button(USER_BUTTON);

int main()
{
    printf("\nWiFi example\n");

    wifi = WiFiInterface::get_default_instance();
    if (!wifi)
    {
        printf("ERROR: No WiFiInterface found.\n");
        return -1;
    }

    printf("\nConnecting to %s...\n", MBED_CONF_APP_WIFI_SSID);
    int ret = wifi->connect(MBED_CONF_APP_WIFI_SSID, MBED_CONF_APP_WIFI_PASSWORD, NSAPI_SECURITY_WPA_WPA2);
    if (ret != 0)
    {
        printf("\nConnection error: %d\n", ret);
        return -1;
    }
    printf("Success\n\n");
    printf("MAC: %s\n", wifi->get_mac_address());
    SocketAddress a;
    wifi->get_ip_address(&a);
    printf("IP: %s\n", a.get_ip_address());
    wifi->get_netmask(&a);
    printf("Netmask: %s\n", a.get_ip_address());
    wifi->get_gateway(&a);
    printf("Gateway: %s\n", a.get_ip_address());
    thread_messages.start(temperature_handler);
    mqtt_demo(wifi);

    printf("\nDone\n");
}
*/