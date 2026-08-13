/*
 * secrets.example.h - template for 04_board1_espnow_sender
 *
 * Copy this file to secrets.h in the SAME folder. secrets.h is gitignored;
 * this template, with placeholder keys only, is safe to commit.
 *
 *   copy secrets.example.h secrets.h
 *
 * ---------------------------------------------------------------------------
 * ESP-NOW link encryption keys
 * ---------------------------------------------------------------------------
 * These two keys authenticate and encrypt the radio link between the node and
 * the gateway. BOTH BOARDS MUST USE IDENTICAL VALUES - copy the same numbers
 * into 05_board2_gateway_cloud/secrets.h. If they differ, frames are silently
 * discarded by the receiving side and nothing arrives.
 *
 * PMK (primary master key)  encrypts the LMK during peer setup
 * LMK (local master key)    encrypts the actual payload for this peer pair
 *
 * Both are exactly 16 bytes. Generate your own rather than shipping these:
 *
 *   PowerShell:
 *     -join ((1..16) | ForEach-Object { '0x{0:X2}' -f (Get-Random -Max 256) }) -replace '0x','0x'
 *
 *   or in Python:
 *     python -c "import os;print(', '.join('0x%02X'%b for b in os.urandom(16)))"
 *
 * What this buys: an attacker in radio range can no longer forge a frame that
 * the gateway will accept, because they cannot produce a valid ciphertext
 * without the LMK. Source MAC filtering alone did not achieve this - a MAC
 * address is trivially spoofable by anyone able to transmit.
 *
 * What it does not buy: anyone with physical access to either board can read
 * these keys out of flash over USB, exactly as with the Wi-Fi credentials.
 */

#pragma once

#define ESPNOW_PMK { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, \
                     0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F }

#define ESPNOW_LMK { 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, \
                     0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F }
