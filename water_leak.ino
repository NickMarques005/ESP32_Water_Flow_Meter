
#include <FS.h>
#include <Arduino.h>
#include "config.h"
#include <ArduinoJson.h>
#include "WiFi.h"
#include <HTTPClient.h>
#include <ESPAsyncWebSrv.h>
#include <AsyncTCP.h>
#include "SPIFFS.h"

/************************
 * VARIABLES AND DEFINES
 ************************/

// LVGL:
static lv_obj_t *screenApp;
static lv_obj_t *btnTurnOn;

// WIFI:
const char *apSSID = "LEAKDEVICE_AP";
const char *apPassword = "411122d6-712b-11ee-b962-0242ac120002";

char network_ssid[64] = "";
char network_pass[64] = "";

bool isConnectedToWifi = false;
bool connectToWifi = false;
bool connectToAP = false;
bool hasServer = false;
//

// SENSOR DATA:

bool waterSensorActive = false;
unsigned long lastUpdateTime = 0;
float waterFlowRateMililiters = 0.0;
float totalWaterConsumedLiters = 0.0;

//

// HTTP SERVER:
const char *server_url_sendData = "http://server_ip:1000/api/saveFlowData";
const char *server_url_obtainNetwork = "http://server_ip:1000/api/sendNetworkData";
//

// WEB SERVER:

AsyncWebServer server(80);

const char *PARAM_INPUT_1 = "ssid";
const char *PARAM_INPUT_2 = "pass";

String ssid;
String pass;

// File paths to save input values permanently
const char *ssidPath = "/ssid.txt";
const char *passPath = "/pass.txt";

IPAddress localIP(192, 168, 1, 200);
// IPAddress localIP(192, 168, 1, 200); // hardcoded

// Set your Gateway IP address
IPAddress localGateway(192, 168, 1, 1);
// IPAddress localGateway(192, 168, 1, 1); //hardcoded
IPAddress subnet(255, 255, 0, 0);

//

// REQUEST DATA:

char sensor_Id[50] = "411122d6-712b-11ee-b962-0242ac120002";
float sensor_Value = 0.0;

//

// TIMER:
unsigned long lastTime = 0;
unsigned long lastFiveSecondsTime = 0;
unsigned long timerDelay = 5000;
bool stopTimers = false;

//

//---- DEFINES:

#define DEFAULT_SCREEN_TIMEOUT 30 * 1000
#define RTC_TIME_ZONE "BRT-3"

/**********************
 *        TTGO
 **********************/

TTGOClass *ttgo;

/**********************
 *      FUNCTIONS
 **********************/

//*** FUNCTION SPIFFS: ***

void initSPIFFS()
{
    if (!SPIFFS.begin(true))
    {
        Serial.println("Houve um erro ao iniciar SPIFFS");
    }
    Serial.println("SPIFFS criado com sucesso");
}

String readFile(fs::FS &fs, const char *path)
{
    Serial.printf("Lendo arquivo: %s\r\n", path);

    File file = fs.open(path);
    if (!file || file.isDirectory())
    {
        Serial.println("Falhou ao abrir arquivo para leitura...");
        return String();
    }

    String fileContent;
    while (file.available())
    {
        fileContent = file.readStringUntil('\n');
        break;
    }

    return fileContent;
}

void writeFile(fs::FS &fs, const char *path, const char *message)
{
    Serial.printf("Alterando arquivo: %s\r\n", path);

    File file = fs.open(path, FILE_WRITE);
    if (!file)
    {
        Serial.println("Falha ao abrir arquivo para escrever");
        return;
    }
    if (file.print(message))
    {
        Serial.println("Arquivo escrito");
    }
    else
    {
        Serial.println("Falha no arquivo");
    }
}

//

//*** FUNCTION SEND DATA PROTOTYPE: ***

void sendSensorDataRequest();

//*** FUNCTION TO GENERATE TEST RANDOM SENSOR VALUE: ***

void simulateWaterSensorReading()
{
    if (waterSensorActive)
    {

        // Simulação de consumo de água:
        waterFlowRateMililiters = random(1600, 2000) / 100.0;

        Serial.print("CONSUMO EM MILI LITROS: ");
        Serial.println(waterFlowRateMililiters);
        // Atualização do consumo total de água em litros
        totalWaterConsumedLiters += waterFlowRateMililiters / 1000.0;

        Serial.print("SUM WATER: ");
        Serial.printf("%.4f\n", totalWaterConsumedLiters);
    }
}

void handleSensorData()
{
    if (stopTimers)
    {
        return;
    }

    unsigned long currentTime = millis();

    if (currentTime - lastTime >= 1000)
    {
        simulateWaterSensorReading();
        lastTime = currentTime;
    }

    if (currentTime - lastFiveSecondsTime >= 5000)
    {
        if (totalWaterConsumedLiters != 0.0)
        {
            Serial.println("*******************************");
            Serial.print("TOTAL WATER CONSUMED (LITERS): ");
            Serial.printf("%.4f\n", totalWaterConsumedLiters);

            sendSensorDataRequest();

            totalWaterConsumedLiters = 0.0;

            lastFiveSecondsTime = currentTime;
            
        }
    }
}

void handleWaterSensor()
{

    waterSensorActive = !waterSensorActive;
    if (waterSensorActive)
    {

        stopTimers = false;
        Serial.println("SENSOR LIGADO!");
    }
    else
    {
        totalWaterConsumedLiters = 0.0;
        stopTimers = true;
        Serial.println("SENSOR DESLIGADO!");
    }
}

//*** FUNCTION ACCESS POINT: ***

void startAccessPoint()
{
    WiFi.softAP(apSSID, apPassword);
    IPAddress apIP = WiFi.softAPIP();

    Serial.print("Endereço IP do ponto de acesso: ");
    Serial.println(apIP);

    Serial.print("Endereço Local: ");
    Serial.println(WiFi.localIP());

    connectToAP = true;
}

//*** FUNCTION WIFI: ***

void initialize_Wifi()
{
    if (ssid == "" || pass == "")
    {
        Serial.println("SSID ou PASS indefinidos");
        connectToWifi = false;
        return;
    }
    WiFi.mode(WIFI_STA);

    WiFi.begin(ssid.c_str(), pass.c_str());
    Serial.print("CONECTANDO AO WIFI...");

    unsigned long startTime = millis();

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(1000);
        Serial.print(".");

        if (millis() - startTime >= 10000)
        {
            Serial.println("\n********************");
            Serial.println("FALHA NA CONEXAO WIFI!");
            Serial.println("**********************");
            isConnectedToWifi = false;
            connectToWifi = false;
            updateWifiConnectionStatus();
            return;
        }
    }
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    isConnectedToWifi = true;
    updateWifiConnectionStatus();
}

void updateWifiConnectionStatus()
{
    if (isConnectedToWifi)
    {
        Serial.println("CONECTADO AO WIFI!");
    }
    else
    {
        Serial.println("NAO CONECTADO AO WIFI!");
    }
}

//*** FUNCTION SERVER: ***

void notFound(AsyncWebServerRequest *request)
{
    request->send(404, "text/plain", "Not found");
}

void initialize_APServer()
{

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        String html = "<!DOCTYPE html>\n<html>\n<head>\n<title>Network Configuration</title>\n<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n<link rel=\"icon\" href=\"data:,\">\n<link rel=\"stylesheet\" type=\"text/css\" href=\"style.css\">\n</head>\n<body>\n<div class=\"topnav\">\n<h1>Configure sua Rede</h1>\n</div>\n<div class=\"content\">\n<div class=\"card-grid\">\n<div class=\"card\">\n<form action=\"/\" method=\"POST\">\n<p>\n<label for=\"ssid\">SSID:</label>\n<input type=\"text\" id=\"ssid\" name=\"ssid\"><br>\n<label for=\"pass\">Senha:</label>\n<input type=\"text\" id=\"pass\" name=\"pass\"><br>\n<br><br>\n<input type=\"submit\" value=\"Enviar\">\n</p>\n</form>\n</div>\n</div>\n</div>\n</body>\n</html>\n<style>\nhtml {\nfont-family: Arial, Helvetica, sans-serif;\ndisplay: inline-block;\ntext-align: center;\n}\nh1 {\nfont-size: 1.8rem;\ncolor: white;\n}\np {\nfont-size: 1.4rem;\n}\n.topnav {\noverflow: hidden;\nbackground-color: #231943;\npadding-block: 30px\n}\nbody {\nmargin: 0;\n}\n.content {\npadding: 5%;\n}\n.card-grid {\nmax-width: 800px;\nmargin: 0 auto;\ndisplay: grid;\ngrid-gap: 2rem;\ngrid-template-columns: repeat(auto-fit, minmax(300px, 1fr));\n}\n.card {\nbackground-color: white;\nbox-shadow: 2px 2px 12px 1px rgba(140, 140, 140, .5);\n}\n.card-title {\nfont-size: 1.2rem;\nfont-weight: bold;\ncolor: #220378\n}\ninput[type=submit] {\nborder: none;\ncolor: #FEFCFB;\nbackground-color: #1c0378;\npadding: 15px 15px;\ntext-align: center;\ntext-decoration: none;\ndisplay: inline-block;\nfont-size: 16px;\nwidth: 100px;\nmargin-right: 10px;\nborder-radius: 4px;\ntransition-duration: 0.4s;\n}\ninput[type=submit]:hover {\nbackground-color: #4412a2;\n}\ninput[type=text],\ninput[type=number],\nselect {\nwidth: 50%;\npadding: 12px 20px;\nmargin: 18px;\ndisplay: inline-block;\nborder: 1px solid #ccc;\nborder-radius: 4px;\nbox-sizing: border-box;\n}\nlabel {\nfont-size: 1.2rem;\n}\n.value {\nfont-size: 1.2rem;\ncolor: #4412a2;\n}\n.state {\nfont-size: 1.2rem;\ncolor: #4412a2;\n}\nbutton {\nborder: none;\ncolor: #FEFCFB;\npadding: 15px 32px;\ntext-align: center;\nfont-size: 16px;\nwidth: 100px;\nborder-radius: 4px;\ntransition-duration: 0.4s;\n}\n.button-on {\nbackground-color: #240378;\n}\n.button-on:hover {\nbackground-color: #3d12a2;\n}\n.button-off {\nbackground-color: #605a65;\n}\n.button-off:hover {\nbackground-color: #241633;\n}\n</style>";
        request->send(200, "text/html", html); });

    server.on("/", HTTP_POST, [](AsyncWebServerRequest *request){
        int params = request->params();
        for(int i=0; i < params; i++)
        {
            AsyncWebParameter* parameter = request->getParam(i);
            if(parameter->isPost())
            {
                if(parameter->name() == PARAM_INPUT_1)
                {
                    ssid = parameter->value().c_str();
                    Serial.print("SSID: ");
                    Serial.println(ssid);
                    writeFile(SPIFFS, ssidPath, ssid.c_str());
                }
                if(parameter->name() == PARAM_INPUT_2)
                {
                    pass = parameter->value().c_str();
                    Serial.print("PASSWORD: ");
                    Serial.println(pass);
                    writeFile(SPIFFS, passPath, pass.c_str());
                }
            }
            request->send(200, "text/html", "Dados de rede salvos.");
            
            connectToWifi = true;
            delay(1000);
        } });

    server.onNotFound(notFound);

    server.begin();
    Serial.println("SERVER CRIADO COM SUCESSO!");
    hasServer = true;
}

//*** FUNCTION HTTP REQUEST: ***

void obtainNetworkData()
{
    WiFiClient client;
    HTTPClient http;
    http.begin(client, server_url_obtainNetwork);
    http.addHeader("Content-Type", "application/json");

    // DADOS QUE SERÃO ENVIADOS:
    Serial.print("\nSensor: ");
    Serial.println(sensor_Id);
    DynamicJsonDocument jsonDoc(256);
    jsonDoc["sensor_id"] = sensor_Id;

    String requestData;
    serializeJson(jsonDoc, requestData);

    Serial.print("REQUEST DATA: ");
    Serial.println(requestData);

    int httpResponseCode = http.POST(requestData);

    if (httpResponseCode > 0)
    {
        if (httpResponseCode == HTTP_CODE_OK)
        {
            String response = http.getString();
            Serial.print("NETWORK DATA RECEIVED: ");
            Serial.println(response);
        }
        else if (httpResponseCode == HTTP_CODE_NOT_FOUND)
        {
            Serial.println("Página não encontrada no servidor");
        }
        else if (httpResponseCode == HTTP_CODE_BAD_REQUEST)
        {
            Serial.println("Requisição mal formatada");
        }
    }
    else
    {
        Serial.print("HTTP Error Code: ");
        Serial.println(httpResponseCode);

        if (httpResponseCode == 0)
        {
            Serial.println("Não foi possível se conectar ao servidor");
            // Tente novamente
        }
    }

    http.end();
}

void sendSensorDataRequest()
{
    // CONEXÃO HTTP COM O SERVER:
    WiFiClient client;
    HTTPClient http;

    http.begin(client, server_url_sendData);
    http.addHeader("Content-Type", "application/json");

    // SENSOR VALUE TEST:
    sensor_Value = totalWaterConsumedLiters;

    // DADOS QUE SERÃO ENVIADOS:

    DynamicJsonDocument jsonDoc(256);
    jsonDoc["sensor_id"] = sensor_Id;
    jsonDoc["sensor_value"] = sensor_Value;

    String requestData;
    serializeJson(jsonDoc, requestData);

    Serial.print("REQUEST DATA: ");
    Serial.println(requestData);

    int httpResponseCode = http.POST(requestData);

    if (httpResponseCode > 0)
    {
        Serial.print("HTTP Response Code: ");
        Serial.println(httpResponseCode);

        // STATUS DE RESPOSTA HTTP:
        if (httpResponseCode == HTTP_CODE_OK)
        {
            Serial.println("Dados enviados com sucesso!");
        }
        else if (httpResponseCode == HTTP_CODE_NOT_FOUND)
        {
            Serial.println("Página não encontrada no servidor");
        }
        else if (httpResponseCode == HTTP_CODE_BAD_REQUEST)
        {
            Serial.println("Requisição mal formatada");
        }
    }
    else
    {
        Serial.print("HTTP Error Code: ");
        Serial.println(httpResponseCode);

        if (httpResponseCode == 0)
        {
            Serial.println("Não foi possível se conectar ao servidor");
            // Tente novamente ou notifique o usuário
        }
    }

    http.end();
}

// HANDLE NETWORK SYSTEM:

static void handleNetworkSystem()
{

    if (!isConnectedToWifi)
    {
        if (!hasServer)
        {
            // Inicializar WIFI ao Access Point:
            Serial.println("\nINICIALIZAR SERVER PARA RECEBER DADOS DE REDE!");
            initialize_APServer();
        }
    }

    delay(5000);
}

// FUNÇÕES DOS BOTÕES:

static void btnTurnAction(lv_obj_t *btn, lv_event_t event)
{
    if (event == LV_EVENT_CLICKED)
    {
        handleWaterSensor();
        Serial.println("TURN SENSOR CLICKED");
    }
}

//*** FUNCTION TEST LVGL: ***

void screen_Test()
{
    //*----Style ScreenApp
    static lv_style_t style_ScreenApp; // Cria o estilo da tela do App
    lv_style_init(&style_ScreenApp);   // Inicializa o estilo
    lv_style_set_radius(&style_ScreenApp, LV_OBJ_PART_MAIN, 0);
    lv_style_set_border_width(&style_ScreenApp, LV_OBJ_PART_MAIN, 0);
    //*----

    //**----TELA DO APLICATIVO
    screenApp = lv_obj_create(lv_scr_act(), NULL); // Cria o objeto que quando tiver parente nulo transformará tal objeto na própria tela do app
    lv_obj_set_size(screenApp, LV_HOR_RES, LV_VER_RES);
    lv_obj_add_style(screenApp, LV_OBJ_PART_MAIN, &style_ScreenApp);
    lv_scr_load(screenApp);
    //**----

    //**----LABEL TESTE
    lv_obj_t *label = lv_label_create(screenApp, NULL);
    lv_label_set_text(label, "Teste WaterLeak");
    lv_obj_align(label, NULL, LV_ALIGN_CENTER, 0, 0);

    //**----BOTAO LIGAR
    btnTurnOn = lv_btn_create(screenApp, NULL);
    lv_obj_set_size(btnTurnOn, 160, 50);
    lv_obj_align(btnTurnOn, NULL, LV_ALIGN_IN_BOTTOM_MID, 0, -10);
    lv_obj_set_event_cb(btnTurnOn, btnTurnAction);
    lv_obj_t *labelTurnOn = lv_label_create(btnTurnOn, NULL);
    lv_label_set_text(labelTurnOn, "SENSOR");
}

/**********************
 *       SETUP
 **********************/

void setup()
{
    // SETUP BÁSICO DO DISPOSITIVO:
    Serial.begin(115200);         // Configura um serial monitor
    ttgo = TTGOClass::getWatch(); // Faz a instância do T Watch para verificar se está conectado e disponível
    ttgo->begin();                // Começa a execução do TTGO
    ttgo->lvgl_begin();           // Inicia o processamento da biblioteca da interface gráfica Littlevgl
    ttgo->openBL();               // Ativa a luz de fundo da tela

    // Check if the RTC clock matches, if not, use compile time
    ttgo->rtc->check();

    // Synchronize time to system time
    ttgo->rtc->syncToSystem();

    initSPIFFS();

    screen_Test();
    Serial.println("START ESP32!");

    ssid = readFile(SPIFFS, ssidPath);
    pass = readFile(SPIFFS, passPath);

    Serial.printf("SSID: %s\n", ssid);
    Serial.printf("PASS: %s\n", pass);

    connectToWifi = true;
}

/**********************
 *        LOOP
 **********************/

void loop()
{
    if (lv_disp_get_inactive_time(NULL) < DEFAULT_SCREEN_TIMEOUT)
    {
        lv_task_handler();
    }

    // Caso não tenha conexão Wifi e não seja para conectar ao wifi via ssid e password, então criar Access Point:
    if (!isConnectedToWifi)
    {
        if (!connectToWifi)
        {
            if (!connectToAP)
            {
                startAccessPoint();
            }
            else
            {
                // Serial.println("AP CONNECTED");
                handleNetworkSystem();
                delay(4000);
            }
        }
        else
        {
            initialize_Wifi();
            delay(2000);
        }
    } 
    else
    {   //Se estiver conectado ao WiFi:
        handleSensorData();
    }

    lv_tick_inc(3);
    delay(5);
}
