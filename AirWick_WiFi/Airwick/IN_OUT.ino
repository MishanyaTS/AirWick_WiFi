void outData(){
    jsonWrite(configJson, "time", GetTime());
  }

void GRAF_init() {
  HTTP.on("/analog.json", HTTP_GET, []() {
    String data = graf(analogRead(A0),10,10000);
    HTTP.send(200, "application/json", data);
  });
}
