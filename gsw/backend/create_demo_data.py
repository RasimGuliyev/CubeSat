#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
CubeSat Telemetri Gonderici - Demo
Otomatik olarak örnek veri setleri olustur
"""

import struct
import json
from datetime import datetime
from Crypto.Cipher import AES
from Crypto.Util import Counter


def construct_iv(timestamp, sequence):
    return struct.pack('<II', timestamp, sequence) + b'\x00' * 8


def encrypt_aes256_ctr(key, plaintext, timestamp, sequence):
    iv = construct_iv(timestamp, sequence)
    ctr = Counter.new(128, initial_value=int.from_bytes(iv, byteorder='little'))
    cipher = AES.new(key, AES.MODE_CTR, counter=ctr)
    return cipher.encrypt(plaintext)


def build_csp_header():
    header = 0
    header |= (2 & 0x1F) << 2
    header |= (1 & 0x1F) << 7
    header |= (10 & 0x3F) << 12
    header |= (20 & 0x3F) << 18
    header |= 1 << 25
    header |= 1 << 27
    return struct.pack('<I', header)


# Test AES key
aes_key = bytes.fromhex("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef")
timestamp = 1774753200  # Sabit timestamp

packets = []
sequence = 1

print("=" * 80)
print("CUBESAT TELEMETRI OLUSTUR - Demo Veri Setleri")
print("=" * 80)

# ============================================================================
# SENARYO 1: NORMAL OPERASYON
# ============================================================================
print("\n[SENARYO 1] NORMAL OPERASYON")
print("-" * 80)

# Beacon 1
beacon1 = struct.pack(
    '<IBffffBH',
    timestamp, 0, 7.8, 30.2, -22.1, 38.5, 42, 3000
)
enc1 = encrypt_aes256_ctr(aes_key, beacon1, timestamp, sequence)
pkt1 = build_csp_header() + enc1

print(f"Paket 1: BEACON (Normal mode)")
print(f"  Battery: 7.8V | Temp: 42°C")
print(f"  HEX: {pkt1.hex().upper()}")

packets.append({
    'type': 'BEACON',
    'scenario': 'NORMAL',
    'sequence': sequence,
    'timestamp': timestamp,
    'data': {
        'battery_voltage': 7.8,
        'mcu_temperature': 42,
        'free_ram': 3000,
        'magnetometer': {'x': 30.2, 'y': -22.1, 'z': 38.5}
    },
    'hex': pkt1.hex().upper()
})
sequence += 1

# Payload 1
payload1 = struct.pack(
    '<IIHfffffiiHBfff',
    timestamp, 1, 1200, 0.82, 21.0, 101330.0, 15.0, 0.0,
    410644600, 289348700, 14, 1, 8.5, -1.2, 152.0
)
enc2 = encrypt_aes256_ctr(aes_key, payload1, timestamp, sequence)
pkt2 = build_csp_header() + enc2

print(f"\nPaket 2: PAYLOAD")
print(f"  Radiation: 1200 CPM | Temp: 21.0°C")
print(f"  GPS: 14 satellites")
print(f"  HEX: {pkt2.hex().upper()[:80]}...")

packets.append({
    'type': 'PAYLOAD',
    'scenario': 'NORMAL',
    'sequence': sequence,
    'timestamp': timestamp,
    'data': {
        'radiation_cpm': 1200,
        'temperature_c': 21.0,
        'gps_satellites': 14
    },
    'hex': pkt2.hex().upper()
})
sequence += 1

# ============================================================================
# SENARYO 2: DÜŞÜK BATARYA
# ============================================================================
print("\n[SENARYO 2] DÜŞÜK BATARYA")
print("-" * 80)

beacon2 = struct.pack(
    '<IBffffBH',
    timestamp + 100, 0, 5.2, 28.0, -20.0, 36.5, 48, 2200
)
enc3 = encrypt_aes256_ctr(aes_key, beacon2, timestamp + 100, sequence)
pkt3 = build_csp_header() + enc3

print(f"Paket 3: BEACON (Low battery)")
print(f"  Battery: 5.2V (DÜŞ) | Temp: 48°C")
print(f"  HEX: {pkt3.hex().upper()}")

packets.append({
    'type': 'BEACON',
    'scenario': 'LOW_BATTERY',
    'sequence': sequence,
    'timestamp': timestamp + 100,
    'data': {
        'battery_voltage': 5.2,
        'mcu_temperature': 48,
        'free_ram': 2200,
        'magnetometer': {'x': 28.0, 'y': -20.0, 'z': 36.5}
    },
    'hex': pkt3.hex().upper()
})
sequence += 1

# ============================================================================
# SENARYO 3: SAFE MODE
# ============================================================================
print("\n[SENARYO 3] SAFE MODE")
print("-" * 80)

beacon3 = struct.pack(
    '<IBffffBH',
    timestamp + 200, 1, 6.8, 15.0, -5.0, 20.0, 51, 1500
)
enc4 = encrypt_aes256_ctr(aes_key, beacon3, timestamp + 200, sequence)
pkt4 = build_csp_header() + enc4

print(f"Paket 4: BEACON (Safe mode)")
print(f"  Battery: 6.8V | Temp: 51°C | Mode: SAFE")
print(f"  HEX: {pkt4.hex().upper()}")

packets.append({
    'type': 'BEACON',
    'scenario': 'SAFE_MODE',
    'sequence': sequence,
    'timestamp': timestamp + 200,
    'data': {
        'battery_voltage': 6.8,
        'mcu_temperature': 51,
        'free_ram': 1500,
        'safe_mode': True,
        'magnetometer': {'x': 15.0, 'y': -5.0, 'z': 20.0}
    },
    'hex': pkt4.hex().upper()
})
sequence += 1

# ============================================================================
# SENARYO 4: YÜKSELİŞ
# ============================================================================
print("\n[SENARYO 4] YÜKSELİŞ (GEOMETRİK VERILER)")
print("-" * 80)

payload2 = struct.pack(
    '<IIHfffffiiHBfff',
    timestamp + 300, 2, 1350, 0.95, 18.5, 99500.0, 2500.0, 0.0,
    410500000, 289200000, 10, 1, 15.2, 8.5, 180.0
)
enc5 = encrypt_aes256_ctr(aes_key, payload2, timestamp + 300, sequence)
pkt5 = build_csp_header() + enc5

print(f"Paket 5: PAYLOAD (Yukseklikte)")
print(f"  Altitude: 2500m (YÜKSELİŞ)")
print(f"  Radiation: 1350 CPM | Temp: 18.5°C")
print(f"  HEX: {pkt5.hex().upper()[:80]}...")

packets.append({
    'type': 'PAYLOAD',
    'scenario': 'ALTITUDE',
    'sequence': sequence,
    'timestamp': timestamp + 300,
    'data': {
        'radiation_cpm': 1350,
        'temperature_c': 18.5,
        'altitude_m': 2500.0,
        'gps_satellites': 10
    },
    'hex': pkt5.hex().upper()
})
sequence += 1

# Dosyaya kaydet
print("\n" + "=" * 80)
print("VERİ SETİNİ DOSYAYA KAYDET")
print("=" * 80)

with open("custom_packets.json", 'w', encoding='utf-8') as f:
    json.dump(packets, f, indent=2, ensure_ascii=False)

print(f"\n✓ Dosya kaydedildi: custom_packets.json")
print(f"  Toplam paket: {len(packets)}")

for pkt in packets:
    print(f"  - {pkt['sequence']:2d}. {pkt['type']:7} ({pkt['scenario']:15})")

print("\n" + "=" * 80)
print("KULLANIM")
print("=" * 80)
print("""
Bu paketleri telemetry_receiver.py ile dekode etmek için:

1. Hex paketini al (custom_packets.json dan)
2. telemetry_receiver.py da process_packet() ile dekode et
3. Verileri telemetry_data.json ya da telemetry_data.csv ye kaydet

Örnek:
  packet_hex = '88A0500A244AEE60...'
  result = decoder.process_packet(packet_hex, sequence=1)
""")

print("STATUS: OK - 5 senaryo paketi olusturuldu!")
print("=" * 80)