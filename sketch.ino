#include <Adafruit_ADS1X15.h>
#include <WiFi.h>
/*
#include "/home/jost/makerkram/bewaesserung/sketch/WebServer/src/WebServer.h"
#include "/home/jost/makerkram/bewaesserung/sketch/PageBuilder/src/PageBuilder.h"
*/
#include <WebServer.h>
#include <PageBuilder.h>

#define SSID "ssid"
#define PASSWORD "password"

#define N_PLANTS 8

enum PlantMode {notAttached, manual, automatic};

struct Plant{
    char name[51];
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

static const char HTML_HEADER_MOLD[] = {
    "<!DOCTYPE html>\n"
    "<html>\n"
    " <head>\n"
    "  <meta content=\"text/html;charset=utf-8\" http-equiv=\"Content-Type\">\n"
    "  <meta content=\"utf-8\" http-equiv=\"encoding\">\n"
    "  <title>{{TITLE}}</title>\n"
    " </head>\n"
    " <body>\n"
};

static const char HTML_FOOTER_MOLD[] = {
    " </body>\n"
    "</html>\n"
};

static const char ADD_EDIT_PAGE_BODY_MOLD[] = {
    "  <h1>{{ADD_OR_EDIT_STR}} plant</h1>\n"
    "{{NONE_AVAIL_HEADING}}"
    "  <form{{NONE_AVAIL_FORM_HIDDEN}}action=\"add-or-edit-submit\" method=\"post\">\n"
    "   <div>\n"
    "    <label for=\"port-select\">Port:</label>\n"
    "    <select{{PORT_SELECT_READONLY}}name=\"slot\" id=\"port-select\">\n"
    "{{PORT_OPTS}}\n"
    "    </select>\n"
    "   </div>\n"
    "   <div>\n"
    "    <label for=\"name-select\">Name:</label>\n"
    "    <input name=\"name\" id=\"name-select\" type=\"text\" pattern=\"[\\x20-\\x7E]+\" maxlength=\"50\"{{NAME_DEFAULT_VALUE}}>\n"
    "   </div>\n"
    "   <div>\n"
    "    <label for=\"mode-select\">Mode:</label>\n"
    "    <select name=\"mode\" id=\"mode-select\">\n"
    "     <option{{AUTOMATIC_SELECTED_PRESET}}value=\"Automatic\">Automatic</option>\n"
    "     <option{{MANUAL_SELECTED_PRESET}}value=\"Manual\">Manual</option>\n"
    "    </select>\n"
    "   </div>\n"
    "   <div id=\"threshold-interval-container\">\n"
    "    <div>\n"
    "     <label for=\"threshold-select\">Moisture threshold:</label>\n"
    "     <input name=\"threshold\" id=\"threshold-select\" type=\"number\" min=\"0\" max=\"60\"{{THRESHOLD_DEFAULT_VALUE}}>\n"
    "    </div>\n"
    "    <div>\n"
    "     <label for=\"interval-select\">Checking interval:</label>\n"
    "     <input name=\"interval\" id=\"interval-select\" type=\"number\" min=\"1\" max=\"14\"{{INTERVAL_DEFAULT_VALUE}}>\n"
    "    </div>\n"
    "   </div>\n"
    "   <div>\n"
    "    <button type=\"submit\" onClick=\"window.location.href='/'\" id=\"submit-btn\">Save</button>\n"
    "   </div>\n"
    "  </form>\n"
    "  <script>\n"
    "    let sel = document.getElementById(\"mode-select\");\n"
    "    sel.addEventListener(\"change\", function() {\n"
    "        let tic = document.getElementById(\"threshold-interval-container\");\n"
    "        if (sel.value == \"Manual\") {\n"
    "            tic.style.display = \"none\";\n"
    "        }\n"
    "        else {\n"
    "            tic.style.display = \"block\";\n"
    "        }\n"
    "    });\n"
    "  </script>\n"
};


class AddPageBuilder : public PageBuilder {
    public:
        AddPageBuilder(const char* uri, PageElementVT elements, HTTPMethod method = HTTP_ANY, bool noCache = true, bool cancel = false, TransferEncoding_t chunked = Auto)
            : PageBuilder{ uri, elements, method, noCache, cancel, chunked }
        {
        }

        bool handle(WebServer& server, HTTPMethod requestMethod, PageBuilderUtil::URI_TYPE_SIGNATURE requestUri){
            PageBuilder::clearElements();

            PageElement htmlHeader(HTML_HEADER_MOLD, {{"TITLE", [](PageArgument& args){ return String("Add plant"); }}});
            PageBuilder::addElement(htmlHeader);

            PageElement body(ADD_EDIT_PAGE_BODY_MOLD, {{"ADD_OR_EDIT_STR", [](PageArgument& args){ return String("Add"); }},
                                                       {"PORT_SELECT_READONLY", [](PageArgument& args){ return String(" "); }},
                                                       {"NAME_DEFAULT_VALUE", [](PageArgument& args){ return String(""); }},
                                                       {"AUTOMATIC_SELECTED_PRESET", [](PageArgument& args){ return String(" selected=\"selected\" "); }},
                                                       {"MANUAL_SELECTED_PRESET", [](PageArgument& args){ return String(" "); }},
                                                       {"THRESHOLD_DEFAULT_VALUE", [](PageArgument& args){ return String(""); }},
                                                       {"INTERVAL_DEFAULT_VALUE", [](PageArgument& args){ return String(""); }}});
            String portOpts = "";
            for (int i = 0; i < N_PLANTS; i++){
                if (plants[i].mode == notAttached){
                    String port = String(i);
                    portOpts += String("     <option value=\"") + port + String("\">") + port + String("</option>\n");
                }
            }

            if (portOpts == ""){
                body.addToken("NONE_AVAIL_HEADING", [](PageArgument& args){ return String("  <h2 style=\"color:red;\">No plant slots are available.</h2>\n"); });
                body.addToken("NONE_AVAIL_FORM_HIDDEN", [](PageArgument& args){ return String(" hidden "); });
            } else {
                body.addToken("NONE_AVAIL_HEADING", [](PageArgument& args){ return String(""); });
                body.addToken("NONE_AVAIL_FORM_HIDDEN", [](PageArgument& args){ return String(" "); });
            }

            body.addToken("PORT_OPTS", [=](PageArgument& args){ return portOpts; });
            PageBuilder::addElement(body);

            PageElement htmlFooter(HTML_FOOTER_MOLD, {});
            PageBuilder::addElement(htmlFooter);

            PageBuilder::handle(server, requestMethod, requestUri);

        }
};

class RootPageBuilder : public PageBuilder {
    public:
        RootPageBuilder(const char* uri, PageElementVT elements, HTTPMethod method = HTTP_ANY, bool noCache = true, bool cancel = false, TransferEncoding_t chunked = Auto)
            : PageBuilder{ uri, elements, method, noCache, cancel, chunked }
        {
        }

        bool handle(WebServer& server, HTTPMethod requestMethod, PageBuilderUtil::URI_TYPE_SIGNATURE requestUri){
            Serial.println("new request: /");

            PageBuilder::clearElements();
            
            PageElement htmlHeader(HTML_HEADER_MOLD, {{"TITLE", [](PageArgument& args){ return String("Active plants"); }}});
            PageBuilder::addElement(htmlHeader);

            static const char BODY_HEADER_MOLD[] = "  <h1>Active plants</h1>\n";
            PageElement bodyHeader(BODY_HEADER_MOLD, {});
            PageBuilder::addElement(bodyHeader);

            static const char AUTOMATIC_PLANT_DIV_MOLD[] = {
                "  <div>\n"
                "   <h3>Slot {{SLOT}}: {{NAME}}</h3>\n"
                "   <p>Mode: Automatic</p>\n"
                "   <p>Moisture threshold: {{THRESHOLD}}</P>\n"
                "   <p>Watering interval: {{INTERVAL}}</P>\n"
                "   <div>\n"
                "    <form action=\"edit\" method=\"post\">\n"
                "     <input hidden name=\"slot\" type=\"number\" readonly=\"readonly\" value=\"{{SLOT}}\">\n"
                "     <button type=\"submit\">Edit</button>\n"
                "    </form>\n"
                "   </div>\n"
                "   <div>\n"
                "    <form action=\"delete\" method=\"post\">\n"
                "     <input hidden name=\"slot\" type=\"number\" readonly=\"readonly\" value=\"{{SLOT}}\">\n"
                "     <button type=\"submit\">Delete</button>\n"
                "    </form>\n"
                "   </div>\n"
                "  </div>\n"
            };

            static const char MANUAL_PLANT_DIV_MOLD[] = {
                "  <div>\n"
                "   <h3>Slot {{SLOT}}: {{NAME}}</h3>\n"
                "   <p>Mode: Manual</p>\n"
                "   <div>\n"
                "    <form action=\"edit\" method=\"post\">\n"
                "     <input hidden name=\"slot\" type=\"number\" readonly=\"readonly\" value=\"{{SLOT}}\">\n"
                "     <button type=\"submit\">Edit</button>\n"
                "    </form>\n"
                "   </div>\n"
                "   <div>\n"
                "    <form action=\"delete\" method=\"post\">\n"
                "     <input hidden name=\"slot\" type=\"number\" readonly=\"readonly\" value=\"{{SLOT}}\">\n"
                "     <button type=\"submit\">Delete</button>\n"
                "    </form>\n"
                "   </div>\n"
                "  </div>\n"
            };
            
            PageElement* plantDiv;
            for (int i = 0; i < N_PLANTS; i++){
                Plant plant = plants[i];
                if (plant.mode != notAttached){
                    printPlant(i);
                    if (plant.mode == automatic){
                        plantDiv = new PageElement(AUTOMATIC_PLANT_DIV_MOLD, {{"SLOT", [=](PageArgument& args){ return String(i); }},
                                                                          {"NAME", [=](PageArgument& args){ return String(plant.name); }},
                                                                          {"THRESHOLD", [=](PageArgument& args){ return String(plant.moistureThreshold); }},
                                                                          {"INTERVAL", [=](PageArgument& args){ return String(plant.wateringInterval); }}});

                    }
                    if (plant.mode == manual){
                        plantDiv = new PageElement(MANUAL_PLANT_DIV_MOLD, {{"SLOT", [=](PageArgument& args){ return String(i); }},
                                                                       {"NAME", [=](PageArgument& args){ return plant.name; }}});
                    }
                    PageBuilder::addElement(*plantDiv);
                }
            }
            
            PageElement htmlFooter(HTML_FOOTER_MOLD, {});
            PageBuilder::addElement(htmlFooter);
            Serial.println();

            
            PageBuilder::handle(server, requestMethod, requestUri);

        }
};



void handleEditSubmit(){
    Serial.println("new request: /add-or-edit-submit");
    int slot = server.arg("slot").toInt();
    String name = server.arg("name");
    PlantMode mode = server.arg("mode") == "Automatic" ? automatic : manual;
    int threshold, interval;
    if (mode == automatic){
        threshold = server.arg("threshold").toInt();
        interval = server.arg("interval").toInt();
    }

    Serial.print("edit_request(slot=");
    Serial.print(slot);
    Serial.print(", name=");
    Serial.print(name);
    Serial.print(", mode=");
    Serial.print(mode);
    Serial.print(", threshold=");
    Serial.print(threshold);
    Serial.print(", interval=");
    Serial.print(interval);
    Serial.println(")\n");

    name.toCharArray(plants[slot].name, 50);
    plants[slot].mode = mode;
    plants[slot].moistureThreshold = threshold;
    plants[slot].wateringInterval = interval;

    server.send(200, "text/plain", "Plant updated successfully");
}

/*
void assembleEditPage(PageBuilder& pb){
    pb.clearElements();

    PageElement htmlHeader(HTML_HEADER_MOLD, {{"TITLE", [](PageArgument& args){ return String("Edit plant"); }}});
    pb.addElement(htmlHeader);

    int plantId = server.arg("slot").toInt();
    Plant plant = plants[plantId];
    String automaticSelectedPreset = plant.mode == automatic ? String(" selected=\"selected\" ") : String(" ");
    String manualSelectedPreset = plant.mode == manual ? String(" selected=\"selected\" ") : String(" ");
    String thresholdDefaultValue = plant.mode == automatic ? String("value=\"") + String(plant.moistureThreshold) + String("\"") : String("");
    String intervalDefaultValue = plant.mode == automatic ? String("value=\"") + String(plant.wateringInterval) + String("\"") : String("");

    PageElement body(ADD_EDIT_PAGE_BODY_MOLD, {{"ADD_OR_EDIT_STR", [](PageArgument& args){ return String("Edit"); }},
                                               {"PORT_SELECT_READONLY", [](PageArgument& args){ return String(" readonly=\"readonly\" "); }},
                                               {"NAME_DEFAULT_VALUE", [=](PageArgument& args){ return plant.name; }},
                                               {"AUTOMATIC_SELECTED_PRESET", [=](PageArgument& args){ return automaticSelectedPreset; }},
                                               {"MANUAL_SELECTED_PRESET", [=](PageArgument& args){ return manualSelectedPreset; }},
                                               {"THRESHOLD_DEFAULT_VALUE", [=](PageArgument& args){ return thresholdDefaultValue; }},
                                               {"INTERVAL_DEFAULT_VALUE", [=](PageArgument& args){ return intervalDefaultValue; }}});
    pb.addElement(body);
    PageElement htmlFooter(HTML_FOOTER_MOLD, {});
    pb.addElement(htmlFooter);
}

*/

AddPageBuilder addPagePB{"/add", {}, HTTP_GET};
RootPageBuilder rootPagePB{"/", {}, HTTP_GET};

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

//    WiFi.mode(WIFI_STA);
//    esp_wifi_set_ps(WIFI_PS_NONE);

    WiFi.begin(SSID, PASSWORD);
    while (WiFi.status() != WL_CONNECTED){
        delay(100);
    }
    Serial.print("IP address:");
    Serial.println(WiFi.localIP().toString());

    addPagePB.insert(server);
    rootPagePB.insert(server);
    server.on("/add-or-edit-submit", HTTP_POST, handleEditSubmit);

    server.begin();
}
        

void loop(void){
    delay(1);
    server.handleClient();
}
