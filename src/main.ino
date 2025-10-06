#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <ArduinoJson.h>

#define AP_SSID "ESP32-Setup"
#define AP_PASSWORD ""           // vacío = AP abierto
#define DNS_PORT 53

Preferences prefs;
WebServer server(80); //crea un servidor web 80/http
DNSServer dnsServer; //Esto crea un servidor DNS falso dentro del ESP32.

IPAddress apIP(192,168,4,1); // crea el objeto con una ip estatica

//declaracion de funciones 

bool tryConnectSavedWiFi(); // intenta conectarse al wifi usando credenciales guardadas
void startConfigAP(); //arranca en modo AP
void handleRoot(); //Muestra la pagina principal
void handleConfigure(); //procesa el formulario
void handleStatus(); //
void handleReset(); //borra credenciales guardadas y reinicia

void setup() {
  Serial.begin(115200);
  delay(100);

  prefs.begin("wifi", false); // namespace (carpeta de datos en la memoria flash) "wifi"

  if (!tryConnectSavedWiFi()) { //Intenta leer de NVS los valores de wifi ssid y password
    Serial.println("No credentials or failed connection -> starting AP portal"); // mensaje de no encontro credenciales 
    startConfigAP();
  } else {
    // Arrancó conectado: levantar endpoints normales
    server.on("/status", HTTP_GET, handleStatus); //Aquí se define un endpoint REST como tal ejecutar la funcion handleStatus
    server.on("/reset", HTTP_ANY, handleReset); //endpint REST ejecuta la funcion handlereset
    server.begin(); //arranca el servidor web
    Serial.println("Server started in STA mode"); //STA mode -> Station Mode
  }
}

void loop() {
  server.handleClient();
  dnsServer.processNextRequest();
}

// Intenta leer credenciales y conectar
bool tryConnectSavedWiFi(){
  if (!prefs.isKey("ssid")) return false; // como tal buca si existe un clave llamada ssid en wifi si no eciste no guardo la red
  String ssid = prefs.getString("ssid", ""); // Recupera el SSID y la contraseña que estaban almacenados previamente en memoria flash (NVS).
  String pass = prefs.getString("pass", "");
  if (ssid.length() == 0) return false; // si por algun motivo el ssid quedo vacio tambien se cae el intento

  Serial.printf("Trying saved WiFi: %s\n", ssid.c_str());
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), pass.c_str()); //Aquí el ESP32 pasa a modo estación (STA) y comienza a intentar conectarse a la red con esas credenciales.

  unsigned long start = millis();
  const unsigned long timeout = 15000; //  limite de espera de 15s
  while ((WiFi.status() != WL_CONNECTED) && (millis() - start < timeout)) {
    delay(200);
    Serial.print("."); //mientras no este conectado y no haya pasado el tiempo  el esp sigue intentando y pones . 
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected to WiFi!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    return true; // si se conecta mensaje de exito 
  } else {
    Serial.println("\nFailed to connect with saved creds.");
    return false; //si falla manda mensaje de failed to connect ..
  }
}

void startConfigAP(){ // activar el modo Acces Point 
  WiFi.mode(WIFI_AP); //nombre 
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255,255,255,0));

  // DNS redirige todas las consultas a la IP del AP
  dnsServer.start(DNS_PORT, "*", apIP);  //Se levanta un servidor DNS falso. Cualquier dirección que escriba el usuario en su navegador será redirigida a la IP del ESP 

  // Endpoints
  server.on("/", HTTP_GET, handleRoot);
  server.on("/configure", HTTP_POST, handleConfigure);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/reset", HTTP_POST, handleReset);

  // Captive portal: redirigir cualquier path al root
  server.onNotFound([](){
    server.sendHeader("Location", String("http://") + apIP.toString(), true);
    server.send(302, "text/plain", "");
  });

  server.begin();
  Serial.println("Config AP started. Connect to SSID: " AP_SSID);
}

void handleRoot(){
  String html = R"rawliteral(
  <!DOCTYPE html>
  <html>
  <head>
    <meta name='viewport' content='width=device-width, initial-scale=1'/>
    <title>ESP32 WiFi Setup</title>
    <style>
      body { font-family: Arial, sans-serif; background: #f2f2f2; text-align: center; margin: 0; padding: 0; }
      .container { max-width: 400px; background: white; margin: 50px auto; padding: 20px; border-radius: 12px; box-shadow: 0 4px 10px rgba(0,0,0,0.1); }
      h2 { color: #333; }
      input { width: 90%; padding: 10px; margin: 8px 0; border: 1px solid #ccc; border-radius: 8px; }
      button { background: #4CAF50; color: white; padding: 12px 20px; border: none; border-radius: 8px; cursor: pointer; width: 100%; font-size: 16px; }
      button:hover { background: #45a049; }
      .msg { margin-top: 15px; font-size: 14px; color: #555; }
    </style>
  </head>
  <body>
    <div class="container">
      <h2>Configurar WiFi</h2>
      <form onsubmit="send(event)">
        <input id="ssid" name="ssid" placeholder="Nombre de la red (SSID)" required><br>
        <input id="pass" name="password" type="password" placeholder="Contraseña" required><br>
        <button type="submit">Guardar</button>
      </form>
      <div class="msg" id="msg">Ingrese los datos de su red WiFi</div>
    </div>

    <script>
      async function send(e){
        e.preventDefault();
        let ssid = document.getElementById('ssid').value;
        let pass = document.getElementById('pass').value;
        let msg = document.getElementById('msg');
        msg.innerText = "Enviando credenciales...";
        try {
          let r = await fetch('/configure',{
            method:'POST',
            headers:{'Content-Type':'application/json'},
            body: JSON.stringify({ssid:ssid,password:pass})
          });
          let t = await r.json();
          msg.innerText = t.message || JSON.stringify(t);
        } catch(err){
          msg.innerText = "Error al enviar datos";
        }
      }
    </script>
  </body>
  </html>
  )rawliteral";
server.send(200, "text/html", html);

}

void handleConfigure(){
  String body = server.arg("plain"); // cuerpo JSON
  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, body);
  if (err){
    server.send(400, "application/json", "{\"success\":false,\"message\":\"Invalid JSON\"}");
    return;
  }
  const char* ssid = doc["ssid"];
  const char* pass = doc["password"];
  if (!ssid || strlen(ssid)==0){
    server.send(400, "application/json", "{\"success\":false,\"message\":\"SSID required\"}");
    return;
  }
  // Guardar en NVS
  prefs.putString("ssid", String(ssid));
  prefs.putString("pass", String(pass?pass:""));
  server.send(200, "application/json", "{\"success\":true,\"message\":\"Guardado. Reiniciando...\"}");
  delay(1500);
  ESP.restart();
}

void handleStatus(){
  StaticJsonDocument<256> doc;
  bool connected = WiFi.status() == WL_CONNECTED;
  doc["connected"] = connected;
  doc["ssid"] = connected ? WiFi.SSID() : "";
  doc["ip"] = connected ? WiFi.localIP().toString() : (WiFi.softAPIP().toString());
  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

void handleReset(){
  prefs.clear(); // borrar namespace
  prefs.end();
  server.send(200, "application/json", "{\"success\":true,\"message\":\"Credenciales borradas. Reiniciando...\"}");
  delay(1000);
  ESP.restart();
}
