#pragma once

#include <Arduino.h>
#include <IPAddress.h>
#include <WiFi.h>

void setupArduinoOta();
void startMdns();
bool startEthernet();
bool networkReady();
IPAddress networkLocalIp();
IPAddress currentGatewayIp();
IPAddress currentSubnetMask();
IPAddress currentDnsIp();
String networkModeString();
void startWifi();
void handleNetworkRecovery();
void handleFirmwarePage();
String probeWifiConfigRedirectUrl(const String &ssid, const String &password, bool dhcp,
                                  const IPAddress &ip, const IPAddress &gateway,
                                  const IPAddress &subnet, const IPAddress &dns,
                                  const String &hostname, uint32_t timeoutMs = 10000,
                                  wl_status_t *statusOut = nullptr);
void sendReconnectPage(const String &title, const String &headline, const String &message,
                       const String &preferredUrl = "", bool includeApFallback = true, int statusCode = 200);
void handleFirmwareUpdate();
void handleFirmwareUpload();
void handleFilesystemUpdate();
void handleFilesystemUpload();
void handleFactoryReset();
void handleApiStatus();
void handleApiMeta();
void handleWifiScan();
