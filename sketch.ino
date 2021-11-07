#include <Adafruit_ADS1X15.h>
#include <WiFi.h>
#include <WebServer.h>
#include <uri/UriRegex.h>
#include <PageBuilder.h>

#include "static.h"

#define SSID "ssid"
#define PASSWORD "password"

#define N_PLANTS 8

enum PlantMode {notAttached, manual, automatic};

struct Plant{
    String name;
    PlantMode mode;

    Adafruit_ADS1115 adc;
    int adcPort;

    int pumpGPIO;

    int moistureThreshold; // percentage
    int wateringInterval; // number of days
};

Plant plants[N_PLANTS];

int getMoisturePercent(Plant plant){
    return (int) plant.adc.readADC_SingleEnded(plant.adcPort) / 2 ^ 16 * 100;
}

String getPlantModeStr(Plant plant){
    return String(plant.mode == automatic ? "Automatic" : "Manual");
}

void printPlant(int i){
    Plant plant = plants[i];
    Serial.print("plant(slot=");
    Serial.print(i);
    Serial.print(", name=");
    Serial.print(plant.name);
    Serial.print(", mode=");
    Serial.print(plant.mode);
    Serial.print(", threshold=");
    Serial.print(plant.moistureThreshold);
    Serial.print(", interval=");
    Serial.print(plant.wateringInterval);
    Serial.println(")");
}


Adafruit_ADS1115 adsGND;
Adafruit_ADS1115 adsVDD;

int pumpGPIOs[] { 32, 33, 25, 26, 27, 14, 12, 13 };

WebServer server;


class GetPlantPageBuilder : public PageBuilder {
    public:
        GetPlantPageBuilder() : PageBuilder({}, HTTP_GET), _uri("^\\/plants\\/([0-N_PLANTS]+)$")
        {
            _uri.initPathArgs(pathArgs);
        }

        bool canHandle(HTTPMethod method, String requestUri){
            if (method == HTTP_GET) return _uri.canHandle(requestUri, pathArgs);
            return false;
        }

        bool handle(WebServer& server, HTTPMethod requestMethod, PageBuilderUtil::URI_TYPE_SIGNATURE requestUri){
            PageBuilder::clearElements();

            int plantId = server.pathArg(0).toInt();
            
            PageElement header(
                HTML_HEADER_MOLD,
                {{"TITLE", [=](PageArgument& args){ return String("Edit plant ") + String(plantId); }}}
            );
            PageBuilder::addElement(header);

            Plant plant = plants[plantId];
            printPlant(plantId);
            String asp, msp, defaultInterval, defaultThreshold;
            if (plant.mode != notAttached){
                if (plant.mode == automatic){
                    msp = " ";
                    asp = " selected=\"selected\" ";
                    defaultInterval = String(plant.wateringInterval);
                    defaultThreshold = String(plant.moistureThreshold);
                } else {
                    msp = " selected=\"selected\" ";
                    asp = " ";
                    defaultInterval = String("");
                    defaultThreshold = String("");
                }
            } else {
                    msp = " ";
                    asp = " selected=\"selected\" ";
                    defaultInterval = String("");
                    defaultThreshold = String("");
            }
            PageElement body(
                GET_PLANT_BODY_MOLD,
                {{"TITLE", [=](PageArgument& args){ return String("Edit plant ") + String(plantId); }},
                 {"ID", [=](PageArgument& args){ return String(plantId); }},
                 {"NAME", [=](PageArgument& args){ return plant.name; }},
                 {"AUTOMATIC_SELECTED_PRESET", [=](PageArgument& args){ return asp; }},
                 {"MANUAL_SELECTED_PRESET", [=](PageArgument& args){ return msp; }},
                 {"THRESHOLD_DEFAULT_VALUE", [=](PageArgument& args){ return defaultThreshold; }},
                 {"INTERVAL_DEFAULT_VALUE", [=](PageArgument& args){ return defaultInterval; }}}
            );
            PageBuilder::addElement(body);

            PageElement footer(HTML_FOOTER_MOLD, {});
            PageBuilder::addElement(footer);

            PageBuilder::handle(server, requestMethod, requestUri);
        }

    private:
        UriRegex _uri;
};


class RootPageBuilder : public PageBuilder {
    public:
        RootPageBuilder() : PageBuilder{ "/plants", {}, HTTP_GET}{}

        bool handle(WebServer& server, HTTPMethod requestMethod, PageBuilderUtil::URI_TYPE_SIGNATURE requestUri){
            PageBuilder::clearElements();

            PageElement header(
                HTML_HEADER_MOLD,
                {{"TITLE", [=](PageArgument& args){ return String("Edit plant ") + String(plantId); }}}
            );

            PageElement* plantDiv;
            for (int i = 0; i < N_PLANTS; i++){
                Plant plant = plants[i];
                if (plant.mode != notAttached){
                    if (plant.mode == automatic){
                        plantDiv = new PageElement(AUTOMATIC_PLANT_DIV_MOLD,
                                                   {{"SLOT", [=](PageArgument& args){ return String(i); }},
                                                    {"NAME", [=](PageArgument& args){ return String(plant.name); }},
                                                    {"THRESHOLD", [=](PageArgument& args){ return String(plant.moistureThreshold); }},
                                                    {"INTERVAL", [=](PageArgument& args){ return String(plant.wateringInterval); }}});

                    }
                    if (plant.mode == manual){
                        plantDiv = new PageElement(MANUAL_PLANT_DIV_MOLD,
                                                   {{"SLOT", [=](PageArgument& args){ return String(i); }},
                                                    {"NAME", [=](PageArgument& args){ return plant.name; }}});
                    }
                    PageBuilder::addElement(*plantDiv);
                }
            }
            
            PageElement footer(HTML_FOOTER_MOLD, {});
            PageBuilder::addElement(footer);
            
            PageBuilder::handle(server, requestMethod, requestUri);
        }
};

void handlePlantPost(){
    int slot = server.pathArg(0).toInt();
    String name = server.arg("name");
    PlantMode mode = server.arg("mode") == "Automatic" ? automatic : manual;
    int threshold, interval;
    if (mode == automatic){
        threshold = server.arg("threshold").toInt();
        interval = server.arg("interval").toInt();
    }

    plants[slot].name = name;
    plants[slot].mode = mode;
    plants[slot].moistureThreshold = threshold;
    plants[slot].wateringInterval = interval;

    printPlant(slot);
    
    server.send(200);
}

void handlePlantDelete(){
    int plantId = server.pathArg(0).toInt();
    plants[plantId].mode = notAttached;
    plants[plantId].name = String("");
    server.send(200);
}


GetPlantPageBuilder getPlantPB;
//RootPageBuilder rootPagePB{"/", {}, HTTP_GET};

void setup(){
    if (!adsGND.begin()) {
        Serial.println("Failed to initialize ads_GND.");
        while (1);
    }
    if (!adsVDD.begin(0x49)) {
        Serial.println("Failed to initialize ads_VDD.");
        while (1);
    }
    for (int i = 0; i < N_PLANTS; i++){
        if (i < 4) {
            plants[i].adc = adsGND;
        }
        else {
            plants[i].adc = adsVDD;
        }
        plants[i].mode = notAttached;
        plants[i].adcPort = i;
        plants[i].pumpGPIO = pumpGPIOs[i];
    }

    Serial.begin(115200);
    WiFi.begin(SSID, PASSWORD);
    while (WiFi.status() != WL_CONNECTED){
        delay(100);
    }
    Serial.print("IP address:");
    Serial.println(WiFi.localIP().toString());

    getPlantPB.insert(server);
//    rootPagePB.insert(server);

    server.on(UriRegex("^\\/plants\\/([0-8]+)$"), HTTP_DELETE, handlePlantDelete);
    server.on(UriRegex("^\\/plants\\/([0-8]+)$"), HTTP_POST, handlePlantPost);

    server.begin();
}

void loop(void){
    delay(1);
    server.handleClient();
}
