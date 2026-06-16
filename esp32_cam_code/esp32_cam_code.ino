#include "esp_camera.h"
#include <WiFi.h>
#include "esp_http_server.h"
#include "soc/soc.h"           
#include "soc/rtc_cntl_reg.h"  

const char* ssid = "ORGHO";
const char* password = "12345678";

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

httpd_handle_t stream_httpd = NULL;

static esp_err_t stream_handler(httpd_req_t *req) {
  camera_fb_t * fb = NULL;
  esp_err_t res = ESP_OK;
  size_t _jpg_buf_len;
  uint8_t * _jpg_buf;
  char * part_buf[64];
  static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=123456789000000000000987654321";
  static const char* _STREAM_BOUNDARY = "\r\n--123456789000000000000987654321\r\n";
  static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

  res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
  if(res != ESP_OK){ return res; }

  while(true){
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      res = ESP_FAIL;
    } else {
      _jpg_buf_len = fb->len;
      _jpg_buf = fb->buf;
    }
    if(res == ESP_OK){
      res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
    }
    if(res == ESP_OK){
      size_t hlen = snprintf((char *)part_buf, 64, _STREAM_PART, _jpg_buf_len);
      res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
    }
    if(res == ESP_OK){
      res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
    }
    if(fb){
      esp_camera_fb_return(fb);
      fb = NULL;
      _jpg_buf = NULL;
    } else if(_jpg_buf){
      free(_jpg_buf);
      _jpg_buf = NULL;
    }
    if(res != ESP_OK){ break; }
    
    // "Cache" delay of 15ms to prevent crashes and give the WiFi chip time to process data 
    vTaskDelay(15 / portTICK_PERIOD_MS); 
  }
  return res;
}

void startCameraServer(){
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 81;
  config.recv_wait_timeout = 5; // Timeout optimization to prevent socket hangs or deadlocks
  config.send_wait_timeout = 5; 
  
  httpd_uri_t index_uri = {
    .uri       = "/stream",
    .method    = HTTP_GET,
    .handler   = stream_handler,
    .user_ctx  = NULL
  };
  if (httpd_start(&stream_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(stream_httpd, &index_uri);
  }
}

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); 
  
  Serial.begin(115200);

  // WiFi modem refresh optimization
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  
  WiFi.begin(ssid, password);
  WiFi.setSleep(false); // Prevent WiFi
  
  Serial.print("Connecting to WiFi...");
  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 20) { // 10 second timeout
    delay(500); 
    Serial.print("."); 
    retries++;
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\nFailed to connect. Auto-restarting...");
    ESP.restart(); // If connected, it will automatically restart
  }
  
  Serial.println("\nWiFi connected");

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  
  // 10MHz for temperature control and 24/7 stability
  config.xclk_freq_hz = 10000000; 
  config.pixel_format = PIXFORMAT_JPEG;
  
  if(psramFound()){
    config.frame_size = FRAMESIZE_VGA; 
    config.jpeg_quality = 25; // Resolution optimization to reduce network lag
    config.fb_count = 2;      
  } else {
    config.frame_size = FRAMESIZE_CIF;
    config.jpeg_quality = 25;
    config.fb_count = 1;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  sensor_t * s = esp_camera_sensor_get();
  s->set_vflip(s, 1);      
  s->set_hmirror(s, 1);    
  s->set_brightness(s, 1); 
  
  // Exposure and lighting optimization
  s->set_gain_ctrl(s, 1);       
  s->set_exposure_ctrl(s, 1);   
  s->set_awb_gain(s, 1);        
  s->set_bpc(s, 1);             
  s->set_wpc(s, 1);             
  s->set_raw_gma(s, 1);         
  s->set_lenc(s, 1);            
  
  startCameraServer();
  Serial.println("\n----------------------------------");
  Serial.print("Live Stream Ready! URL: http://");
  Serial.print(WiFi.localIP());
  Serial.println(":81/stream");
  Serial.println("----------------------------------");
}

void loop() {
  delay(10000);
}