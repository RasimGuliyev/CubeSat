#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Cubesat test komut output
"""

import struct
import time
from datetime import datetime
from Crypto.Cipher import AES
from Crypto.Util import Counter


def crc8(data):
    """Simple CRC8 checksum"""
    crc = 0
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 0x80:
                crc = (crc << 1) ^ 0x31
            else:
                crc = crc << 1
            crc &= 0xFF
    return crc


def construct_iv(timestamp, sequence):
    """IV = timestamp(4B) + sequence(4B) + padding(8B)"""
    return struct.pack('<II', timestamp, sequence) + b'\x00' * 8


def encrypt_aes256_ctr(key, plaintext, timestamp, sequence):
    """AES-256 CTR sifreleme"""
    iv = construct_iv(timestamp, sequence)
    ctr = Counter.new(128, initial_value=int.from_bytes(iv, byteorder='little'))
    cipher = AES.new(key, AES.MODE_CTR, counter=ctr)
    return cipher.encrypt(plaintext)


def build_csp_header():
    """CSP header olustur"""
    CSP_PRIORITY = 0
    CSP_SOURCE = 2
    CSP_DEST = 7
    CSP_DEST_PORT = 12
    CSP_SOURCE_PORT = 18
    CSP_FLAGS_XTEA = 25
    CSP_FLAGS_CRC = 27

    header = 0
    header |= (0 & 0x03)  # Priority = 0
    header |= (2 & 0x1F) << CSP_SOURCE  # Source = 2
    header |= (1 & 0x1F) << CSP_DEST  # Dest = 1
    header |= (10 & 0x3F) << CSP_DEST_PORT
    header |= (20 & 0x3F) << CSP_SOURCE_PORT
    header |= 1 << CSP_FLAGS_XTEA  # Encryption enabled
    header |= 1 << CSP_FLAGS_CRC
    return struct.pack('<I', header)


# Test komutlari
print("=" * 70)
print("CUBESAT KOMUT TEST - Sistem Durumu Sorgulama")
print("=" * 70)

test_key = bytes.fromhex("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef")

# Test 1: Sistem Durumunu Sor
print("\n[TEST 1] CMD_GET_SYSTEM_STATUS Gonder")
print("-" * 70)

timestamp = int(time.time())
sequence = 1
cmd_type = 0x09  # CMD_GET_SYSTEM_STATUS
payload_len = 0

# Komut header konstruksiyonu
cmd_header = struct.pack('<IBBHBxxx', timestamp, cmd_type, sequence, payload_len, 0)
crc = crc8(cmd_header[:8])
cmd_header = struct.pack('<IBBHBxxx', timestamp, cmd_type, sequence, payload_len, crc)

full_payload = cmd_header
encrypted = encrypt_aes256_ctr(test_key, full_payload, timestamp, sequence)

csp_header = build_csp_header()
packet = csp_header + encrypted

print(f"Timestamp: {timestamp} ({datetime.fromtimestamp(timestamp).strftime('%Y-%m-%d %H:%M:%S')})")
print(f"Komut: GET_SYSTEM_STATUS (0x09)")
print(f"Sequence: {sequence}")
print(f"CRC8: 0x{crc:02X}")
print(f"Paket Boyutu: {len(packet)} bytes")
print(f"\nCSP Header: {csp_header.hex().upper()}")
print(f"Encrypted Payload: {encrypted.hex().upper()[:64]}...")
print(f"\nFULL PACKET HEX:\n{packet.hex().upper()}")

# Test 2: Veri Talep Et
print("\n\n[TEST 2] CMD_DATA_REQUEST Gonder")
print("-" * 70)

sequence = 2
cmd_type = 0x03  # CMD_DATA_REQUEST

cmd_header = struct.pack('<IBBHBxxx', timestamp, cmd_type, sequence, 0, 0)
crc = crc8(cmd_header[:8])
cmd_header = struct.pack('<IBBHBxxx', timestamp, cmd_type, sequence, 0, crc)

full_payload = cmd_header
encrypted = encrypt_aes256_ctr(test_key, full_payload, timestamp, sequence)

packet = csp_header + encrypted

print(f"Komut: DATA_REQUEST (0x03)")
print(f"Paket Boyutu: {len(packet)} bytes")
print(f"FULL PACKET HEX:\n{packet.hex().upper()}")

# Test 3: MODE CHANGE - Safe Mode
print("\n\n[TEST 3] CMD_MODE_CHANGE -> Safe Mode")
print("-" * 70)

sequence = 3
cmd_type = 0x01  # CMD_MODE_CHANGE
mode_payload = struct.pack('B', 1)  # 1 = Safe Mode
payload_len = 1

cmd_header = struct.pack('<IBBHBxxx', timestamp, cmd_type, sequence, payload_len, 0)
crc = crc8(cmd_header[:8] + mode_payload)
cmd_header = struct.pack('<IBBHBxxx', timestamp, cmd_type, sequence, payload_len, crc)

full_payload = cmd_header + mode_payload
encrypted = encrypt_aes256_ctr(test_key, full_payload, timestamp, sequence)

packet = csp_header + encrypted

print(f"Komut: MODE_CHANGE (0x01)")
print(f"Hedef Mode: Safe (1)")
print(f"Paket Boyutu: {len(packet)} bytes")
print(f"FULL PACKET HEX:\n{packet.hex().upper()}")

# Ozet
print("\n\n" + "=" * 70)
print("OZET - 3 Komut Gonderildi")
print("=" * 70)
print("1. GET_SYSTEM_STATUS")
print("2. DATA_REQUEST")
print("3. MODE_CHANGE (Safe Mode)")
print("\nBu paketler LoRa modulü üzerinden uyduya gonderilir.")
print("=" * 70)
