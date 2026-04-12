static bool ethernetLinkReady();

String probeWifiConfigRedirectUrl(const String &ssid, const String &password, bool dhcp,
                                  const IPAddress &ip, const IPAddress &gateway,
                                  const IPAddress &subnet, const IPAddress &dns,
                                  const String &hostname, uint32_t timeoutMs,
                                  wl_status_t *statusOut) {
  if (statusOut) *statusOut = WL_IDLE_STATUS;
  if (!ssid.length()) return "";

  wifi_mode_t currentMode = WiFi.getMode();
  if (currentMode == WIFI_AP || currentMode == WIFI_AP_STA) {
    WiFi.mode(WIFI_AP_STA);
  } else {
    WiFi.mode(WIFI_STA);
  }

  WiFi.setHostname(hostname.length() ? hostname.c_str() : kDefaultHostname);
  if (!dhcp) {
    bool staticComplete = ip != IPAddress() && gateway != IPAddress() && subnet != IPAddress();
    if (staticComplete) {
      WiFi.config(ip, gateway, subnet, dns);
    } else {
      WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);
    }
  } else {
    WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);
  }

  WiFi.disconnect(false, false);
  WiFi.begin(ssid.c_str(), password.c_str());

  unsigned long start = millis();
  while (millis() - start < timeoutMs) {
    if (WiFi.status() == WL_CONNECTED && WiFi.localIP() != IPAddress()) {
      if (statusOut) *statusOut = WL_CONNECTED;
      String url = "http://" + WiFi.localIP().toString() + "/";
      Serial.print("wifi: pre-reboot IP probe succeeded ");
      Serial.println(url);
      return url;
    }
    delay(100);
  }

  wl_status_t finalStatus = WiFi.status();
  if (statusOut) *statusOut = finalStatus;
  Serial.print("wifi: pre-reboot IP probe timed out, status=");
  Serial.println(static_cast<int>(finalStatus));
  return "";
}

void sendReconnectPage(const String &title, const String &headline, const String &message,
                       const String &preferredUrl, bool includeApFallback, int statusCode) {
  String mdnsUrl = "http://" + htmlEscape(config.hostname.length() ? config.hostname : kDefaultHostname) + ".local/";
  String currentIpUrl = networkLocalIp() != IPAddress() ? ("http://" + networkLocalIp().toString() + "/") : "";
  String staticIpUrl = (!config.eth.enabled && config.wifi.ssid.length() > 0 && !config.wifi.dhcp &&
                         config.wifi.ip != IPAddress()) ? ("http://" + config.wifi.ip.toString() + "/") : "";

  String targets = "location.origin + '/'";
  auto addTarget = [&targets](const String &url) {
    String cleaned = url;
    cleaned.trim();
    if (!cleaned.length()) return;
    String normalized = cleaned.endsWith("/") ? cleaned : (cleaned + "/");
    String needle = "'" + normalized + "'";
    if (targets.indexOf(needle) >= 0) return;
    targets += ",";
    targets += needle;
  };

  addTarget(preferredUrl);
  addTarget(currentIpUrl);
  addTarget(mdnsUrl);
  addTarget(staticIpUrl);
  if (includeApFallback) addTarget("http://192.168.4.1/");

  String html = "<!doctype html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>";
  html += "<meta name='theme-color' content='#0a0e14'>";
  html += "<title>" + htmlEscape(title) + "</title>";
  html += "<style>";
  html += "*,*::before,*::after{box-sizing:border-box;}";
  html += ":root{--bg:#0a0e14;--surface:#131920;--surface-elevated:#1a2029;--border:#252d3a;--text:#f0f4f8;--text-muted:#7a8a9d;--primary:#3b82f6;--warning:#f59e0b;--radius:14px;--radius-sm:10px;}";
  html += "body{margin:0;font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;background:var(--bg);color:var(--text);line-height:1.5;min-height:100vh;-webkit-font-smoothing:antialiased;}";
  html += ".container{max-width:640px;margin:0 auto;padding:16px;}";
  html += ".header{text-align:center;padding:24px 0 16px;}";
  html += ".header h1{margin:0;font-size:1.4rem;font-weight:700;letter-spacing:-0.03em;}";
  html += ".header p{margin:4px 0 0;color:var(--text-muted);font-size:.85rem;}";
  html += ".card{background:var(--surface);border:1px solid var(--border);border-radius:var(--radius);padding:16px;margin-bottom:12px;}";
  html += ".title{margin:0 0 10px;font-size:1rem;font-weight:700;}";
  html += ".notice{margin-top:12px;background:rgba(245,158,11,.12);border:1px solid rgba(245,158,11,.28);border-radius:var(--radius-sm);padding:12px;color:#fde68a;}";
  html += ".status{margin-top:12px;padding:12px;background:var(--surface-elevated);border-radius:var(--radius-sm);font-family:Consolas,monospace;font-size:.8rem;white-space:pre-wrap;}";
  html += ".status-row{display:flex;align-items:center;gap:10px;}";
  html += ".spinner{width:16px;height:16px;border:2px solid rgba(255,255,255,.18);border-top-color:var(--primary);border-radius:50%;animation:spin .8s linear infinite;flex:0 0 auto;}";
  html += ".status-text{flex:1;min-width:0;}";
  html += ".target{margin-top:10px;color:var(--text-muted);font-size:.82rem;word-break:break-all;}";
  html += "code{background:var(--surface-elevated);border:1px solid var(--border);border-radius:8px;padding:2px 6px;color:var(--text);}";
  html += "@keyframes spin{to{transform:rotate(360deg);}}";
  html += "</style>";
  html += "</head><body><div class='container'><header class='header'><h1>" + htmlEscape(title) + "</h1><p>Automatic reconnect in progress</p></header>";
  html += "<div class='card'><h2 class='title'>" + htmlEscape(headline) + "</h2>";
  html += "<p>" + htmlEscape(message) + "</p>";
  html += "<div class='notice'>The page will keep trying to reconnect and redirect to the main page when the device is reachable again.</div>";
  html += "<div class='status status-row'><span class='spinner'></span><span id='reconnect_status' class='status-text'>Waiting for device restart...</span></div>";
  html += "<div id='reconnect_target' class='target'></div></div>";
  html += "<script>";
  html += "const targets=[" + targets + "];";
  html += "const statusEl=document.getElementById('reconnect_status');";
  html += "const targetEl=document.getElementById('reconnect_target');";
  html += "async function probe(url){try{await fetch(url+'?_reconnect='+Date.now(),{mode:'no-cors',cache:'no-store'});return true;}catch(e){return false;}}";
  html += "async function tryReconnect(){for(const url of targets){statusEl.textContent='Trying to reconnect...';targetEl.textContent='Trying '+url;if(await probe(url)){statusEl.textContent='Connection successful. Redirecting to the main page.';targetEl.textContent=url;setTimeout(function(){location.replace(url);},800);return;}}setTimeout(tryReconnect,3000);}";
  html += "setTimeout(tryReconnect,2000);";
  html += "</script></div></body></html>";
  web.send(statusCode, "text/html", html);
}

void handleFirmwarePage() {
  sendSpiffsFallbackPage("System", "/system.html");
}

void handleFirmwareUpdate() {
  if (!checkAuth()) { requestAuth(); return; }
  bool ok = !Update.hasError();
  if (ok) {
    sendReconnectPage("Firmware Update", "Firmware updated. Rebooting...",
                      "Trying to reconnect to the device after the firmware update.", "", true, 200);
  } else {
    web.send(500, "text/html", "<!doctype html><html><body><h3>Firmware update failed.</h3></body></html>");
  }
  if (ok) {
    delay(500);
    ESP.restart();
  }
}

void handleFirmwareUpload() {
  if (!checkAuth()) { requestAuth(); return; }
  HTTPUpload &up = web.upload();
  if (up.status == UPLOAD_FILE_START) {
    Serial.printf("ota-web: start %s\n", up.filename.c_str());
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
      Update.printError(Serial);
    }
  } else if (up.status == UPLOAD_FILE_WRITE) {
    if (Update.write(up.buf, up.currentSize) != up.currentSize) {
      Update.printError(Serial);
    }
  } else if (up.status == UPLOAD_FILE_END) {
    if (Update.end(true)) {
      Serial.printf("ota-web: success %u bytes\n", up.totalSize);
    } else {
      Update.printError(Serial);
    }
  } else if (up.status == UPLOAD_FILE_ABORTED) {
    Update.abort();
    Serial.println("ota-web: aborted");
  }
}

void handleFilesystemUpdate() {
  if (!checkAuth()) { requestAuth(); return; }
  bool ok = !Update.hasError();
  if (ok) {
    sendReconnectPage("Filesystem Update", "Filesystem updated. Rebooting...",
                      "Trying to reconnect to the device after the SPIFFS update.", "", true, 200);
  } else {
    web.send(500, "text/html", "<!doctype html><html><body><h3>Filesystem update failed.</h3></body></html>");
  }
  if (ok) {
    delay(500);
    ESP.restart();
  }
}

void handleFilesystemUpload() {
  if (!checkAuth()) { requestAuth(); return; }
  HTTPUpload &up = web.upload();
  if (up.status == UPLOAD_FILE_START) {
    Serial.printf("fs-web: start %s\n", up.filename.c_str());
    if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_SPIFFS)) {
      Update.printError(Serial);
    }
  } else if (up.status == UPLOAD_FILE_WRITE) {
    if (Update.write(up.buf, up.currentSize) != up.currentSize) {
      Update.printError(Serial);
    }
  } else if (up.status == UPLOAD_FILE_END) {
    if (Update.end(true)) {
      Serial.printf("fs-web: success %u bytes\n", up.totalSize);
    } else {
      Update.printError(Serial);
    }
  } else if (up.status == UPLOAD_FILE_ABORTED) {
    Update.abort();
    Serial.println("fs-web: aborted");
  }
}

void handleFactoryReset() {
  if (!checkAuth()) { requestAuth(); return; }
  Serial.println("system: factory reset requested");
  clearPersistedData();
  sendReconnectPage("Factory Reset", "Factory reset complete. Rebooting...",
                    "All saved settings and persisted HVAC state were erased. The device should return in setup AP mode.",
                    "http://192.168.4.1/", true, 200);
  delay(500);
  ESP.restart();
}

void handleApiStatus() {
  if (!checkAuth()) { requestAuth(); return; }
  JsonDocument doc;
  const bool ethLinkUp = ETH.linkUp();
  const IPAddress ethLocalIp = ETH.localIP();
  const bool ethRuntimeReady = ethernetLinkReady();
  const wifi_mode_t wifiMode = WiFi.getMode();
  doc["firmware_version"] = kFirmwareVersion;
  doc["filesystem_version"] = filesystemVersion;
  doc["filesystem_version_expected"] = kFilesystemVersionExpected;
  doc["version_match"] = (filesystemVersion == String(kFilesystemVersionExpected));
  doc["boot_count"] = bootCount;
  doc["reset_reason"] = resetReason;
  doc["network_mode"] = networkModeString();
  doc["ip"] = networkLocalIp().toString();
  doc["gateway"] = currentGatewayIp().toString();
  doc["subnet"] = currentSubnetMask().toString();
  doc["dns"] = currentDnsIp().toString();
  doc["hostname"] = config.hostname.length() ? config.hostname : kDefaultHostname;
  doc["timezone"] = normalizeTimezoneOffset(config.timezone);
  doc["uptime_ms"] = millis();
  doc["telnet_port"] = config.telnetPort;
  doc["dinplug_status"] = dinplugConnectionStatus();
  doc["emitter_count"] = config.emitterCount;
  doc["hvac_count"] = config.hvacCount;
  doc["dinplug_bindings_used"] = dinplugBindingCount;
  doc["dinplug_bindings_total"] = kMaxDinplugBindingsTotal;
  doc["ir_receiver_enabled"] = config.irReceiver.enabled;
  doc["ir_receiver_gpio"] = config.irReceiver.gpio;
  doc["heap_free"] = ESP.getFreeHeap();
  doc["heap_min_free"] = ESP.getMinFreeHeap();
  doc["heap_max_alloc"] = ESP.getMaxAllocHeap();
  doc["trend_samples_count"] = trendHistoryCount;
  doc["trend_sample_interval_sec"] = (kTrendSampleIntervalMs / 1000UL);
  doc["telnet_clients_active"] = activeTelnetClientCount();
  JsonArray telnetClientsJson = doc["telnet_clients"].to<JsonArray>();
  for (uint8_t i = 0; i < kMaxTelnetClients; i++) {
    WiFiClient &c = telnetClients[i];
    if (!c || !c.connected()) continue;
    JsonObject tc = telnetClientsJson.add<JsonObject>();
    tc["slot"] = i;
    tc["ip"] = c.remoteIP().toString();
    tc["port"] = c.remotePort();
  }
  doc["temp_sensors_enabled"] = config.tempSensors.enabled;
  doc["temp_sensor_count"] = tempSensorCount;
  doc["temp_sensor_precision"] = tempSensorPrecision();
  JsonArray sensors = doc["temp_sensors"].to<JsonArray>();
  for (uint8_t i = 0; i < tempSensorCount; i++) {
    JsonObject s = sensors.add<JsonObject>();
    s["index"] = i;
    s["name"] = sensorNameForIndex(i);
    s["address"] = sensorAddressToString(tempSensorAddresses[i]);
    s["valid"] = tempSensorValid[i];
    if (tempSensorValid[i]) s["current_temp"] = tempSensorReadings[i];
  }
  doc["ethernet_enabled"] = config.eth.enabled;
  doc["eth_begin_last_ok"] = ethernetBeginLastOk;
  doc["eth_link_up"] = ethLinkUp;
  doc["eth_local_ip"] = ethLocalIp.toString();
  doc["eth_gateway"] = ETH.gatewayIP().toString();
  doc["eth_subnet"] = ETH.subnetMask().toString();
  doc["eth_dns"] = ETH.dnsIP().toString();
  doc["eth_runtime_ready"] = ethRuntimeReady;
  doc["eth_last_begin_ms"] = ethernetLastBeginMs;
  doc["eth_last_link_up_ms"] = ethernetLastLinkUpMs;
  doc["wifi_mode_raw"] = static_cast<int>(wifiMode);
  doc["wifi_rssi"] = WiFi.isConnected() ? WiFi.RSSI() : 0;
  doc["time_synced"] = clockHasValidTime();
  doc["local_time"] = localTimeString();
  String out;
  serializeJson(doc, out);
  web.send(200, "application/json", out);
}

void handleApiMeta() {
  if (!checkAuth()) { requestAuth(); return; }
  JsonDocument doc;
  doc["firmware_version"] = kFirmwareVersion;
  doc["filesystem_version"] = filesystemVersion;
  doc["filesystem_version_expected"] = kFilesystemVersionExpected;
  doc["version_match"] = (filesystemVersion == String(kFilesystemVersionExpected));
  JsonArray protocols = doc["protocols"].to<JsonArray>();
  protocols.add("CUSTOM");
  for (uint16_t i = 0; i <= kLastDecodeType; i++) {
    decode_type_t proto = static_cast<decode_type_t>(i);
    if (!IRac::isProtocolSupported(proto)) continue;
    protocols.add(typeToString(proto));
  }

  JsonArray gpioOptions = doc["gpio_options"].to<JsonArray>();
  const uint8_t gpios[] = {2,4,5,12,13,14,15,16,17,18,19,21,22,23,25,26,27,32,33};
  for (uint8_t gpio : gpios) gpioOptions.add(gpio);

  JsonArray dinActions = doc["din_actions"].to<JsonArray>();
  const char *actions[] = {"none","temp_up","temp_down","set_temp","power_on","power_off","toggle_power","mode_heat","mode_cool","mode_fan","mode_auto","mode_off","light_on","light_off","toggle_light"};
  for (const char *action : actions) dinActions.add(action);

  JsonArray toggleModes = doc["toggle_modes"].to<JsonArray>();
  const char *modes[] = {"auto","cool","heat","dry","fan"};
  for (const char *mode : modes) toggleModes.add(mode);

  JsonArray modeOverrides = doc["mode_overrides"].to<JsonArray>();
  const char *modeOverrideList[] = {"keep","auto","cool","heat","dry","fan"};
  for (const char *mode : modeOverrideList) modeOverrides.add(mode);

  JsonArray lightModes = doc["light_modes"].to<JsonArray>();
  const char *lightModesList[] = {"keep","on","off","toggle"};
  for (const char *mode : lightModesList) lightModes.add(mode);

  doc["max_custom_commands"] = kMaxCustomCommands;
  doc["max_dinplug_buttons"] = kMaxDinplugBindingsTotal;
  doc["max_dinplug_bindings_total"] = kMaxDinplugBindingsTotal;
  doc["dinplug_bindings_used"] = dinplugBindingCount;
  doc["dinplug_bindings_available"] = kMaxDinplugBindingsTotal - dinplugBindingCount;
  doc["max_temp_sensors"] = kMaxTempSensors;
  doc["max_emitters"] = kMaxEmitters;
  doc["max_hvacs"] = kMaxHvacs;

  String out;
  serializeJson(doc, out);
  web.send(200, "application/json", out);
}

void handleWifiScan() {
  if (!checkAuth()) { requestAuth(); return; }
  JsonDocument doc;
  JsonArray networks = doc["networks"].to<JsonArray>();
  WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
  WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);
  int n = WiFi.scanNetworks(false, true);
  for (int i = 0; i < n; i++) {
    JsonObject o = networks.add<JsonObject>();
    o["ssid"] = WiFi.SSID(i);
    o["rssi"] = WiFi.RSSI(i);
  }
  String out;
  serializeJson(doc, out);
  web.send(200, "application/json", out);
}

void setupArduinoOta() {
  const char *host = (config.hostname.length() ? config.hostname.c_str() : kDefaultHostname);
  ArduinoOTA.setHostname(host);
  if (config.web.password.length()) {
    ArduinoOTA.setPassword(config.web.password.c_str());
  }
  ArduinoOTA.onStart([]() {
    Serial.println("ota: start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nota: end");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("ota: %u%%\r", (progress * 100U) / total);
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("ota: error[%u]\n", error);
  });
  ArduinoOTA.begin();
  Serial.print("ota: ready on ");
  Serial.print(host);
  Serial.println(".local");
}

static void configureWifiStation() {
  dnsServer.stop();
  dnsServerActive = false;
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(config.hostname.length() ? config.hostname.c_str() : kDefaultHostname);
  if (!config.wifi.dhcp) {
    WiFi.config(config.wifi.ip, config.wifi.gateway, config.wifi.subnet, config.wifi.dns);
  }
}

static void beginWifiStationConnection() {
  configureWifiStation();
  wifiFallbackPending = true;
  WiFi.begin(config.wifi.ssid.c_str(), config.wifi.password.c_str());
}

static void stopWifiForEthernet() {
  dnsServer.stop();
  dnsServerActive = false;
  if (WiFi.getMode() != WIFI_OFF) {
    WiFi.disconnect(false, false);
    WiFi.mode(WIFI_OFF);
  }
}

static bool ethernetLinkReady() {
  return ethernetUp && ETH.linkUp() && ETH.localIP() != IPAddress();
}

void handleNetworkEvent(WiFiEvent_t event) {
  switch (event) {
    case ARDUINO_EVENT_ETH_START:
      ETH.setHostname(config.hostname.length() ? config.hostname.c_str() : kDefaultHostname);
      Serial.println("eth: event start");
      break;
    case ARDUINO_EVENT_ETH_CONNECTED:
      Serial.println("eth: event connected");
      break;
    case ARDUINO_EVENT_ETH_GOT_IP:
      ethernetUp = true;
      wifiFallbackPending = false;
      ethernetLastLinkUpMs = millis();
      Serial.print("eth: event got ip=");
      Serial.println(ETH.localIP());
      stopWifiForEthernet();
      startMdns();
      break;
    case ARDUINO_EVENT_ETH_DISCONNECTED:
      Serial.println("eth: event disconnected");
      ethernetUp = false;
      if (config.wifi.ssid.length() > 0) {
        WiFi.disconnect(false, false);
        beginWifiStationConnection();
      } else {
        WiFi.mode(WIFI_AP);
        WiFi.softAP(kApSsid);
        WiFi.softAPsetHostname(config.hostname.length() ? config.hostname.c_str() : kDefaultHostname);
        dnsServer.start(53, "*", WiFi.softAPIP());
        dnsServerActive = true;
        startMdns();
      }
      break;
    case ARDUINO_EVENT_ETH_STOP:
      Serial.println("eth: event stop");
      ethernetUp = false;
      break;
    default:
      break;
  }
}

void setupNetworkEvents() {
  static bool registered = false;
  if (registered) return;
  WiFi.onEvent(handleNetworkEvent);
  registered = true;
}

void startMdns() {
#if ARDUINO_ARCH_ESP32
  MDNS.end();
  const char *host = (config.hostname.length() ? config.hostname.c_str() : kDefaultHostname);
  if (!MDNS.begin(host)) {
    Serial.println("mdns: start failed");
    return;
  }
  MDNS.addService("http", "tcp", 80);
  MDNS.addService("irservertelnet", "tcp", config.telnetPort);
  MDNS.addServiceTxt("irservertelnet", "tcp", "domain", "ir_server_telnet");
  MDNS.addServiceTxt("irservertelnet", "tcp", "board", "ir-server-telnet");
  MDNS.addServiceTxt("irservertelnet", "tcp", "hostname", host);
  // Keep the legacy service during the migration from the old hvactelnet domain.
  MDNS.addService("hvactelnet", "tcp", config.telnetPort);
  MDNS.addServiceTxt("hvactelnet", "tcp", "domain", "hvactelnet");
  MDNS.addServiceTxt("hvactelnet", "tcp", "board", "ir-server-telnet");
  MDNS.addServiceTxt("hvactelnet", "tcp", "hostname", host);
  Serial.print("mdns: responding for ");
  Serial.print(host);
  Serial.println(".local");
#endif
}

bool startEthernet() {
  ethernetLastBeginMs = millis();
  if (!config.eth.enabled) return false;
  if (ethernetStarted) {
    if (ethernetLinkReady()) {
      ethernetLastLinkUpMs = millis();
      return true;
    }
    return false;
  }
  ethernetUp = false;
  const char *host = (config.hostname.length() ? config.hostname.c_str() : kDefaultHostname);
  ETH.setHostname(host);
  Serial.println("eth: starting LAN8720 (WT32 defaults)");
  bool started = ETH.begin(kEthPhyAddr, kEthPowerPin, kEthMdcPin, kEthMdioPin,
                           ETH_PHY_LAN8720, ETH_CLOCK_GPIO0_IN);
  ethernetStarted = started;
  ethernetBeginLastOk = started;
  if (!started) {
    Serial.println("eth: begin failed");
    return false;
  }

  if (!config.wifi.dhcp) {
    const bool staticComplete = config.wifi.ip != IPAddress() &&
                                config.wifi.gateway != IPAddress() &&
                                config.wifi.subnet != IPAddress();
    if (staticComplete) {
      bool ok = ETH.config(config.wifi.ip, config.wifi.gateway, config.wifi.subnet, config.wifi.dns);
      Serial.println(ok ? "eth: static IP configured" : "eth: static IP config failed");
    } else {
      Serial.println("eth: static IP requested but incomplete values; falling back to DHCP");
      ETH.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);
    }
  } else {
    ETH.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);
  }

  unsigned long start = millis();
  while (millis() - start < 12000) {
    if (ETH.linkUp() && ETH.localIP() != IPAddress()) {
      ethernetUp = true;
      ethernetLastLinkUpMs = millis();
      break;
    }
    delay(100);
  }
  if (!ethernetUp) {
    Serial.println("eth: no link/IP");
    return false;
  }
  Serial.print("eth: connected IP=");
  Serial.println(ETH.localIP());
  return true;
}

bool networkReady() {
  if (ethernetLinkReady()) return true;
  if (WiFi.status() == WL_CONNECTED) return true;
  return false;
}

IPAddress networkLocalIp() {
  if (ethernetLinkReady()) return ETH.localIP();
  if (WiFi.status() == WL_CONNECTED) return WiFi.localIP();
  if (WiFi.getMode() == WIFI_MODE_AP || WiFi.getMode() == WIFI_MODE_APSTA) return WiFi.softAPIP();
  return IPAddress();
}

IPAddress currentGatewayIp() {
  if (ethernetLinkReady()) return ETH.gatewayIP();
  if (WiFi.status() == WL_CONNECTED) return WiFi.gatewayIP();
  return IPAddress();
}

IPAddress currentSubnetMask() {
  if (ethernetLinkReady()) return ETH.subnetMask();
  if (WiFi.status() == WL_CONNECTED) return WiFi.subnetMask();
  return IPAddress();
}

IPAddress currentDnsIp() {
  if (ethernetLinkReady()) return ETH.dnsIP();
  if (WiFi.status() == WL_CONNECTED) return WiFi.dnsIP();
  return IPAddress();
}

String networkModeString() {
  if (ethernetLinkReady()) return "ETH";
  if (WiFi.status() == WL_CONNECTED) return "WiFi STA";
  if (WiFi.getMode() == WIFI_MODE_AP || WiFi.getMode() == WIFI_MODE_APSTA) return "WiFi AP";
  return "offline";
}

void startWifi() {
  setupNetworkEvents();
  if (startEthernet()) {
    dnsServer.stop();
    dnsServerActive = false;
    startMdns();
    return;
  }

  if (config.wifi.ssid.length() == 0) {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(kApSsid);
    WiFi.softAPsetHostname(config.hostname.length() ? config.hostname.c_str() : kDefaultHostname);
    Serial.print("wifi: AP mode SSID=");
    Serial.println(kApSsid);
    Serial.print("wifi: AP IP=");
    Serial.println(WiFi.softAPIP());
    dnsServer.start(53, "*", WiFi.softAPIP());
    dnsServerActive = true;
    startMdns();
    return;
  }
  beginWifiStationConnection();
  Serial.print("wifi: connecting to ");
  Serial.println(config.wifi.ssid);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(200);
  }
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(kApSsid);
    WiFi.softAPsetHostname(kDefaultHostname);
    Serial.println("wifi: connect failed, fallback to AP");
    Serial.print("wifi: AP IP=");
    Serial.println(WiFi.softAPIP());
    dnsServer.start(53, "*", WiFi.softAPIP());
    dnsServerActive = true;
  } else {
    Serial.print("wifi: connected IP=");
    Serial.println(WiFi.localIP());
    startMdns();
  }
}

void handleNetworkRecovery() {
  static wl_status_t lastWifiStatus = WL_IDLE_STATUS;
  static bool lastEthReady = false;
  static unsigned long lastWifiRecoveryAttemptMs = 0;
  static unsigned long lastEthRecoveryAttemptMs = 0;

  const unsigned long now = millis();
  const wl_status_t wifiStatus = WiFi.status();
  const bool ethReady = ethernetLinkReady();

  if (WiFi.getMode() == WIFI_STA && wifiStatus == WL_CONNECTED && lastWifiStatus != WL_CONNECTED) {
    wifiFallbackPending = false;
    Serial.print("wifi: reconnected IP=");
    Serial.println(WiFi.localIP());
    if (!ethReady) startMdns();
  } else if (WiFi.getMode() == WIFI_STA && wifiStatus != WL_CONNECTED && lastWifiStatus == WL_CONNECTED) {
    Serial.print("wifi: disconnected status=");
    Serial.println(static_cast<int>(wifiStatus));
  }
  lastWifiStatus = wifiStatus;

  if (ethReady && !lastEthReady) {
    Serial.print("eth: active IP=");
    Serial.println(ETH.localIP());
  } else if (!ethReady && lastEthReady) {
    Serial.println("eth: link lost");
  }
  lastEthReady = ethReady;

  if (config.eth.enabled && !ethernetStarted && !ethReady && (now - lastEthRecoveryAttemptMs) >= 15000UL) {
    lastEthRecoveryAttemptMs = now;
    if (startEthernet()) {
      stopWifiForEthernet();
      startMdns();
      lastEthReady = true;
      return;
    }
  }

  if (ethReady) return;
  if (config.wifi.ssid.length() == 0) return;
  if (WiFi.getMode() != WIFI_STA) return;
  if (wifiStatus == WL_CONNECTED) return;
  const unsigned long wifiRecoveryIntervalMs = wifiFallbackPending ? 3000UL : 15000UL;
  if ((now - lastWifiRecoveryAttemptMs) < wifiRecoveryIntervalMs) return;
  lastWifiRecoveryAttemptMs = now;

  Serial.print("wifi: attempting recovery status=");
  Serial.println(static_cast<int>(wifiStatus));

  WiFi.disconnect(false, false);
  beginWifiStationConnection();
}
