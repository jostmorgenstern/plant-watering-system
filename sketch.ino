#include <Adafruit_ADS1X15.h>
#include <WiFi.h>
#include <WebServer.h>
#include <uri/UriRegex.h>
#include <PageBuilder.h>
#include <regex>

#include "static.h"

#define SSID "ssid"
#define PASSWORD "pw"
#define N_SLOTS 8
#define SLOT_ID_REGEX "^[0-7]$"
#define MAX_SLOT_NAME_LEN 50
#define MIN_THRESHOLD 30
#define MAX_THRESHOLD 50
#define MIN_INTERVAL 1
#define MAX_INTERVAL 14


enum SlotMode {none, manual, automatic};

struct Slot{
    char name[MAX_SLOT_NAME_LEN + 1];
    SlotMode mode;

    Adafruit_ADS1115 adc;
    int adcPort;

    int pumpGPIO;

    int threshold; // percentage
    int interval; // number of days
};

Slot slots[N_SLOTS];

int getMoisturePercent(Slot slot){
    return (int) slot.adc.readADC_SingleEnded(slot.adcPort) / 2 ^ 16 * 100;
}

String getSlotModeStr(SlotMode mode){
    switch(mode){
        case automatic:
            return "automatic";
        case manual:
            return "manual";
        case none:
            return "none";
    }
}

SlotMode slotModeFromStr(String str){
        if(str == "automatic") return automatic;
        else if(str == "manual") return manual;
        else if(str == "none") return none;
}

int stringIsIntInRange(const String& str, int low, int high){
    // Checks if str is the String representation of any integer between low and high (both inclusive).
    // For example, stringIsIntInRange("10", 3, 40) is true.
    // If false, returns -1 and if true, returns the the integer represented by str.
    // Both low and high need to be positive and high needs to be greater than low.
    for (int i = low; i <= high; i++){
        if (String(i) == str) return i;
    }
    return -1;
}


void printRequest(WebServer& server){
    switch(server.method()){
        case HTTP_GET:
            Serial.print("GET ");
            break;
        case HTTP_POST:
            Serial.print("POST ");
            break;
        case HTTP_DELETE:
            Serial.print("DELETE ");
            break;
    }
    Serial.print(server.uri());
    int argc = server.args();
    if (argc > 0) Serial.print("?");
    for (int i = 0; i < argc; i++){
        Serial.print(server.argName(i));
        Serial.print("=");
        Serial.print(server.arg(i));
        if (i < argc - 1) Serial.print("&");
    }
    Serial.println();
}

void printCString(char arr[]){
    for (int i = 0; (int) arr[i] != 0; i++) {
        Serial.print(arr[i]);
        Serial.print(",");
        Serial.println();
    }
}

void printSlot(int i){
    Slot slot = slots[i];
    Serial.print("slot(id=");
    Serial.print(i);
    Serial.print(", name=");
    Serial.print(slot.name);
    Serial.print(", mode=");
    Serial.print(slot.mode);
    Serial.print(", threshold=");
    Serial.print(slot.threshold);
    Serial.print(", interval=");
    Serial.print(slot.interval);
    Serial.println(")");
}

Adafruit_ADS1115 adsGND;
Adafruit_ADS1115 adsVDD;

int pumpGPIOs[] { 32, 33, 25, 26, 27, 14, 12, 13 };

WebServer server;


class GetPlantPageBuilder : public PageBuilder {
    public:
        GetPlantPageBuilder() : PageBuilder({}, HTTP_GET), _uri("^\\/slots\\/([0-7]+)$")
        {
            _uri.initPathArgs(pathArgs);
        }

        bool canHandle(HTTPMethod method, String requestUri){
            if (method == HTTP_GET) return _uri.canHandle(requestUri, pathArgs);
            return false;
        }

        bool handle(WebServer& server, HTTPMethod requestMethod, PageBuilderUtil::URI_TYPE_SIGNATURE requestUri){
            printRequest(server);
            PageBuilder::clearElements();

            int slotId = server.pathArg(0).toInt();
            
            PageElement header(
                HTML_HEADER_MOLD,
                {{"TITLE", [=](PageArgument& args){ return String("Edit plant ") + String(slotId); }}}
            );
            PageBuilder::addElement(header);

            Slot slot = slots[slotId];
            printSlot(slotId);
            String asp, msp, defaultInterval, defaultThreshold;
            if (slot.mode != none){
                if (slot.mode == automatic){
                    msp = " ";
                    asp = " selected=\"selected\" ";
                    defaultInterval = String(slot.interval);
                    defaultThreshold = String(slot.threshold);
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
                {{"TITLE", [=](PageArgument& args){ return String("Edit plant ") + String(slotId); }},
                 {"ID", [=](PageArgument& args){ return String(slotId); }},
                 {"NAME", [=](PageArgument& args){ return slot.name; }},
                 {"AUTOMATIC_SELECTED_PRESET", [=](PageArgument& args){ return asp; }},
                 {"MANUAL_SELECTED_PRESET", [=](PageArgument& args){ return msp; }},
                 {"THRESHOLD_DEFAULT_VALUE", [=](PageArgument& args){ return defaultThreshold; }},
                 {"INTERVAL_DEFAULT_VALUE", [=](PageArgument& args){ return defaultInterval; }}}
            );
            PageBuilder::addElement(body);

            PageElement footer(HTML_FOOTER_MOLD, {});
            PageBuilder::addElement(footer);

            PageBuilder::handle(server, requestMethod, requestUri);
            Serial.println("Success");
        }

    private:
        UriRegex _uri;
};

class RootPageBuilder : public PageBuilder {
    public:
        RootPageBuilder() : PageBuilder{ "/slots", {}, HTTP_GET}{}

        bool handle(WebServer& server, HTTPMethod requestMethod, PageBuilderUtil::URI_TYPE_SIGNATURE requestUri){
            printRequest(server);
            PageBuilder::clearElements();

            PageElement header(
                HTML_HEADER_MOLD,
                {{"TITLE", [=](PageArgument& args){ return String("Active slots"); }}}
            );
            PageBuilder::addElement(header);

            static const char TITLE_MOLD[] = "     <h1>Active slots</h1>\n";
            PageElement title(TITLE_MOLD, {});
            PageBuilder::addElement(title);

            PageElement* plantDiv;
            String slotOpts = "";
            for (int i = 0; i < N_SLOTS; i++){
                Slot slot = slots[i];
                if (slot.mode == automatic){
                    plantDiv = new PageElement(AUTOMATIC_PLANT_DIV_MOLD,
                                               {{"ID", [=](PageArgument& args){ return String(i); }},
                                                {"NAME", [=](PageArgument& args){ return String(slot.name); }},
                                                {"THRESHOLD", [=](PageArgument& args){ return String(slot.threshold); }},
                                                {"INTERVAL", [=](PageArgument& args){ return String(slot.interval); }}});
                    PageBuilder::addElement(*plantDiv);
                }
                else if (slot.mode == manual){
                    plantDiv = new PageElement(MANUAL_PLANT_DIV_MOLD,
                                               {{"ID", [=](PageArgument& args){ return String(i); }},
                                                {"NAME", [=](PageArgument& args){ return slot.name; }}});
                    PageBuilder::addElement(*plantDiv);
                }
                else if (slot.mode == none){
                    String slotOpt = String("<option value=\"slotId\">slotId</option>\n");
                    slotOpt.replace("slotId", String(i));
                    slotOpts += slotOpt;
                }
                
            }
            Serial.println(slotOpts);
            PageElement addForm(ROOT_PAGE_ADD_FORM_MOLD,
                                {{"SLOTOPTS", [=](PageArgument& args){ return slotOpts; }}});
            PageBuilder::addElement(addForm);
            
            PageElement footer(HTML_FOOTER_MOLD, {});
            PageBuilder::addElement(footer);
            
            PageBuilder::handle(server, requestMethod, requestUri);
            for (int i = 0; i < N_SLOTS; i++){
                printSlot(i);
            }
            Serial.println("Success");
        }
};


void handleSlotPost(){
    printRequest(server);

    int slotId = server.pathArg(0).toInt();
    
    String modeStr = server.arg("mode");
    if (modeStr != "automatic" && modeStr != "manual" && modeStr != "none"){
        server.send(400);
        Serial.println("Error: mode invalid");
        return;
    }
    SlotMode mode = slotModeFromStr(modeStr);

    if (mode == none) {
        slots[slotId].mode = mode;
        server.send(200);
        printSlot(slotId);
        return;
    }

    if (mode == automatic){
        int threshold, interval;
        if ((threshold = stringIsIntInRange(server.arg("threshold"), MIN_THRESHOLD, MAX_THRESHOLD)) == -1){
            server.send(400);
            Serial.println("Error: threshold invalid");
            return;
        }
        if ((interval = stringIsIntInRange(server.arg("interval"), MIN_INTERVAL, MAX_INTERVAL)) == -1){
            server.send(400);
            Serial.println("Error: interval invalid");
            return;
        }
        slots[slotId].threshold = threshold;
        slots[slotId].interval = interval;
        Serial.println(threshold);
        Serial.println(interval);
    }

    int nameArgLen = server.arg("name").length();
    String nameArg = server.arg("name");
    if (nameArgLen > MAX_SLOT_NAME_LEN){
        server.send(400);
        Serial.println("Error: name too long");
        return;
    }
    for (int i = 0; i < nameArgLen; i++){
        if (!(0x20 <= nameArg.charAt(i) <= 0x7E)) {
            server.send(400);
            Serial.println("Error: name contains invalid chars");
            return;
        }
    }
    nameArg.toCharArray(slots[slotId].name, MAX_SLOT_NAME_LEN + 1);
    Serial.println(slots[slotId].name);
    slots[slotId].mode = mode;

    server.send(200);
    Serial.println("Success");
    for (int i = 0; i < N_SLOTS; i++){
        printSlot(i);
    }
    return;
}


GetPlantPageBuilder getPlantPB;
RootPageBuilder rootPagePB;

void setup(){
    if (!adsGND.begin()) {
        Serial.println("Failed to initialize ads_GND.");
        while (1);
    }
    if (!adsVDD.begin(0x49)) {
        Serial.println("Failed to initialize ads_VDD.");
        while (1);
    }
    for (int i = 0; i < N_SLOTS; i++){
        if (i < 4) {
            slots[i].adc = adsGND;
        }
        else {
            slots[i].adc = adsVDD;
        }
        slots[i].mode = none;
        slots[i].adcPort = i;
        slots[i].pumpGPIO = pumpGPIOs[i];
    }

    Serial.begin(115200);

    WiFi.begin(SSID, PASSWORD);
    while (WiFi.status() != WL_CONNECTED){
        delay(100);
    }
    Serial.print("IP address:");
    Serial.println(WiFi.localIP().toString());

    getPlantPB.insert(server);
    rootPagePB.insert(server);

    server.on(UriRegex("^\\/slots\\/([0-7]+)$"), HTTP_POST, handleSlotPost);

    server.begin();
}

void loop(void){
    delay(1);
    server.handleClient();
}
