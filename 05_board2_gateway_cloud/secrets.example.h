/*
 * secrets.example.h - template for 05_board2_gateway_cloud
 *
 * Copy this file to secrets.h in the SAME folder and replace every placeholder
 * with a real value. secrets.h is listed in .gitignore and must never be
 * committed. This file, with placeholders only, is safe to commit.
 *
 *   copy secrets.example.h secrets.h
 *
 * The file has to live inside the sketch folder because the Arduino build only
 * compiles and includes files that sit next to the .ino.
 */

#pragma once

// ---------------------------------------------------------------------------
// Wi-Fi
// ---------------------------------------------------------------------------
// Must be a 2.4 GHz network. An ESP32 has no 5 GHz radio, so a 5 GHz-only SSID
// will never connect no matter how correct the password is. If your router
// publishes one name for both bands, the ESP32 will find the 2.4 GHz half.
// Run sketch 03 to list every 2.4 GHz network the board can actually see.
#define WIFI_SSID "YOUR_2G4_WIFI_SSID"

// 0 = WPA2-Personal: an ordinary pre-shared key. Home routers, phone hotspots.
// 1 = WPA2-Enterprise: 802.1X with PEAP. University networks such as eduroam.
#define WIFI_USE_ENTERPRISE 0

// --- used when WIFI_USE_ENTERPRISE is 0 ---
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

// --- used when WIFI_USE_ENTERPRISE is 1 ---
//
// SECURITY NOTE: these are compiled into the firmware image as plain text. Any
// person who can reach the board's USB port can recover them with a single
// esptool read-flash command. On eduroam that is normally your full
// institutional account, so treat a flashed board as a device that carries your
// login. Do not lend it out, and re-flash something harmless before you do.
//
// Identity is the OUTER identity sent in the clear during the handshake. Some
// institutions want an anonymous value here such as "anonymous@example.ac.nz";
// others want the same string as the username. If in doubt, use the same.
// Username and password are your normal institutional login.
#define WIFI_EAP_IDENTITY "YOUR_EDUROAM_IDENTITY"
#define WIFI_EAP_USERNAME "YOUR_EDUROAM_USERNAME"
#define WIFI_EAP_PASSWORD "YOUR_EDUROAM_PASSWORD"

// ---------------------------------------------------------------------------
// MQTT broker
// ---------------------------------------------------------------------------
// ThingsBoard community cloud. Use your own host or LAN IP instead if you are
// running a self-hosted instance. No scheme, no port, just the hostname.
#define MQTT_HOST "thingsboard.cloud"

// ---------------------------------------------------------------------------
// ThingsBoard device credential
// ---------------------------------------------------------------------------
// In ThingsBoard: Entities -> Devices -> your device -> Manage credentials ->
// Access token. Copy the token exactly, with no surrounding spaces.
//
// This token is used as the MQTT USERNAME. The MQTT password stays empty. That
// is ThingsBoard's access-token authentication scheme, and it is why the sketch
// calls connect(clientId, token, "").
#define THINGSBOARD_ACCESS_TOKEN "YOUR_THINGSBOARD_DEVICE_ACCESS_TOKEN"
