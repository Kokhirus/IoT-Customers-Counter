#include "mbed.h"
#include "MQTTmbed.h"
#include "MQTTClient.h"
#include "MQTTNetwork.h"

WiFiInterface *wifi;

#define MQTTCLIENT_QOS2 1
 
MQTT::Client<MQTTNetwork, Countdown>* client_ptr;

Thread thread_customers_cnt;
char* customers_amount_topic = "Kokhirus/feeds/iot.customers";
char* shop_opened_topic = "Kokhirus/feeds/iot.shop-opened";
int customers_amount = 40;
volatile bool shop_opened = true;

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
 
void customers_amount_handler() {
    char msg[5];
    while(true) {
        if(!shop_opened) {
            //thread_sleep_for(5000);
            continue;
        }
        customers_amount += rand()%5;
        sprintf(msg, "%d", customers_amount);
        post_message(customers_amount_topic, msg);
        thread_sleep_for(10000);
    }
}

int str_to_int(int len, char* str) {
    int r3salt = 0;
    for (int i = 0; i < len; ++i) {
        r3salt += (str[i] - '0') * pow(10, (len - 1));
        --len;
    }
    return r3salt;
}

void customers_amount_listener(MQTT::MessageData& md)
{
    MQTT::Message &message = md.message;
    printf("customers_amount_listener: ");
    printf("Payload %.*s\r\n", message.payloadlen, (char*)message.payload);
}

void shop_opened_listener(MQTT::MessageData& md)
{
    MQTT::Message &message = md.message;
    char *content = (char *)message.payload;

    printf("shop_opened_listener: ");
    printf("Payload %.*s\r\n", message.payloadlen, content);

    shop_opened = str_to_int(1, content);
    //printf("shop_opened now equals %d\r\n", shop_opened);
    if(!shop_opened) {
        customers_amount = 0;
        post_message(customers_amount_topic, "0");
    }
}

void messageArrived(MQTT::MessageData& md)
{
    MQTT::Message &message = md.message;
    printf("Message arrived: qos %d, retained %d, dup %d, packetid %d\r\n", message.qos, message.retained, message.dup, message.id);
    printf("Payload %.*s\r\n", message.payloadlen, (char*)message.payload);
}
 
void mqtt_demo(NetworkInterface *net)
{
    float version = 0.6;
 
    MQTTNetwork network(net);
    MQTT::Client<MQTTNetwork, Countdown> client = MQTT::Client<MQTTNetwork, Countdown>(network);
    client_ptr = &client;

    char* hostname = "52.54.110.50";
    int port = 1883;
 
    printf("Connecting to %s:%d\r\n", hostname, port);

    int rc = network.connect(hostname, port);
 
    if (rc != 0)
        printf("rc from TCP connect is %d\r\n", rc);
    printf("Connected socket\n\r");
 
    MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
    data.MQTTVersion = 3;
    data.clientID.cstring = "2a617592b917";
    data.username.cstring = "Kokhirus";
    data.password.cstring = "aio_veME64C0nrUnPwTmNNWYkTwkOTHy";
    if ((rc = client.connect(data)) != 0)
        printf("rc from MQTT connect is %d\r\n", rc);
 
    if ((rc = client.subscribe(customers_amount_topic, MQTT::QOS2, customers_amount_listener)) != 0)
        printf("rc from MQTT subscribe customers_amount_topic is %d\r\n", rc);

    if ((rc = client.subscribe(shop_opened_topic, MQTT::QOS2, shop_opened_listener)) != 0)
        printf("rc from MQTT shop_opened_topic subscribe is %d\r\n", rc);

    thread_customers_cnt.start(customers_amount_handler);
    
    while (true)
        client.yield(100);
 
    if ((rc = client.unsubscribe(customers_amount_topic)) != 0)
        printf("rc from unsubscribe was %d\r\n", rc);

    if ((rc = client.unsubscribe(shop_opened_topic)) != 0)
        printf("rc from unsubscribe was %d\r\n", rc);
 
    if ((rc = client.disconnect()) != 0)
        printf("rc from disconnect was %d\r\n", rc);
 
    network.disconnect();
 
    printf("Version %.2f: finish\r\n", version);
 
    return;
}

const char *sec2str(nsapi_security_t sec)
{
    switch (sec) {
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

int scan_demo(WiFiInterface *wifi)
{
    WiFiAccessPoint *ap;

    printf("Scan:\n");

    int count = wifi->scan(NULL,0);

    if (count <= 0) {
        printf("scan() failed with return value: %d\n", count);
        return 0;
    }

    /* Limit number of network arbitrary to 15 */
    count = count < 15 ? count : 15;

    ap = new WiFiAccessPoint[count];
    count = wifi->scan(ap, count);

    if (count <= 0) {
        printf("scan() failed with return value: %d\n", count);
        return 0;
    }

    for (int i = 0; i < count; i++) {
        printf("Network: %s secured: %s BSSID: %hhX:%hhX:%hhX:%hhx:%hhx:%hhx RSSI: %hhd Ch: %hhd\n", ap[i].get_ssid(),
               sec2str(ap[i].get_security()), ap[i].get_bssid()[0], ap[i].get_bssid()[1], ap[i].get_bssid()[2],
               ap[i].get_bssid()[3], ap[i].get_bssid()[4], ap[i].get_bssid()[5], ap[i].get_rssi(), ap[i].get_channel());
    }
    printf("%d networks available.\n", count);

    delete[] ap;
    return count;
}

DigitalIn button(USER_BUTTON);

int main()
{
    printf("WiFi example\n");

    wifi = WiFiInterface::get_default_instance();
    if (!wifi) {
        printf("ERROR: No WiFiInterface found.\n");
        return -1;
    }
    // int count = scan_demo(wifi);
    // if (count == 0) {
    //     printf("No WIFI APs found - can't continue further.\n");
    //     return -1;
    // }
    printf("\nConnecting to %s...\n", MBED_CONF_APP_WIFI_SSID);
    int ret = wifi->connect(MBED_CONF_APP_WIFI_SSID, MBED_CONF_APP_WIFI_PASSWORD, NSAPI_SECURITY_WPA_WPA2);
    if (ret != 0) {
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

    mqtt_demo(wifi);

    wifi->disconnect();

    printf("\nDone\n");
}