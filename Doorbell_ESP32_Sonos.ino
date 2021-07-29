#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <esp_task_wdt.h>


//1 seconds WDT
#define WDT_TIMEOUT 1

const String DebugVersion = "21.1.28";


const char* host = "ESP32Doorbell";
const char* ssid = "Deuterium IoT";
const char* password = "InternetOfTrash";

const char* sonosHost = "10.0.0.10";
const int sonosPort = 5005;
const String sonosAction = "Keuken/clipall/Dogs-Barking.mp3/60";

const int ampPin = 35;
float multiplier = 0.185;

bool bellTrigger = false;
float bellThreshold = 5.5;

int tri50 = 0;
int tri52 = 0;
int tri54 = 0;
int tri56 = 0;
int tri58 = 0;
int tri60 = 0;
int tri61 = 0;
int tri62 = 0;
int tri63 = 0;
int tri64 = 0;
int tri65 = 0;
int triHigh = 0;

int last = millis();

WebServer server(80);

/*
   Login page
*/
const char* loginIndex =
  "<form name='loginForm'>"
  "<table width='20%' bgcolor='A09F9F' align='center'>"
  "<tr>"
  "<td colspan=2>"
  "<center><font size=4><b>ESP32 Login Page</b></font></center>"
  "<br>"
  "</td>"
  "<br>"
  "<br>"
  "</tr>"
  "<td>Username:</td>"
  "<td><input type='text' size=25 name='userid'><br></td>"
  "</tr>"
  "<br>"
  "<br>"
  "<tr>"
  "<td>Password:</td>"
  "<td><input type='Password' size=25 name='pwd'><br></td>"
  "<br>"
  "<br>"
  "</tr>"
  "<tr>"
  "<td><input type='submit' onclick='check(this.form)' value='Login'></td>"
  "</tr>"
  "</table>"
  "</form>"
  "<script>"
  "function check(form)"
  "{"
  "if(form.userid.value=='admin' && form.pwd.value=='admin')"
  "{"
  "window.open('/serverIndex')"
  "}"
  "else"
  "{"
  " alert('Error Password or Username')/*displays error message*/"
  "}"
  "}"
  "</script>";


/*
   Server Index Page
*/
const char* serverIndex =
  "<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js'></script>"
  "<form method='POST' action='#' enctype='multipart/form-data' id='upload_form'>"
  "<input type='file' name='update'>"
  "<input type='submit' value='Update'>"
  "</form>"
  "<div id='prg'>progress: 0%</div>"
  "<script>"
  "$('form').submit(function(e){"
  "e.preventDefault();"
  "var form = $('#upload_form')[0];"
  "var data = new FormData(form);"
  " $.ajax({"
  "url: '/update',"
  "type: 'POST',"
  "data: data,"
  "contentType: false,"
  "processData:false,"
  "xhr: function() {"
  "var xhr = new window.XMLHttpRequest();"
  "xhr.upload.addEventListener('progress', function(evt) {"
  "if (evt.lengthComputable) {"
  "var per = evt.loaded / evt.total;"
  "$('#prg').html('progress: ' + Math.round(per*100) + '%');"
  "}"
  "}, false);"
  "return xhr;"
  "},"
  "success:function(d, s) {"
  "console.log('success!')"
  "},"
  "error: function (a, b, c) {"
  "}"
  "});"
  "});"
  "</script>";


/*
   setup function
*/
void setup(void) {
  Serial.begin(115200);

  // ESP Watchdog
  Serial.println("Configuring WDT...");
  esp_task_wdt_init(WDT_TIMEOUT, true); //enable panic so ESP32 restarts
  esp_task_wdt_add(NULL); //add current thread to WDT watch

  // Connect to WiFi network
  WiFi.begin(ssid, password);
  Serial.println("");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  /*use mdns for host name resolution*/
  if (!MDNS.begin(host)) { //http://esp32.local
    Serial.println("Error setting up MDNS responder!");
    while (1) {
      delay(1000);
    }
  }
  Serial.println("mDNS responder started");
  /*return index page which is stored in serverIndex */
  server.on("/", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", loginIndex);
  });
  server.on("/serverIndex", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", serverIndex);
  });
  /*handling uploading firmware file */
  server.on("/update", HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    ESP.restart();
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.printf("Update: %s\n", upload.filename.c_str());
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { //start with max available size
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      /* flashing firmware to ESP*/
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) { //true to set the size to the current progress
        Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
      } else {
        Update.printError(Serial);
      }
    }
  });
  //BellCheck page
  server.on("/bel", HTTP_GET, [] () {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", "The bell trigger was: " + String(bellTrigger));
    bellTrigger = false;
    triggerSonos();
  });

  //Debug page
  server.on("/debug", HTTP_GET, [] () {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", (amp(ampPin) + DebugVersion) );
  });

  //Debug big page
  server.on("/debugbig", HTTP_GET, [] () {
    server.sendHeader("Connection", "close");
    //String temp = amp(13) + "<br>" + amp(12) + "<br>" + amp(14) + "<br>" + amp(27) + "<br>" + amp(26) + "<br>" + amp(25) + "<br>" + amp(33) + "<br>" + amp(32) + "<br>" + amp(35) + "<br>" + amp(34) + "<br>" + amp(39) + "<br>" + amp(36) + "<br>" + amp(4) + "<br>" + amp(2) + "<br>" + amp(15);
    String temp = amp(35) + "<br>" + amp(35) + "<br>" + amp(35) + "<br>" + amp(35) + "<br>" + amp(35) + "<br>" + amp(35) + "<br>" + amp(35) + "<br>" + amp(35) + "<br>" + amp(35) + "<br>" + amp(35) + "<br>" + amp(35) + "<br>" + amp(35) + "<br>" + amp(35) + "<br>" + amp(35) + "<br>" + amp(35) + "<br>" + amp(35) + "<br>" + amp(35) + "<br>" + amp(35) + "<br>" + amp(35) + "<br>" + amp(35) + "<br>" + amp(35) + "<br>" + amp(35) + "<br>" + amp(35) + "<br>" + amp(35) + "<br>" + amp(35) + "<br>" + amp(35) + "<br>" + amp(35) + "<br>" + amp(35) + "<br>" + amp(35) + "<br>" + amp(35) + "<br>" + amp(35) + "<br>" + amp(35) + "<br>" + amp(35) + "<br>" + amp(35) + "<br>" + amp(35) + "<br>" + amp(35) + "<br>" + amp(35) + "<br>" + amp(35) + "<br>" + amp(35) + "<br>" + amp(35) + "<br>" + amp(35) + "<br>" + amp(35) + "<br>" + amp(35) + "<br>" + amp(35) + "<br>" + amp(35) + "<br>" + amp(35) + "<br>" + amp(35) + "<br>" + amp(35) + "<br>" + amp(35) + "<br>" + amp(35) + "<br>" + amp(35) + "<br>" + amp(35) + "<br>" + amp(35) + "<br>" + amp(35) + "<br>" + amp(35) + "<br>" + amp(35) + "<br>" + amp(35) + "<br>" + amp(35) + "<br>" + amp(35) + "<br>" + amp(35) + "<br>" + amp(35) + "<br>" + amp(35) + "<br>" + amp(35) + "<br>" + amp(35) + "<br>" + amp(35) + "<br>" + amp(35) + "<br>" + amp(35) + "<br>" + amp(35) + "<br>" + amp(35) + "<br>" + amp(35) + "<br>" + amp(35);
    server.send(200, "text/html", temp);
  });

  //Debug big page
  server.on("/stats", HTTP_GET, [] () {
    server.sendHeader("Connection", "close");
    String temp = "5.0 -> #" + String(tri50) + "<br>" + "5.2 -> #" + String(tri52) + "<br>" + "5.4 -> #" + String(tri54) + "<br>" + "5.6 -> #" + String(tri56) + "<br>" + "5.8 -> #" + String(tri58) + "<br>" + "6.0 -> #" + String(tri60) + "<br>" + "6.1 -> #" + String(tri61) + "<br>" + "6.2 -> #" + String(tri62) + "<br>" + "6.3 -> #" + String(tri63) + "<br>" + "6.4 -> #" + String(tri64) + "<br>" + "6.5 -> #" + String(tri65); 
    server.send(200, "text/html", temp);
  });
  server.begin();
}

String amp(int pin) {
  float SensorRead = analogRead(pin) * (5.0 / 4095.0);
  float Current = (SensorRead - 2.5) / multiplier;
  Serial.println("Sensor: " + String(Current, DEC));
  return ("Sensor (" + String(pin) + ") " + String(Current));
}

float ampFloat(int pin) {
  float SensorRead = analogRead(pin) * (5.0 / 4095.0);
  float Current = (SensorRead - 2.5) / multiplier;
  Serial.println("Sensor: " + String(Current, DEC));
  return Current;
}

void checkBell() {
  float firstAmp = ampFloat(35);
  if (firstAmp > 6.5)   {
     tri65 += 1;
  } else if (firstAmp > 6.4)  {
    tri64 += 1;    
  } else if (firstAmp > 6.3)  {
    tri63 += 1;
  } else if (firstAmp > 6.2)  {
    tri62 += 1;
  } else if (firstAmp > 6.1)  {
    tri61 += 1;
  } else if (firstAmp > 6.0)  {
    tri60 += 1;
  } else if (firstAmp > 5.8)  {
    tri58 += 1;
  } else if (firstAmp > 5.6)  {
    tri56 += 1;
  } else if (firstAmp > 5.4)  {
    tri54 += 1;
  } else if (firstAmp > 5.2)  {
    tri52 += 1;
  } else if (firstAmp > 5.0)  {
    tri50 += 1;
  }

  if (triHigh < firstAmp) {
   triHigh = firstAmp;     
  }
  
  if (firstAmp > bellThreshold)
  {
    if (ampFloat(35) > bellThreshold)
    {
      if (ampFloat(35) > bellThreshold)
      {
        bellTrigger = true;
        triggerSonos();
      }
    }
  }
}

void triggerSonos() {
  // Use WiFiClient class to create TCP connections
  WiFiClient client;
  if (!client.connect(sonosHost, sonosPort)) {
    Serial.println("connection failed");
    return;
  }
  // We now create a URI for the request
  String url = "/" + sonosAction;

  Serial.print("Requesting URL: ");
  Serial.println(url);

  // This will send the request to the server
  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "Connection: close\r\n\r\n");

  Serial.println();
  Serial.println("closing connection");
}

void loop(void) {
  server.handleClient();
  checkBell();
  if (millis() - last >= 100) {
    esp_task_wdt_reset();
    last = millis();
  }
}
