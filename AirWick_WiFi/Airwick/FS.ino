// Инициализация FFS
void FS_init(void) {
  if (!LittleFS.begin()) {
    LOG.println(F("Ошибка монтирования LittleFS"));
    return;
  }
  LOG.println(F("LittleFS успешно подключена"));
  {
    Dir dir = LittleFS.openDir("/");
    while (dir.next()) {
      String fileName = dir.fileName();
      size_t fileSize = dir.fileSize();
    }
  }
  // HTTP страницы для работы с FFS
  HTTP.on("/list", HTTP_GET, handleFileList);
  HTTP.on("/edit", HTTP_GET, []() {
    if (!handleFileRead("/edit.htm")) HTTP.send(404, "text/plain", "FileNotFound");
  });
  HTTP.on("/edit", HTTP_PUT, handleFileCreate);
  HTTP.on("/edit", HTTP_DELETE, handleFileDelete);
  HTTP.on("/edit", HTTP_POST, []() {
    HTTP.send(200, "text/plain", "");
  }, handleFileUpload);
  HTTP.onNotFound([]() {
    if (!handleFileRead(HTTP.uri()))
      HTTP.send(404, "text/plain", "FileNotFound");
  });
}

String getContentType(String filename) {
  if (HTTP.hasArg("download")) return "application/octet-stream";
  else if (filename.endsWith(".htm")) return "text/html";
  else if (filename.endsWith(".html")) return "text/html";
  else if (filename.endsWith(".json")) return "application/json";
  else if (filename.endsWith(".css")) return "text/css";
  else if (filename.endsWith(".js")) return "application/javascript";
  else if (filename.endsWith(".png")) return "image/png";
  else if (filename.endsWith(".gif")) return "image/gif";
  else if (filename.endsWith(".jpg")) return "image/jpeg";
  else if (filename.endsWith(".ico")) return "image/x-icon";
  else if (filename.endsWith(".xml")) return "text/xml";
  else if (filename.endsWith(".pdf")) return "application/x-pdf";
  else if (filename.endsWith(".zip")) return "application/x-zip";
  else if (filename.endsWith(".gz")) return "application/x-gzip";
  return "text/plain";
}

bool handleFileRead(String path) {
  if (path.endsWith("/")) path += "index.htm";
  String contentType = getContentType(path);
  String pathWithGz = path + ".gz";
  if (LittleFS.exists(pathWithGz) || LittleFS.exists(path)) {
    if (LittleFS.exists(pathWithGz))
      path += ".gz";
    File file = LittleFS.open(path, "r");
    size_t sent = HTTP.streamFile(file, contentType);
    file.close();
    return true;
  }
  return false;
}

void handleFileUpload() {
  if (HTTP.uri() != "/edit") return;
  HTTPUpload& upload = HTTP.upload();
  if (upload.status == UPLOAD_FILE_START) {
    String filename = upload.filename;
    if (!filename.startsWith("/")) filename = "/" + filename;
    fsUploadFile = LittleFS.open(filename, "w");
    filename = String();
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (fsUploadFile)
      fsUploadFile.write(upload.buf, upload.currentSize);
  } else if (upload.status == UPLOAD_FILE_END) {
    if (fsUploadFile)
      fsUploadFile.close();
  }
}

void handleFileDelete() {
  if (HTTP.args() == 0) return HTTP.send(500, "text/plain", "BAD ARGS");
  String path = HTTP.arg(0);
  if (path == "/")
    return HTTP.send(500, "text/plain", "BAD PATH");
  if (!LittleFS.exists(path))
    return HTTP.send(404, "text/plain", "FileNotFound");
  LittleFS.remove(path);
  HTTP.send(200, "text/plain", "");
  path = String();
}

void handleFileCreate() {
  if (HTTP.args() == 0)
    return HTTP.send(500, "text/plain", "BAD ARGS");
  String path = HTTP.arg(0);
  if (path == "/")
    return HTTP.send(500, "text/plain", "BAD PATH");
  if (LittleFS.exists(path))
    return HTTP.send(500, "text/plain", "FILE EXISTS");
  File file = LittleFS.open(path, "w");
  if (file)
    file.close();
  else
    return HTTP.send(500, "text/plain", "CREATE FAILED");
  HTTP.send(200, "text/plain", "");
  path = String();
}

void handleFileList() {
  if (!HTTP.hasArg("dir")) {
    HTTP.send(500, "text/plain", "BAD ARGS");
    return;
  }
  String path = HTTP.arg("dir");
  Dir dir = LittleFS.openDir(path);
  path = String();
  String output = "[";
  while (dir.next()) {
    File entry = dir.openFile("r");
    if (output != "[") output += ',';
    bool isDir = false;
    output += "{\"type\":\"";
    output += (isDir) ? "dir" : "file";
    output += "\",\"name\":\"";
    String entryName = String(entry.name());
    if (entryName.startsWith("/")) entryName.remove(0, 1);
    output += entryName;
    output += "\"}";
    entry.close();
  }
  output += "]";
  HTTP.send(200, "text/json", output);
}
