#include "mbed.h"
#include "MQTTmbed.h"
#include "MQTTClient.h"
#include "MQTTNetwork.h"

WiFiInterface *wifi;

#define MQTTCLIENT_QOS2 1

#define FRONT_MOTION_PIN  D5
#define REAR_MOTION_PIN  D6

DigitalIn front_sensor(FRONT_MOTION_PIN);
DigitalIn rear_sensor(REAR_MOTION_PIN);

MQTT::Client<MQTTNetwork, Countdown>* client_ptr;

Thread thread_customers_cnt;
Thread thread_customers_sensor;
Thread thread_shop_opened;
char* customers_amount_topic = "Kokhirus/feeds/iot.customers";
char* shop_opened_topic = "Kokhirus/feeds/iot.shop-opened";
char* open_checker_topic = "Kokhirus/feeds/iot.open-checker";
volatile int customers_amount = 0;
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
    printf("Posted a message: %s\r\n", msg);
}

void sensor(DigitalIn sens1, DigitalIn sens2, bool inc) {
    int wait_time = 0;
    while ((sens2.read() != 0 || (sens1.read() == 0 && sens2.read() == 0)) && wait_time < 50) {
        ++wait_time;
        thread_sleep_for(20);
    }
    if (wait_time >= 50) {
        printf("Customer hasnt entered. Waiting\n");
        return;
    }
    while (sens2.read() == 0)
        thread_sleep_for(20);
    printf("Update. Now there are %d customers\n", (inc ? ++customers_amount : --customers_amount));
    if (customers_amount < 0) {
        printf("Counter has reached negative number. Restoring to default (0)\r\n");
        customers_amount = 0;
    }
}

void customers_sensor_handler() {
    while(true) {
        if (front_sensor.read() != 0 && rear_sensor.read() != 0) continue;
        if (front_sensor.read() == 0)
            sensor(front_sensor, rear_sensor, true);
        else
            sensor(rear_sensor, front_sensor, false);
    }
}

void customers_amount_handler() {
    char msg[5];
    while(true) {
        if(!shop_opened) {
            post_message(customers_amount_topic, "0");
            thread_sleep_for(10000);
            continue;
        }
        //customers_amount += rand()%5;
        sprintf(msg, "%d", customers_amount);
        printf("From customers_amount_handler:\r\n");
        post_message(customers_amount_topic, msg);
        thread_sleep_for(10000);
    }
}

void shop_opened_handler() {
    while(true) {
        printf("From shop_opened_handler:\r\n");
        char a[5];
        if (shop_opened){
            sprintf(a, "%d", 1);
            post_message(open_checker_topic, a);
        }
        else{
            sprintf(a, "%d", 0);
            post_message(open_checker_topic, a);
        }
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
    customers_amount = 0;
    if(!shop_opened) {
        printf("From shop_opened_listener:\r\n");
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
    data.password.cstring = "aio_OsYD103f8kRRmnD1kqPzTNsN9ghR";
    if ((rc = client.connect(data)) != 0)
        printf("rc from MQTT connect is %d\r\n", rc);
 
    if ((rc = client.subscribe(customers_amount_topic, MQTT::QOS2, customers_amount_listener)) != 0)
        printf("rc from MQTT subscribe customers_amount_topic is %d\r\n", rc);

    if ((rc = client.subscribe(shop_opened_topic, MQTT::QOS2, shop_opened_listener)) != 0)
        printf("rc from MQTT shop_opened_topic subscribe is %d\r\n", rc);
    
    {
        char a[5];
        sprintf(a, "%d", 1);
        post_message(shop_opened_topic, a);
    }

    thread_customers_sensor.start(customers_sensor_handler);
    thread_sleep_for(1000);
    thread_customers_cnt.start(customers_amount_handler);
    thread_sleep_for(1000);
    thread_shop_opened.start(shop_opened_handler);
    
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



// int main () {
//     while(true) {
//         if (front_sensor.read() != 0 && rear_sensor.read() != 0) continue;
//         if (front_sensor.read() == 0)
//             sensor(front_sensor, rear_sensor, true);
//         else
//             sensor(rear_sensor, front_sensor, false);
//     }
// }

// int main() {
//     while (true) {
//         int wait_time = 0;
//         if (front_sensor.read() != 0 && rear_sensor.read() != 0) continue;
//         if (front_sensor.read() == 0) {
//             while ((rear_sensor.read() != 0 || (front_sensor.read() == 0 && rear_sensor.read() == 0)) && wait_time < 50) {
//                 ++wait_time;
//                 thread_sleep_for(20);
//             }
//             if (wait_time >= 50) {
//                 printf("Customer hasnt entered. ripbozo\n");
//                 continue;
//             }
//             while (rear_sensor.read() == 0)
//                 thread_sleep_for(20);
//             ++customers_amount;
//             printf("Customer has entered. %d in total\n", customers_amount);
//         }
//         else {
//             while ((front_sensor.read() != 0 || (front_sensor.read() == 0 && rear_sensor.read() == 0)) && wait_time < 50) {
//                 ++wait_time;
//                 thread_sleep_for(20);
//             }
//             if (wait_time >= 50) {
//                 printf("Customer hasnt left. ripbozo\n");
//                 continue;
//             }
//             while (front_sensor.read() == 0)
//                 thread_sleep_for(20);
//             --customers_amount;
//             printf("Customer has left. %d in total\n", customers_amount);
//         }
//     }
// }

// int main() {
//     while (true) {
//         if (front_sensor.read() != 0 && rear_sensor.read() != 0) continue;
//         if (front_sensor.read() == 0) {
//             while (rear_sensor.read() != 0 || (front_sensor.read() == 0 && rear_sensor.read() == 0))
//                 thread_sleep_for(20);
//             while (rear_sensor.read() == 0)
//                 thread_sleep_for(20);
//             ++customers_amount;
//             printf("Customer has entered. %d in total\n", customers_amount);
//         }
//         else {
//             while (front_sensor.read() != 0 || (front_sensor.read() == 0 && rear_sensor.read() == 0))
//                 thread_sleep_for(20);
//             while (front_sensor.read() == 0)
//                 thread_sleep_for(20);
//             --customers_amount;
//             printf("Customer has left. %d in total\n", customers_amount);
//         }
//     }
// }