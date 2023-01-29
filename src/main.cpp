#define CAMERA_MODEL_AI_THINKER // Has PSRAM
#include "camera_pins.h"
#include "ConexionWifi.h"
/*
// Ingrese la contraseña de la cuenta de conexión WIFI
const char *ssid = "NETLIFE-ALBARADO";       //"NETLIFE-ALBARADO"; //"Microcontroladores"
const char *password = "severino0985955238"; //"severino0985955238"; //"Raspii123"
// Ingrese la contraseña de la cuenta de conexión AP
const char *apssid = "ESP32-ONE";
const char *appassword = "12345678"; // La contraseña de AP debe tener al menos 8 caracteres o más
*/

#include "esp_camera.h"
#include <WiFi.h>
#include <esp32-hal-ledc.h>   // utilizado para controlar el servomotor
#include "soc/soc.h"          //Usado para fuente de alimentación inestable y reinicio
#include "soc/rtc_cntl_reg.h" //Usado para fuente de alimentación inestable y reinicio
////////////// Apartado del drive /////
#include <WiFiClientSecure.h>
#include "Base64.h"

// https://script.google.com/macros/s/AKfycbxSIXPYVZs3nI1aHPbbJVjCt1An894tXko-YCKpOy1Hazmwnuyq96uhuvqL-OwioLr0Nw/exec
String myScript = "/macros/s/AKfycbxSIXPYVZs3nI1aHPbbJVjCt1An894tXko-YCKpOy1Hazmwnuyq96uhuvqL-OwioLr0Nw/exec"; // Create your Google Apps Script and replace the "myScript" path.
String myLineNotifyToken = "myToken=**********";                                                               // Line Notify Token. You can set the value of xxxxxxxxxx empty if you don't want to send picture to Linenotify.
String myFoldername = "&myFoldername=";                                                                        //"&myFoldername=papeles";
String myFilename = "&myFilename=";                                                                            //"&myFilename=papeles.jpg";
String myImage = "&myFile=";                                                                                   //"&myFile=";

// biblioteca oficial
#include "esp_camera.h"      //video camera
#include "esp_http_server.h" //Biblioteca del servidor HTTP
#include "img_converters.h"  //Biblioteca de conversión de formato de imagen

// Apartado para los servos
#include <ESP32Servo.h>
#define AbriP_PIN 14
#define ClasifM_PIN 15

// Instanciamos los dos servos
Servo abrirServo;
Servo clasifServo;
int posServo;

String Feedback = ""; // El comando personalizado devuelve el mensaje del cliente

// valor de parámetro de comando personalizado
String Command = "";
String cmd = "";
String P1 = "";
String P2 = "";
String P3 = "";
String P4 = "";
String P5 = "";
String P6 = "";
String P7 = "";
String P8 = "";
String P9 = "";

// Personaliza el valor del estado de desmantelamiento del comando
byte ReceiveState = 0;
byte cmdState = 1;
byte strState = 1;
byte questionstate = 0;
byte equalstate = 0;
byte semicolonstate = 0;

typedef struct
{
  httpd_req_t *req;
  size_t len;
} jpg_chunking_t;

#define PART_BOUNDARY "123456789000000000000987654321"
static const char *_STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char *_STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char *_STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

httpd_handle_t stream_httpd = NULL;
httpd_handle_t camera_httpd = NULL;

static size_t jpg_encode_stream(void *arg, size_t index, const void *data, size_t len)
{
  jpg_chunking_t *j = (jpg_chunking_t *)arg;
  if (!index)
  {
    j->len = 0;
  }
  if (httpd_resp_send_chunk(j->req, (const char *)data, len) != ESP_OK)
  {
    return 0;
  }
  j->len += len;
  return len;
}

// captura de pantalla de la imagen
static esp_err_t capture_handler(httpd_req_t *req)
{
  camera_fb_t *fb = NULL;
  esp_err_t res = ESP_OK;

  fb = esp_camera_fb_get();
  if (!fb)
  {
    Serial.println("Camera capture failed");
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }

  httpd_resp_set_type(req, "image/jpeg");
  httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

  size_t fb_len = 0;
  if (fb->format == PIXFORMAT_JPEG)
  {
    fb_len = fb->len;
    res = httpd_resp_send(req, (const char *)fb->buf, fb->len);
  }
  else
  {
    jpg_chunking_t jchunk = {req, 0};
    res = frame2jpg_cb(fb, 80, jpg_encode_stream, &jchunk) ? ESP_OK : ESP_FAIL;
    httpd_resp_send_chunk(req, NULL, 0);
    fb_len = jchunk.len;
  }
  esp_camera_fb_return(fb);
  return res;
}

// vídeo transmitido en vivo
static esp_err_t stream_handler(httpd_req_t *req)
{
  camera_fb_t *fb = NULL;
  esp_err_t res = ESP_OK;
  size_t _jpg_buf_len = 0;
  uint8_t *_jpg_buf = NULL;
  char *part_buf[64];

  res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
  if (res != ESP_OK)
  {
    return res;
  }

  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

  while (true)
  {
    fb = esp_camera_fb_get();
    if (!fb)
    {
      Serial.println("Camera capture failed");
      res = ESP_FAIL;
    }
    else
    {
      if (fb->format != PIXFORMAT_JPEG)
      {
        bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
        esp_camera_fb_return(fb);
        fb = NULL;
        if (!jpeg_converted)
        {
          Serial.println("JPEG compression failed");
          res = ESP_FAIL;
        }
      }
      else
      {
        _jpg_buf_len = fb->len;
        _jpg_buf = fb->buf;
      }
    }

    if (res == ESP_OK)
    {
      res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
    }
    if (res == ESP_OK)
    {
      res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
    }
    if (res == ESP_OK)
    {
      size_t hlen = snprintf((char *)part_buf, 64, _STREAM_PART, _jpg_buf_len);
      res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
    }
    if (fb)
    {
      esp_camera_fb_return(fb);
      fb = NULL;
      _jpg_buf = NULL;
    }
    else if (_jpg_buf)
    {
      free(_jpg_buf);
      _jpg_buf = NULL;
    }
    if (res != ESP_OK)
    {
      break;
    }
  }

  return res;
}

// Mostrar el estado de los parámetros de video (debe devolver el formato json para cargar la configuración inicial)
static esp_err_t status_handler(httpd_req_t *req)
{
  static char json_response[1024];

  sensor_t *s = esp_camera_sensor_get();
  char *p = json_response;
  *p++ = '{';
  p += sprintf(p, "\"flash\":%d,", 0);
  p += sprintf(p, "\"framesize\":%u,", s->status.framesize);
  *p++ = '}';
  *p++ = 0;
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  return httpd_resp_send(req, json_response, strlen(json_response));
}
// Personalizar la página de inicio de la página web
static const char PROGMEM INDEX_HTML[] = R"rawliteral(<!DOCTYPE html>
<!DOCTYPE html>
<html>
    <head>
        <meta charset="utf-8">
        <meta name="viewport" content="width=device-width,initial-scale=1">
        <meta http-equiv="Access-Control-Allow-Headers" content="Origin, X-Requested-With, Content-Type, Accept">
        <meta http-equiv="Access-Control-Allow-Methods" content="GET,POST,PUT,DELETE,OPTIONS">
        <meta http-equiv="Access-Control-Allow-Origin" content="*">
        <title>Teachable Machine</title>
        <style>
          body{font-family:Arial,Helvetica,sans-serif;background:#181818;color:#EFEFEF;font-size:16px}h2{font-size:18px}section.main{display:flex}#menu,section.main{flex-direction:column}#menu{display:flex;flex-wrap:nowrap;min-width:340px;background:#363636;padding:8px;border-radius:4px;margin-top:-10px;margin-right:10px}#content{display:flex;flex-wrap:wrap;align-items:stretch}figure{padding:0;margin:0;-webkit-margin-before:0;margin-block-start:0;-webkit-margin-after:0;margin-block-end:0;-webkit-margin-start:0;margin-inline-start:0;-webkit-margin-end:0;margin-inline-end:0}figure img{display:block;width:100%;height:auto;border-radius:4px;margin-top:8px}@media (min-width: 800px) and (orientation:landscape){#content{display:flex;flex-wrap:nowrap;align-items:stretch}figure img{display:block;max-width:100%;max-height:calc(100vh - 40px);width:auto;height:auto}figure{padding:0;margin:0;-webkit-margin-before:0;margin-block-start:0;-webkit-margin-after:0;margin-block-end:0;-webkit-margin-start:0;margin-inline-start:0;-webkit-margin-end:0;margin-inline-end:0}}section#buttons{display:flex;flex-wrap:nowrap;justify-content:space-between}#nav-toggle{cursor:pointer;display:block}#nav-toggle-cb{outline:0;opacity:0;width:0;height:0}#nav-toggle-cb:checked+#menu{display:none}.input-group{display:flex;flex-wrap:nowrap;line-height:22px;margin:5px 0}.input-group>label{display:inline-block;padding-right:10px;min-width:47%}.input-group input,.input-group select{flex-grow:1}.range-max,.range-min{display:inline-block;padding:0 5px}button{display:block;margin:5px;padding:0 12px;border:0;line-height:28px;cursor:pointer;color:#fff;background:#ff3034;border-radius:5px;font-size:16px;outline:0}button:hover{background:#ff494d}button:active{background:#f21c21}button.disabled{cursor:default;background:#a0a0a0}input[type=range]{-webkit-appearance:none;width:100%;height:22px;background:#363636;cursor:pointer;margin:0}input[type=range]:focus{outline:0}input[type=range]::-webkit-slider-runnable-track{width:100%;height:2px;cursor:pointer;background:#EFEFEF;border-radius:0;border:0 solid #EFEFEF}input[type=range]::-webkit-slider-thumb{border:1px solid rgba(0,0,30,0);height:22px;width:22px;border-radius:50px;background:#ff3034;cursor:pointer;-webkit-appearance:none;margin-top:-11.5px}input[type=range]:focus::-webkit-slider-runnable-track{background:#EFEFEF}input[type=range]::-moz-range-track{width:100%;height:2px;cursor:pointer;background:#EFEFEF;border-radius:0;border:0 solid #EFEFEF}input[type=range]::-moz-range-thumb{border:1px solid rgba(0,0,30,0);height:22px;width:22px;border-radius:50px;background:#ff3034;cursor:pointer}input[type=range]::-ms-track{width:100%;height:2px;cursor:pointer;background:0 0;border-color:transparent;color:transparent}input[type=range]::-ms-fill-lower{background:#EFEFEF;border:0 solid #EFEFEF;border-radius:0}input[type=range]::-ms-fill-upper{background:#EFEFEF;border:0 solid #EFEFEF;border-radius:0}input[type=range]::-ms-thumb{border:1px solid rgba(0,0,30,0);height:22px;width:22px;border-radius:50px;background:#ff3034;cursor:pointer;height:2px}input[type=range]:focus::-ms-fill-lower{background:#EFEFEF}input[type=range]:focus::-ms-fill-upper{background:#363636}.switch{display:block;position:relative;line-height:22px;font-size:16px;height:22px}.switch input{outline:0;opacity:0;width:0;height:0}.slider{width:50px;height:22px;border-radius:22px;cursor:pointer;background-color:grey}.slider,.slider:before{display:inline-block;transition:.4s}.slider:before{position:relative;content:"";border-radius:50%;height:16px;width:16px;left:4px;top:3px;background-color:#fff}input:checked+.slider{background-color:#ff3034}input:checked+.slider:before{-webkit-transform:translateX(26px);transform:translateX(26px)}select{border:1px solid #363636;font-size:14px;height:22px;outline:0;border-radius:5px}.image-container{position:relative;min-width:160px}.close{position:absolute;right:5px;top:5px;background:#ff3034;width:16px;height:16px;border-radius:100px;color:#fff;text-align:center;line-height:18px;cursor:pointer}.hidden{display:none}
        </style>
        <script src="https:\/\/ajax.googleapis.com/ajax/libs/jquery/1.8.0/jquery.min.js"></script>
        <script src="https:\/\/cdn.jsdelivr.net/npm/@tensorflow/tfjs@1.3.1/dist/tf.min.js"></script>
        <script src="https:\/\/cdn.jsdelivr.net/npm/@teachablemachine/image@0.8/dist/teachablemachine-image.min.js"></script>  
    </head>
    <body>
        <section class="main">
            <figure>
              <div id="stream-container" class="image-container hidden" align= "center" >
                <div class="close" id="close-stream" style="display:none">×</div>
                <img id="stream" src=""  style="display:none" crossorigin="anonymous">
                <canvas id="canvas" width="0" height="0" style="border:2px solid white"></canvas>
              </div>
            </figure>         
            <section id="buttons">
                <table>
                <tr><td><button id="restart" onclick="try{fetch(document.location.origin+'/control?restart');}catch(e){}">Restart</button></td><td><button id="get-still" style="display:none">Get Still</button></td><td><button id="toggle-stream" style="display:none"></td></tr>
                </table>
            </section>        
            <div id="logo">
                <label for="nav-toggle-cb" id="nav-toggle">&#9776;&nbsp;&nbsp;Toggle settings</label>
            </div>
            <div id="content">
                <div id="sidebar">
                    <input type="checkbox" id="nav-toggle-cb">
                    <nav id="menu">
                        <div class="input-group">
                          <label for="kind">Kind</label>
                          <select id="kind">
                            <option value="image">image</option>
                            <option value="pose">pose</option>
                          </select>
                        </div>
                        <div class="input-group">
                          <label for="modelPath">Model Path</label>
                          <input type="text" id="modelPath" value="https://teachablemachine.withgoogle.com/models/keGx0EHOd/">
                        </div>
                        <div class="input-group">
                            <label for="btnModel"></label>
                            <button type="button" id="btnModel" onclick="LoadModel();">Start Recognition</button>
                        </div>                                              
                        <div class="input-group" id="flash-group">
                            <label for="flash">Flash</label>
                            <div class="range-min">0</div>
                            <input type="range" id="flash" min="0" max="255" value="0" class="default-action">
                            <div class="range-max">255</div>
                        </div>                    
                        <div class="input-group" id="framesize-group">
                            <label for="framesize">Resolution</label>
                            <select id="framesize" class="default-action">
                                <option value="10">UXGA(1600x1200)</option>
                                <option value="9">SXGA(1280x1024)</option>
                                <option value="8">XGA(1024x768)</option>
                                <option value="7">SVGA(800x600)</option>
                                <option value="6">VGA(640x480)</option>
                                <option value="5">CIF(400x296)</option>
                                <option value="4" selected="selected">QVGA(320x240)</option>
                                <option value="3">HQVGA(240x176)</option>
                                <option value="0">QQVGA(160x120)</option>
                            </select>
                        </div>
                        <div class="input-group" id="vflip-group">
                            <label for="vflip">V-Flip</label>
                            <div class="switch">
                                <input id="vflip" type="checkbox" class="default-action" checked="checked">
                                <label class="slider" for="vflip"></label>
                            </div>
                        </div>
                    </nav>
                </div>
            </div>
        </section>
        <br>
        <div id="result" style="color:red"><div>
        
        <script>
          document.addEventListener('DOMContentLoaded', function (event) {
            var baseHost = document.location.origin
            var streamUrl = baseHost + ':81'
            const hide = el => {
              el.classList.add('hidden')
            }
            const show = el => {
              el.classList.remove('hidden')
            }
            const disable = el => {
              el.classList.add('disabled')
              el.disabled = true
            }
            const enable = el => {
              el.classList.remove('disabled')
              el.disabled = false
            }
            const updateValue = (el, value, updateRemote) => {
              updateRemote = updateRemote == null ? true : updateRemote
              let initialValue
              if (el.type === 'checkbox') {
                initialValue = el.checked
                value = !!value
                el.checked = value
              } else {
                initialValue = el.value
                el.value = value
              }
              if (updateRemote && initialValue !== value) {
                updateConfig(el);
              }
            }
            function updateConfig (el) {
              let value
              switch (el.type) {
                case 'checkbox':
                  value = el.checked ? 1 : 0
                  break
                case 'range':
                case 'select-one':
                  value = el.value
                  break
                case 'button':
                case 'submit':
                  value = '1'
                  break
                default:
                  return
              }
              const query = `${baseHost}/control?var=${el.id}&val=${value}`
              fetch(query)
                .then(response => {
                  console.log(`request to ${query} finished, status: ${response.status}`)
                })
            }
            document
              .querySelectorAll('.close')
              .forEach(el => {
                el.onclick = () => {
                  hide(el.parentNode)
                }
              })
            // read initial values
            fetch(`${baseHost}/status`)
              .then(function (response) {
                return response.json()
              })
              .then(function (state) {
                document
                  .querySelectorAll('.default-action')
                  .forEach(el => {
                    updateValue(el, state[el.id], false)
                  })
              })
            const view = document.getElementById('stream')
            const viewContainer = document.getElementById('stream-container')
            const stillButton = document.getElementById('get-still')
            const streamButton = document.getElementById('toggle-stream')
            const closeButton = document.getElementById('close-stream')
            const stopStream = () => {
              //window.stop();
              view.src="";
              streamButton.innerHTML = 'Start Stream'
            }
            const startStream = () => {
              view.src = `${streamUrl}/stream`
              show(viewContainer)
              streamButton.innerHTML = 'Stop Stream'
            }
            // Attach actions to buttons
            stillButton.onclick = () => {
              stopStream()
              try{
                view.src = `${baseHost}/capture?_cb=${Date.now()}`
              }
              catch(e) {
                view.src = `${baseHost}/capture?_cb=${Date.now()}`  
              }
              show(viewContainer)
            }
            closeButton.onclick = () => {
              stopStream()
              hide(viewContainer)
            }
            streamButton.onclick = () => {
              const streamEnabled = streamButton.innerHTML === 'Stop Stream'
              if (streamEnabled) {
                stopStream()
              } else {
                startStream()
              }
            }
            // Attach default on change action
            document
              .querySelectorAll('.default-action')
              .forEach(el => {
                el.onchange = () => updateConfig(el)
              })
          })
        </script>
          
        <script>
        var getStill = document.getElementById('get-still');
        var ShowImage = document.getElementById('stream');
        var canvas = document.getElementById("canvas");
        var context = canvas.getContext("2d");  
        var modelPath = document.getElementById('modelPath');
        var result = document.getElementById('result');
        var kind = document.getElementById('kind');        
        let Model;
        
        async function LoadModel() {
          if (modelPath.value=="") {
            result.innerHTML = "Please input model path.";
            return;
          }
    
          result.innerHTML = "Please wait for loading model.";
          
          const URL = modelPath.value;
          const modelURL = URL + "model.json";
          const metadataURL = URL + "metadata.json";
          if (kind.value=="image") {
            Model = await tmImage.load(modelURL, metadataURL);
          }
          
          maxPredictions = Model.getTotalClasses();
          result.innerHTML = "";
    
          getStill.style.display = "block";
          getStill.click();
        }
    
        async function predict() {
          var data = "";
          var maxClassName = "";
          var maxProbability = "";
    
          canvas.setAttribute("width", ShowImage.width);
          canvas.setAttribute("height", ShowImage.height);
          context.drawImage(ShowImage, 0, 0, ShowImage.width, ShowImage.height); 
           
          if (kind.value=="image")
            var prediction = await Model.predict(canvas);
          else if (kind.value=="pose") {
            var { pose, posenetOutput } = await Model.estimatePose(canvas);
            var prediction = await Model.predict(posenetOutput);
          }
    
          if (maxPredictions>0) {
            for (let i = 0; i < maxPredictions; i++) {
              if (i==0) {
              maxClassName = prediction[i].className;
              maxProbability = prediction[i].probability;
              }
              else {
              if (prediction[i].probability>maxProbability) {
                maxClassName = prediction[i].className;
                maxProbability = prediction[i].probability;
              }
              }
              data += prediction[i].className + "," + prediction[i].probability.toFixed(2) + "<br>";
            }
            result.innerHTML = data;
            result.innerHTML += "<br>Result: " + maxClassName + "," + maxProbability; 

            $.ajax({url: document.location.origin+'/control?serial='+maxClassName+";"+maxProbability+';stop', async: false});
          }
          else
            result.innerHTML = "Unrecognizable";
            
          getStill.click();
        }
    
        ShowImage.onload = function (event) {
          if (Model) {
          try { 
            document.createEvent("TouchEvent");
            setTimeout(function(){predict();},250);
          }
          catch(e) { 
            predict();
          } 
          }
        }    
        </script>
    </body>
</html>

)rawliteral";

// Página de inicio http://192.168.xxx.xxx
static esp_err_t index_handler(httpd_req_t *req)
{
  /*
   httpd_resp_set_type(req, "text/html");
  sensor_t * s = esp_camera_sensor_get();
    if (s->id.PID == OV2640_PID) {
        return httpd_resp_send(req, (const char *)INDEX_HTML, strlen(INDEX_HTML));
    }
    return httpd_resp_send(req, (const char *)INDEX_HTML, strlen(INDEX_HTML));
}
  */

  httpd_resp_set_type(req, "text/html");
  return httpd_resp_send(req, (const char *)INDEX_HTML, strlen(INDEX_HTML));
}

// El comando personalizado desensambla la cadena de parámetros y la coloca en una variable
void getCommand(char c)
{
  if (c == '?')
    ReceiveState = 1;
  if ((c == ' ') || (c == '\r') || (c == '\n'))
    ReceiveState = 0;

  if (ReceiveState == 1)
  {
    Command = Command + String(c);

    if (c == '=')
      cmdState = 0;
    if (c == ';')
      strState++;

    if ((cmdState == 1) && ((c != '?') || (questionstate == 1)))
      cmd = cmd + String(c);
    if ((cmdState == 0) && (strState == 1) && ((c != '=') || (equalstate == 1)))
      P1 = P1 + String(c);
    if ((cmdState == 0) && (strState == 2) && (c != ';'))
      P2 = P2 + String(c);
    if ((cmdState == 0) && (strState == 3) && (c != ';'))
      P3 = P3 + String(c);
    if ((cmdState == 0) && (strState == 4) && (c != ';'))
      P4 = P4 + String(c);
    if ((cmdState == 0) && (strState == 5) && (c != ';'))
      P5 = P5 + String(c);
    if ((cmdState == 0) && (strState == 6) && (c != ';'))
      P6 = P6 + String(c);
    if ((cmdState == 0) && (strState == 7) && (c != ';'))
      P7 = P7 + String(c);
    if ((cmdState == 0) && (strState == 8) && (c != ';'))
      P8 = P8 + String(c);
    if ((cmdState == 0) && (strState >= 9) && ((c != ';') || (semicolonstate == 1)))
      P9 = P9 + String(c);

    if (c == '?')
      questionstate = 1;
    if (c == '=')
      equalstate = 1;
    if ((strState >= 9) && (c == ';'))
      semicolonstate = 1;
  }
}

// Control de parámetro de comando
/*
static esp_err_t cmd_handler(httpd_req_t *req)
{
  char *buf; // Acceda a la cadena de parámetros después de la URL
  size_t buf_len;
  char variable[128] = {
      0,
  }; // Acceder al valor de var del parámetro
  char value[128] = {
      0,
  }; // Acceder al valor del parámetro val
  String myCmd = "";

  buf_len = httpd_req_get_url_query_len(req) + 1;
  if (buf_len > 1)
  {
    buf = (char *)malloc(buf_len);
    if (!buf)
    {
      httpd_resp_send_500(req);
      return ESP_FAIL;
    }
    if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK)
    {
      if (httpd_query_key_value(buf, "var", variable, sizeof(variable)) == ESP_OK &&
          httpd_query_key_value(buf, "val", value, sizeof(value)) == ESP_OK)
      {
      }
      else
      {
        myCmd = String(buf); // Si el formato no oficial no contiene var, val, es un formato de instrucción personalizado
      }
    }
    free(buf);
  }
  else
  {
    httpd_resp_send_404(req);
    return ESP_FAIL;
  }

  Feedback = "";
  Command = "";
  cmd = "";
  P1 = "";
  P2 = "";
  P3 = "";
  P4 = "";
  P5 = "";
  P6 = "";
  P7 = "";
  P8 = "";
  P9 = "";
  ReceiveState = 0, cmdState = 1, strState = 1, questionstate = 0, equalstate = 0, semicolonstate = 0;
  if (myCmd.length() > 0)
  {
    myCmd = "?" + myCmd; // La cadena de parámetros después de que la URL se convierte en un formato de comando personalizado
    for (int i = 0; i < myCmd.length(); i++)
    {
      getCommand(char(myCmd.charAt(i))); // Desensamblar la cadena de parámetros del comando personalizado
    }
  }

  if (cmd.length() > 0)
  {
    Serial.println("");
    // Serial.println("Command: "+Command);
    Serial.println("cmd= " + cmd + " ,P1= " + P1 + " ,P2= " + P2 + " ,P3= " + P3 + " ,P4= " + P4 + " ,P5= " + P5 + " ,P6= " + P6 + " ,P7= " + P7 + " ,P8= " + P8 + " ,P9= " + P9);
    Serial.println("");

    // Bloque de comando personalizado http://192.168.xxx.xxx/control?cmd=P1;P2;P3;P4;P5;P6;P7;P8;P9

    if (cmd == "ip")
    { // Consulta APIP, STAIP
      Feedback = "AP IP: " + WiFi.softAPIP().toString();
      Feedback += "<br>";
      Feedback += "STA IP: " + WiFi.localIP().toString();
    }
    else if (cmd == "mac")
    { // Consultar la dirección MAC
      Feedback = "STA MAC: " + WiFi.macAddress();
    }
    else if (cmd == "restart")
    {
      ESP.restart();
    }

#if defined(CAMERA_MODEL_AI_THINKER)
    else if (cmd == "digitalwrite")
    {
      ledcDetachPin(P1.toInt());
      pinMode(P1.toInt(), OUTPUT);
      digitalWrite(P1.toInt(), P2.toInt());
    }
    else if (cmd == "digitalread")
    {
      Feedback = String(digitalRead(P1.toInt()));
    }
    else if (cmd == "analogwrite")
    {
      if (P1 == "4")
      {
        ledcAttachPin(4, 4);
        ledcSetup(4, 5000, 8);
        ledcWrite(4, P2.toInt());
      }
      else
      {
        ledcAttachPin(P1.toInt(), 9);
        ledcSetup(9, 5000, 8);
        ledcWrite(9, P2.toInt());
      }
    }
    else if (cmd == "analogread")
    {
      Feedback = String(analogRead(P1.toInt()));
    }
    else if (cmd == "touchread")
    {
      Feedback = String(touchRead(P1.toInt()));
    }
#endif
    else if (cmd == "resetwifi")
    { // restablecer la conexión de red
      for (int i = 0; i < 2; i++)
      {
        WiFi.begin(P1.c_str(), P2.c_str());
        Serial.print("Connecting to ");
        Serial.println(P1);
        long int StartTime = millis();
        while (WiFi.status() != WL_CONNECTED)
        {
          delay(500);
          if ((StartTime + 5000) < millis())
            break;
        }
        Serial.println("");
        Serial.println("STAIP: " + WiFi.localIP().toString());
        Feedback = "STAIP: " + WiFi.localIP().toString();

        if (WiFi.status() == WL_CONNECTED)
        {
          WiFi.softAP((WiFi.localIP().toString() + "_" + P1).c_str(), P2.c_str());
#if defined(CAMERA_MODEL_AI_THINKER)
          for (int i = 0; i < 2; i++)
          { // Si no puede conectarse a WIFI, configure el flash para que parpadee lentamente
            ledcWrite(4, 10);
            delay(300);
            ledcWrite(4, 0);
            delay(300);
          }
#endif
          break;
        }
      }
    }
#if defined(CAMERA_MODEL_AI_THINKER)
    else if (cmd == "flash")
    { // Controlar el flash incorporado
      ledcAttachPin(4, 4);
      ledcSetup(4, 5000, 8);
      int val = P1.toInt();
      ledcWrite(4, val);
    }
#endif
    else if (cmd == "serial")
    {
      if (P1 != "" & P1 != "stop")
        Serial.println(P1);
      if (P2 != "" & P2 != "stop")
        Serial.println(P2);
      Serial.println();
    }
    else
    {
      Feedback = "Command is not defined";
    }

    if (Feedback == "")
    {
      Feedback = Command; // Si no se establecen datos de retorno, devuelva el valor del comando
    }
    const char *resp = Feedback.c_str();
    httpd_resp_set_type(req, "text/html");                       // Establecer el formato de datos de retorno
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*"); // Permitir lectura entre dominios
    return httpd_resp_send(req, resp, strlen(resp));
  }
  else
  {
    // Bloque de comandos oficial, también puede personalizar los comandos aquí http://192.168.xxx.xxx/control?var=xxx&val=xxx
    int val = atoi(value);
    sensor_t *s = esp_camera_sensor_get();
    int res = 0;

    if (!strcmp(variable, "framesize"))
    {
      if (s->pixformat == PIXFORMAT_JPEG)
        res = s->set_framesize(s, (framesize_t)val);
    }
    else if (!strcmp(variable, "quality"))
      res = s->set_quality(s, val);
    else if (!strcmp(variable, "contrast"))
      res = s->set_contrast(s, val);
    else if (!strcmp(variable, "brightness"))
      res = s->set_brightness(s, val);
    else if (!strcmp(variable, "hmirror"))
      res = s->set_hmirror(s, val);
    else if (!strcmp(variable, "vflip"))
      res = s->set_vflip(s, val);
#if defined(CAMERA_MODEL_AI_THINKER)
    else if (!strcmp(variable, "flash"))
    {
      ledcAttachPin(4, 4);
      ledcSetup(4, 5000, 8);
      ledcWrite(4, val);
    }
#endif
    else
    {
      res = -1;
    }

    if (res)
    {
      return httpd_resp_send_500(req);
    }

    if (buf)
    {
      Feedback = String(buf);
      const char *resp = Feedback.c_str();
      httpd_resp_set_type(req, "text/html");
      httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
      return httpd_resp_send(req, resp, strlen(resp)); // devuelve cadena de parámetro
    }
    else
    {
      httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
      return httpd_resp_send(req, NULL, 0);
    }
  }
}
*/

// Apartado para el drive

String urlencode(String str)
{
  String encodedString = "";
  char c;
  char code0;
  char code1;
  char code2;
  for (int i = 0; i < str.length(); i++)
  {
    c = str.charAt(i);
    if (c == ' ')
    {
      encodedString += '+';
    }
    else if (isalnum(c))
    {
      encodedString += c;
    }
    else
    {
      code1 = (c & 0xf) + '0';
      if ((c & 0xf) > 9)
      {
        code1 = (c & 0xf) - 10 + 'A';
      }
      c = (c >> 4) & 0xf;
      code0 = c + '0';
      if (c > 9)
      {
        code0 = c - 10 + 'A';
      }
      code2 = '\0';
      encodedString += '%';
      encodedString += code0;
      encodedString += code1;
    }
    yield();
  }
  return encodedString;
}

// Captura de imagen y enviar al drive
String SendCapturedImage(String myFoldername, String myFilename)
{
  const char *myDomain = "script.google.com";
  String getAll = "", getBody = "";

  camera_fb_t *fb = NULL;
  fb = esp_camera_fb_get();
  if (!fb)
  {
    Serial.println("Camera capture failed");
    delay(1000);
    ESP.restart();
    return "Camera capture failed";
  }

  Serial.println("Connect to " + String(myDomain));
  WiFiClientSecure client_tcp;
  client_tcp.setInsecure(); // run version 1.0.5 or above

  if (client_tcp.connect(myDomain, 443))
  {
    Serial.println("Connection successful");

    char *input = (char *)fb->buf;
    char output[base64_enc_len(3)];
    String imageFile = "data:image/jpeg;base64,";
    for (int i = 0; i < fb->len; i++)
    {
      base64_encode(output, (input++), 3);
      if (i % 3 == 0)
        imageFile += urlencode(String(output));
    }
    String Data = myLineNotifyToken + myFoldername + myFilename + myImage;

    client_tcp.println("POST " + myScript + " HTTP/1.1");
    client_tcp.println("Host: " + String(myDomain));
    client_tcp.println("Content-Length: " + String(Data.length() + imageFile.length()));
    client_tcp.println("Content-Type: application/x-www-form-urlencoded");
    client_tcp.println("Connection: keep-alive");
    client_tcp.println();

    client_tcp.print(Data);
    int Index;
    for (Index = 0; Index < imageFile.length(); Index = Index + 1000)
    {
      client_tcp.print(imageFile.substring(Index, Index + 1000));
    }
    esp_camera_fb_return(fb);

    int waitTime = 3000; // timeout 10 seconds
    long startTime = millis();
    boolean state = false;

    while ((startTime + waitTime) > millis())
    {
      Serial.print(".");
      delay(100);
      while (client_tcp.available())
      {
        char c = client_tcp.read();
        if (state == true)
          getBody += String(c);
        if (c == '\n')
        {
          if (getAll.length() == 0)
            state = true;
          getAll = "";
        }
        else if (c != '\r')
          getAll += String(c);
        startTime = millis();
      }
      if (getBody.length() > 0)
        break;
    }
    client_tcp.stop();
    Serial.println(getBody);
  }
  else
  {
    getBody = "Connected to " + String(myDomain) + " failed.";
    Serial.println("Connected to " + String(myDomain) + " failed.");
  }

  return getBody;
}

// variable que permite cambiar el estado de verificacion del material
int mat_pla = 0;
int mat_pap = 0;
int mat_met = 0;
int cont = 0;
// Control de parámetro de comando
static esp_err_t cmd_handler(httpd_req_t *req)
{
  char *buf; // Acceda a la cadena de parámetros después de la URL
  size_t buf_len;
  char variable[128] = {
      0,
  }; // Acceder al valor de var del parámetro
  char value[128] = {
      0,
  }; // Acceder al valor del parámetro val
  String myCmd = "";

  buf_len = httpd_req_get_url_query_len(req) + 1;
  if (buf_len > 1)
  {
    buf = (char *)malloc(buf_len);
    if (!buf)
    {
      httpd_resp_send_500(req);
      return ESP_FAIL;
    }
    if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK)
    {
      if (httpd_query_key_value(buf, "var", variable, sizeof(variable)) == ESP_OK &&
          httpd_query_key_value(buf, "val", value, sizeof(value)) == ESP_OK)
      {
      }
      else
      {
        myCmd = String(buf); // Si el formato no oficial no contiene var, val, es un formato de instrucción personalizado
      }
    }
    free(buf);
  }
  else
  {
    httpd_resp_send_404(req);
    return ESP_FAIL;
  }

  Feedback = "";
  Command = "";
  cmd = "";
  P1 = "";
  P2 = "";
  P3 = "";
  P4 = "";
  P5 = "";
  P6 = "";
  P7 = "";
  P8 = "";
  P9 = "";
  ReceiveState = 0, cmdState = 1, strState = 1, questionstate = 0, equalstate = 0, semicolonstate = 0;
  if (myCmd.length() > 0)
  {
    myCmd = "?" + myCmd; // La cadena de parámetros después de que la URL se convierte en un formato de comando personalizado
    for (int i = 0; i < myCmd.length(); i++)
    {
      getCommand(char(myCmd.charAt(i))); // Desensamblar la cadena de parámetros del comando personalizado
    }
  }
  /*
  if (cmd.length() > 0)
  {

    // Bloque de comando personalizado http://192.168.xxx.xxx/control?cmd=P1;P2;P3;P4;P5;P6;P7;P8;P9
    if (P1 != "Nada" && P2.toDouble() >= 0.50)
    {
      delay(1000);
      lcd.setCursor(0, 0); // Mostrar el resultado del reconocimiento de imagen en la pantalla
      lcd.print("Image result:   ");
      // Ubicamos el objeto en su respectiva posicion
      if ((P1 == "Plastico" && P2.toDouble() >= 0.60) && (mat_pla == 0) && cont == 2)
      {
        lcd.setCursor(0, 1);
        lcd.print(P1 + "                ");
        Serial.println("Entre a plastico");
        // abrirServo.write(0);
        clasifServo.write(70);
        delay(500);
        posServo = clasifServo.read(); // Leemos si se ha movido el servo para proceder abrir la tapa
        if (posServo > 0)
        {
          // Abrimos la tapa por 1 sg y medio, y volvemos a cerrarla
          abrirServo.write(90);
          delay(1500);
          abrirServo.write(0);
        }
        delay(500);
        clasifServo.write(0);
      }
      else if ((P1 == "Papeles" && P2.toDouble() >= 0.60) && (mat_pap == 0) && cont == 2)
      {
        lcd.setCursor(0, 1);
        lcd.print(P1 + "                ");
        Serial.println("Entre a papales");
        clasifServo.write(3);
        delay(500);
        posServo = clasifServo.read(); // Leemos si se ha movido el servo para proceder abrir la tapa
        if (posServo > 0)
        {
          // Abrimos la tapa por 1 sg y medio, y volvemos a cerrarla
          abrirServo.write(90);
          delay(1500);
          abrirServo.write(0);
        }
        delay(500);
        clasifServo.write(0);
        // String myFoldername1 = myFoldername + "papeles";
        // String myFilename1 = myFilename + "papeles" + ".jpg";
        // SendCapturedImage(myFoldername1, myFilename1);
        // delay(2000);
      }
      else if ((P1 == "Metal" && P2.toDouble() >= 0.60) && (mat_met == 0) && cont == 2)
      {
        lcd.setCursor(0, 1);
        lcd.print(P1 + "                ");
        Serial.print("Entre a metal");
        clasifServo.write(180);
        delay(500);
        posServo = clasifServo.read(); // Leemos si se ha movido el servo para proceder abrir la tapa
        if (posServo > 0)
        {
          // Abrimos la tapa por 1 sg y medio, y volvemos a cerrarla
          abrirServo.write(90);
          delay(1500);
          abrirServo.write(0);
        }
        delay(500);
        clasifServo.write(0);
        // String myFoldername1 = myFoldername + "metales";
        // String myFilename1 = myFilename + "metales" + ".jpg";
        // SendCapturedImage(myFoldername1, myFilename1);
        // delay(2000);
      }

      lcd.clear();

      mat_pap = !mat_pap;
      mat_pla = !mat_pla;
      mat_met = !mat_met;
      cont += 1;
      if (cont == 3)
      {
        cont = 0;
      }
      Serial.println("hay objeto identificado: " + P1 + " " + P2.toDouble() + "cont: " + cont);
    }
    else
    {
      Serial.println("No hay objeto: " + P1 + " " + P2.toDouble());
      // Seteamos nuevamente los valores para que continue leyendo los datos
      lcd.setCursor(0, 0); // Mostrar el resultado del reconocimiento de imagen en la pantalla
      lcd.print("Objeto no ...");
      lcd.setCursor(0, 1); // Mostrar el resultado del reconocimiento de imagen en la pantalla
      lcd.print("Identificado..");
      mat_pla = 0;
      mat_pap = 0;
      mat_met = 0;
      cont = 0;
    }
    */
  if (cmd == "restart")
  {
    ESP.restart();
    //}
  }
  else
  {
    // Bloque de comandos oficial, también puede personalizar los comandos aquí http://192.168.xxx.xxx/control?var=xxx&val=xxx
    int val = atoi(value);
    sensor_t *s = esp_camera_sensor_get();
    int res = 0;

    if (!strcmp(variable, "framesize"))
    {
      if (s->pixformat == PIXFORMAT_JPEG)
        res = s->set_framesize(s, (framesize_t)val);
    }
    else if (!strcmp(variable, "quality"))
      res = s->set_quality(s, val);
    else if (!strcmp(variable, "contrast"))
      res = s->set_contrast(s, val);
    else if (!strcmp(variable, "brightness"))
      res = s->set_brightness(s, val);
    else if (!strcmp(variable, "hmirror"))
      res = s->set_hmirror(s, val);
    else if (!strcmp(variable, "vflip"))
      res = s->set_vflip(s, val);
#if defined(CAMERA_MODEL_AI_THINKER)
    else if (!strcmp(variable, "flash"))
    {
      ledcAttachPin(4, 4);
      ledcSetup(4, 5000, 8);
      ledcWrite(4, val);
    }
#endif
    else
    {
      res = -1;
    }

    if (res)
    {
      return httpd_resp_send_500(req);
    }

    if (buf)
    {
      Feedback = String(buf);
      const char *resp = Feedback.c_str();
      httpd_resp_set_type(req, "text/html");
      httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
      return httpd_resp_send(req, resp, strlen(resp)); // devuelve cadena de parámetro
    }
    else
    {
      httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
      return httpd_resp_send(req, NULL, 0);
    }
  }
}

// Función a ejecutar para la ruta URL personalizada
void startCameraServer()
{
  httpd_config_t config = HTTPD_DEFAULT_CONFIG(); // El puerto del servidor se puede configurar en HTTPD_DEFAULT_CONFIG()

  // http://192.168.xxx.xxx/
  httpd_uri_t index_uri = {
      .uri = "/",
      .method = HTTP_GET,
      .handler = index_handler,
      .user_ctx = NULL};

  // http://192.168.xxx.xxx/status
  httpd_uri_t status_uri = {
      .uri = "/status",
      .method = HTTP_GET,
      .handler = status_handler,
      .user_ctx = NULL};

  // http://192.168.xxx.xxx/control
  httpd_uri_t cmd_uri = {
      .uri = "/control",
      .method = HTTP_GET,
      .handler = cmd_handler,
      .user_ctx = NULL};

  // http://192.168.xxx.xxx/capture
  httpd_uri_t capture_uri = {
      .uri = "/capture",
      .method = HTTP_GET,
      .handler = capture_handler,
      .user_ctx = NULL};

  // http://192.168.xxx.xxx:81/stream
  httpd_uri_t stream_uri = {
      .uri = "/stream",
      .method = HTTP_GET,
      .handler = stream_handler,
      .user_ctx = NULL};

  Serial.printf("Starting web server on port: '%d'\n", config.server_port); // Server Port
  if (httpd_start(&camera_httpd, &config) == ESP_OK)
  {
    // Registrar la función ejecutada correspondiente a la ruta URL personalizada
    httpd_register_uri_handler(camera_httpd, &index_uri);
    httpd_register_uri_handler(camera_httpd, &cmd_uri);
    httpd_register_uri_handler(camera_httpd, &status_uri);
    httpd_register_uri_handler(camera_httpd, &capture_uri);
  }

  config.server_port += 1; // Puerto de transmisión
  config.ctrl_port += 1;   // El puerto UDP
  Serial.printf("Starting stream server on port: '%d'\n", config.server_port);
  if (httpd_start(&stream_httpd, &config) == ESP_OK)
  {
    httpd_register_uri_handler(stream_httpd, &stream_uri);
  }
}

void setup()
{
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); // La configuración de reiniciar cuando la alimentación está apagada y es inestable
  Serial.begin(115200);
  ///////////////////////
  ConectarWifi();
  ///////////////Ajustes para el servo////////////////////////
  // Configuramos los servos a sis respectivos pines y sus tiempos
  abrirServo.attach(AbriP_PIN);
  clasifServo.attach(ClasifM_PIN);
  // Movemos los dos servos a 0 grados
  abrirServo.write(0);
  clasifServo.write(0);
  // Ajustes de configuración de vídeo:  https://github.com/espressif/esp32-camera/blob/master/driver/include/esp_camera.h
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
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  if (psramFound())
  { // Si hay IC de memoria PSRAM (Pseudo SRAM)
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  }
  else
  {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  // Inicializacion de la ESP32-CAM (ídeo)
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK)
  {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  // Puede personalizar el tamaño predeterminado del cuadro de video (tamaño de resolución)
  sensor_t *s = esp_camera_sensor_get();
  // los sensores iniciales están volteados verticalmente y los colores están un poco saturados
  if (s->id.PID == OV2640_PID)
  { // OV3660_PID
    // s->set_vflip(s, 1);                 // darle la vuelta
    s->set_framesize(s, FRAMESIZE_QVGA); // Resolucion de la pantalla
  }
  // tamaño de cuadro desplegable para una mayor velocidad de cuadro inicial
  /*
  UXGA(1600x1200), SXGA(1280x1024), XGA(1024x768)
  SVGA(800x600), VGA(640x480), CIF(400x296), QVGA(320x240)
  HQVGA(240x176), QQVGA(160x120), QXGA(2048x1564 for OV3660)
  */

  ledcWrite(canal, 0);
  startCameraServer();
  /*


  */
}

String material[3] = {"Papeles", "Plastico", "Metal"};
void loop()
{
  if (cmd.length() > 0)
  {

    // Bloque de comando personalizado http://192.168.xxx.xxx/control?cmd=P1;P2;P3;P4;P5;P6;P7;P8;P9
    if (P1 != "Nada" && P2.toDouble() >= 0.50)
    {
      delay(1000);
      lcd.setCursor(0, 0); // Mostrar el resultado del reconocimiento de imagen en la pantalla
      lcd.print("Image result:   ");
      // Ubicamos el objeto en su respectiva posicion
      if ((P1 == "Plastico" && P2.toDouble() >= 0.60) && (mat_pla == 0) && cont == 2)
      {
        String myFoldername1 = myFoldername + "Plastico";
        String myFilename1 = myFilename + "Plastico" + ".jpg";
        SendCapturedImage(myFoldername1, myFilename1);
        lcd.setCursor(0, 1);
        lcd.print(P1 + "                ");
        Serial.println("Entre a plastico");
        // abrirServo.write(0);
        clasifServo.write(70);
        delay(500);
        posServo = clasifServo.read(); // Leemos si se ha movido el servo para proceder abrir la tapa
        if (posServo > 0)
        {
          // Abrimos la tapa por 1 sg y medio, y volvemos a cerrarla
          abrirServo.write(90);
          delay(1500);
          abrirServo.write(0);
        }
        delay(500);
        clasifServo.write(0);
      }
      else if ((P1 == "Papeles" && P2.toDouble() >= 0.60) && (mat_pap == 0) && cont == 2)
      {
        String myFoldername1 = myFoldername + "Papeles";
        String myFilename1 = myFilename + "Papeles" + ".jpg";
        SendCapturedImage(myFoldername1, myFilename1);
        lcd.setCursor(0, 1);
        lcd.print(P1 + "                ");
        Serial.println("Entre a papales");
        clasifServo.write(3);
        delay(500);
        posServo = clasifServo.read(); // Leemos si se ha movido el servo para proceder abrir la tapa
        if (posServo > 0)
        {
          // Abrimos la tapa por 1 sg y medio, y volvemos a cerrarla
          abrirServo.write(90);
          delay(1500);
          abrirServo.write(0);
        }
        delay(500);
        clasifServo.write(0);
      }
      else if ((P1 == "Metal" && P2.toDouble() >= 0.60) && (mat_met == 0) && cont == 2)
      {
        String myFoldername1 = myFoldername + "Metal";
        String myFilename1 = myFilename + "Metal" + ".jpg";
        SendCapturedImage(myFoldername1, myFilename1);
        lcd.setCursor(0, 1);
        lcd.print(P1 + "                ");
        Serial.print("Entre a metal");
        clasifServo.write(180);
        delay(500);
        posServo = clasifServo.read(); // Leemos si se ha movido el servo para proceder abrir la tapa
        if (posServo > 0)
        {
          // Abrimos la tapa por 1 sg y medio, y volvemos a cerrarla
          abrirServo.write(90);
          delay(1500);
          abrirServo.write(0);
        }
        delay(500);
        clasifServo.write(0);
      }

      lcd.clear();

      mat_pap = !mat_pap;
      mat_pla = !mat_pla;
      mat_met = !mat_met;
      cont += 1;
      if (cont == 3)
      {
        cont = 0;
      }
      Serial.println("hay objeto identificado: " + P1 + " " + P2.toDouble() + "cont: " + cont);
    }
    else
    {
      Serial.println("No hay objeto: " + P1 + " " + P2.toDouble());
      // Seteamos nuevamente los valores para que continue leyendo los datos
      lcd.setCursor(0, 0); // Mostrar el resultado del reconocimiento de imagen en la pantalla
      lcd.print("Objeto no ...");
      lcd.setCursor(0, 1); // Mostrar el resultado del reconocimiento de imagen en la pantalla
      lcd.print("Identificado..");
      mat_pla = 0;
      mat_pap = 0;
      mat_met = 0;
      cont = 0;
    }
   
  }
}
