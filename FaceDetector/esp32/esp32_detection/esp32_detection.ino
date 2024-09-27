#include "WiFi.h"
#include "esp_camera.h"
#include "esp_timer.h"
#include "img_converters.h"
#include "Arduino.h"
#include "soc/soc.h"           // Desabilita problemas de brownout
#include "soc/rtc_cntl_reg.h"  // Desabilita problemas de brownout
#include "driver/rtc_io.h"
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <FS.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>

// Credenciais da rede Wi-Fi
const char* ssid = "Inteli.Iot";
const char* password = "@Intelix10T#";

// Cria o objeto AsyncWebServer na porta 80
AsyncWebServer server(80);

// Variáveis globais e semáforos
bool photo_ready = false;
SemaphoreHandle_t photoSemaphore;
SemaphoreHandle_t sendSemaphore;

// Caminho do arquivo para salvar a foto
#define FILE_PHOTO "/photo.jpg"

// Pinos da câmera (modelo AI-Thinker)
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5

#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// Função para verificar se a foto foi salva corretamente
bool checkPhoto(fs::FS &fs) {
  File f_pic = fs.open(FILE_PHOTO);
  unsigned int pic_sz = f_pic.size();
  f_pic.close();
  return (pic_sz > 100);
}

// Função para capturar e salvar a foto no SPIFFS
void capturePhotoSaveSpiffs() {
  camera_fb_t * fb = NULL;
  bool ok = false;

  do {
    Serial.println("Capturando uma foto...");
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Falha na captura da câmera");
      return;
    }

    Serial.printf("Nome do arquivo da foto: %s\n", FILE_PHOTO);
    File file = SPIFFS.open(FILE_PHOTO, FILE_WRITE);

    if (!file) {
      Serial.println("Falha ao abrir o arquivo no modo de escrita");
    } else {
      file.write(fb->buf, fb->len);
      Serial.printf("A foto foi salva em %s - Tamanho: %u bytes\n", FILE_PHOTO, file.size());
      file.close();
    }
    esp_camera_fb_return(fb);

    ok = checkPhoto(SPIFFS);
  } while (!ok);
}

// Tarefa de captura de fotos
void capturePhotoTask(void * parameter) {
  while (true) {
    if (xSemaphoreTake(photoSemaphore, portMAX_DELAY) == pdTRUE) {
      capturePhotoSaveSpiffs();
      photo_ready = true;
      xSemaphoreGive(sendSemaphore);
    }
  }
}

// Tarefa de envio de fotos ao servidor
void sendPhotoTask(void * parameter) {
  while (true) {
    if (xSemaphoreTake(sendSemaphore, portMAX_DELAY) == pdTRUE) {
      if (photo_ready) {
        Serial.println("Enviando a foto para o computador...");
        File file = SPIFFS.open(FILE_PHOTO, FILE_READ);
        if (file) {
          size_t fileSize = file.size();
          uint8_t *buffer = (uint8_t *)malloc(fileSize);
          if (buffer) {
            file.read(buffer, fileSize);
            file.close();

            if (WiFi.status() == WL_CONNECTED) {
              HTTPClient http;
              http.begin("http://10.128.0.8:5000/receive-image"); // Substitua pelo IP do seu servidor
              http.addHeader("Content-Type", "image/jpeg");

              int httpResponseCode = http.POST(buffer, fileSize);

              if (httpResponseCode > 0) {
                String response = http.getString();
                Serial.println(httpResponseCode);
                Serial.println(response);
              } else {
                Serial.print("Erro ao enviar a imagem. Código de erro: ");
                Serial.println(httpResponseCode);
              }
              http.end();
            } else {
              Serial.println("Não conectado ao WiFi");
            }
            free(buffer);
          } else {
            Serial.println("Falha ao alocar memória para o buffer da imagem");
          }
        } else {
          Serial.println("Falha ao abrir o arquivo da foto para leitura.");
        }
      }
    }
  }
}

// Função para processar os dados de detecção de faces recebidos
void handleFaceDetection(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
  // Parseia o JSON diretamente do buffer de dados
  const size_t capacity = JSON_ARRAY_SIZE(10) + 10 * JSON_OBJECT_SIZE(4);
  DynamicJsonDocument doc(capacity);
  DeserializationError error = deserializeJson(doc, data, len);

  if (error) {
    Serial.print("Erro ao parsear JSON: ");
    Serial.println(error.c_str());
    request->send(400, "application/json", "{\"status\": \"error\", \"message\": \"Formato JSON inválido\"}");
    return;
  }

  Serial.println("Dados de detecção de face recebidos:");
  serializeJsonPretty(doc, Serial);
  Serial.println();
  request->send(200, "application/json", "{\"status\": \"success\"}");
  // Aqui você pode processar os dados recebidos conforme necessário
}

// Função setup
void setup() {
  // Porta serial para depuração
  Serial.begin(115200);

  // Desativa o detector de brownout
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  // Inicializa os semáforos
  photoSemaphore = xSemaphoreCreateBinary();
  sendSemaphore = xSemaphoreCreateBinary();

  // Conecta ao Wi-Fi
  WiFi.begin(ssid, password);
  Serial.println("Conectando ao WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Conectado ao Wi-Fi. Endereço IP: ");
  Serial.println(WiFi.localIP());

  // Inicializa o SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("Ocorreu um erro ao montar o SPIFFS");
    ESP.restart();
  } else {
    Serial.println("SPIFFS montado com sucesso");
  }

  // Configuração da câmera
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0       = Y2_GPIO_NUM;
  config.pin_d1       = Y3_GPIO_NUM;
  config.pin_d2       = Y4_GPIO_NUM;
  config.pin_d3       = Y5_GPIO_NUM;
  config.pin_d4       = Y6_GPIO_NUM;
  config.pin_d5       = Y7_GPIO_NUM;
  config.pin_d6       = Y8_GPIO_NUM;
  config.pin_d7       = Y9_GPIO_NUM;
  config.pin_xclk     = XCLK_GPIO_NUM;
  config.pin_pclk     = PCLK_GPIO_NUM;
  config.pin_vsync    = VSYNC_GPIO_NUM;
  config.pin_href     = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM;
  config.xclk_freq_hz = 10000000;
  config.pixel_format = PIXFORMAT_JPEG;

  if (psramFound()) {
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  // Inicializa a câmera
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Falha na inicialização da câmera com erro 0x%x", err);
    ESP.restart();
  }

  // Define rotas do servidor
  server.on("/capture", HTTP_GET, [](AsyncWebServerRequest * request) {
    xSemaphoreGive(photoSemaphore);  // Libera o semáforo para capturar uma nova foto
    request->send(200, "text/plain", "Capturando Foto");
  });

  server.on("/saved-photo", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, FILE_PHOTO, "image/jpeg");
  });

  // Endpoint para receber dados de detecção de faces
  server.on("/face-detection", HTTP_POST, [](AsyncWebServerRequest *request){
    // Manipulação da resposta
  }, NULL, handleFaceDetection);

  // Inicia o servidor
  server.begin();

  // Cria as tasks
  xTaskCreate(capturePhotoTask, "CapturePhotoTask", 8192, NULL, 1, NULL);
  xTaskCreate(sendPhotoTask, "SendPhotoTask", 8192, NULL, 1, NULL);
}

void loop() {
}

