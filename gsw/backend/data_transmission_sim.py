#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Cubesat Data Transmission Simulator
Ground Station -> CubeSat ornek veri gonderimi
"""

import struct
import socket
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


def decrypt_aes256_ctr(key, ciphertext, timestamp, sequence):
    """AES-256 CTR sifre cozme"""
    iv = construct_iv(timestamp, sequence)
    ctr = Counter.new(128, initial_value=int.from_bytes(iv, byteorder='little'))
    cipher = AES.new(key, AES.MODE_CTR, counter=ctr)
    return cipher.decrypt(ciphertext)


def build_csp_header(dest=1, dest_port=10, source_port=20):
    """CSP header olustur"""
    CSP_PRIORITY = 0
    CSP_SOURCE = 2
    CSP_DEST = 7
    CSP_DEST_PORT = 12
    CSP_SOURCE_PORT = 18
    CSP_FLAGS_XTEA = 25
    CSP_FLAGS_CRC = 27

    header = 0
    header |= (0 & 0x03)
    header |= (2 & 0x1F) << CSP_SOURCE  # Ground = 2
    header |= (dest & 0x1F) << CSP_DEST  # CubeSat = 1
    header |= (dest_port & 0x3F) << CSP_DEST_PORT
    header |= (source_port & 0x3F) << CSP_SOURCE_PORT
    header |= 1 << CSP_FLAGS_XTEA
    header |= 1 << CSP_FLAGS_CRC
    return struct.pack('<I', header)


def create_beacon_packet(timestamp, sequence):
    """
    Beacon paketi olustur (37 bytes)
    Sensorlerden alinan verilerle doldur
    """
    # Sanal sensor verileri
    safe_mode = 0  # Normal mode
    battery_voltage = 7.4  # Volts
    mag_x = 25.5
    mag_y = -18.3
    mag_z = 42.1
    mcu_temp = 45  # Celsius
    free_ram = 2048  # Bytes
    task_health = 0x0F  # All 4 tasks healthy (bits: ADCS, Payload, Comm, EPS)
    error_flags = 0x00
    uptime = 3600  # 1 hour

    beacon = struct.pack(
        '<IBffffBBBBBH',
        timestamp,
        safe_mode,
        battery_voltage,
        mag_x,
        mag_y,
        mag_z,
        mcu_temp,
        free_ram,
        task_health,
        error_flags,
        uptime
    )

    return beacon


def create_payload_packet(timestamp, sequence_num):
    """
    Payload (telemetri) paketi olustur (60+ bytes)
    Bilimsel veriler
    """
    sample_counter = sequence_num
    radiation_cpm = 1250  # Counts per minute
    radiation_dose = 0.85  # microSv/h
    temp_celsius = 22.5
    pressure_pa = 101325.0  # Sea level pressure
    altitude_m = 0.0
    lat_raw = 410644600  # 41.0644600 degrees N (1E-7 format)
    lon_raw = 289348700  # 28.9348700 degrees E
    gps_satellites = 12
    gps_fix = 1  # Valid fix
    euler_roll = 5.2
    euler_pitch = -2.8
    euler_yaw = 145.3

    payload = struct.pack(
        '<IBffffBBBBBHffffBBBBH',
        timestamp,
        sample_counter,
        radiation_cpm,
        radiation_dose,
        temp_celsius,
        pressure_pa,
        altitude_m,
        lat_raw,
        lon_raw,
        gps_satellites,
        gps_fix,
        euler_roll,
        euler_pitch,
        euler_yaw,
        0, 0, 0, 0  # rezerv alanlar
    )

    return payload


def parse_beacon(data):
    """Beacon paketini parse et"""
    if len(data) < 37:
        return None
    unpacked = struct.unpack('<IBffffBBBBBH', data)
    return {
        'timestamp': unpacked[0],
        'safe_mode': bool(unpacked[1]),
        'battery_voltage': unpacked[2],
        'magnetometer': {
            'x': unpacked[3],
            'y': unpacked[4],
            'z': unpacked[5]
        },
        'mcu_temp': unpacked[6],
        'free_ram': unpacked[7],
        'task_health': unpacked[8],
        'error_flags': unpacked[9],
        'uptime_seconds': unpacked[10]
    }


def parse_payload(data):
    """Payload paketini parse et"""
    if len(data) < 60:
        return None
    unpacked = struct.unpack('<IBffffBBBBBHffffBBBBH', data)
    return {
        'timestamp': unpacked[0],
        'sample_counter': unpacked[1],
        'radiation': {'cpm': unpacked[2], 'dose_rate': unpacked[3]},
        'temperature': unpacked[4],
        'pressure': unpacked[5],
        'altitude': unpacked[6],
        'gps': {
            'lat': unpacked[7] / 1e7,
            'lon': unpacked[8] / 1e7,
            'satellites': unpacked[9],
            'fix_valid': bool(unpacked[10])
        },
        'euler': {
            'roll': unpacked[11],
            'pitch': unpacked[12],
            'yaw': unpacked[13]
        }
    }


# Main Test
print("=" * 80)
print("CUBESAT VERI GONDERIMI - Ground Station Simulator")
print("=" * 80)

test_key = bytes.fromhex("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef")

timestamp = int(time.time())
print(f"\n[ZAMAN] {datetime.fromtimestamp(timestamp).strftime('%Y-%m-%d %H:%M:%S')}")

# ============================================================================
# PAKET 1: BEACON - Sistem Saglik Beaconı
# ============================================================================
print("\n" + "=" * 80)
print("PAKET 1: BEACON GONDERIM (Sistem Sagligi)")
print("=" * 80)

beacon_data = create_beacon_packet(timestamp, 1)
beacon_str = parse_beacon(beacon_data)

print(f"\n📊 Beacon Verisi:")
print(f"  Battery: {beacon_str['battery_voltage']:.1f}V")
print(f"  MCU Temp: {beacon_str['mcu_temp']}°C")
print(f"  Free RAM: {beacon_str['free_ram']} bytes")
print(f"  Magnetometer: X={beacon_str['magnetometer']['x']:.2f}, " +
      f"Y={beacon_str['magnetometer']['y']:.2f}, " +
      f"Z={beacon_str['magnetometer']['z']:.2f} mT")
print(f"  Uptime: {beacon_str['uptime_seconds']} seconds")
print(f"  Safe Mode: {'SAFE' if beacon_str['safe_mode'] else 'NORMAL'}")

# Sifreleme
encrypted_beacon = encrypt_aes256_ctr(test_key, beacon_data, timestamp, 1)
csp_header = build_csp_header()
beacon_packet = csp_header + encrypted_beacon

print(f"\n🔐 Sifreleme:")
print(f"  Plaintext: {beacon_data.hex().upper()[:64]}...")
print(f"  Encrypted: {encrypted_beacon.hex().upper()[:64]}...")
print(f"  CSP Header: {csp_header.hex().upper()}")

print(f"\n📦 Final Paket (HEX):")
print(f"  {beacon_packet.hex().upper()}")
print(f"  Boyut: {len(beacon_packet)} bytes")

# ============================================================================
# PAKET 2: PAYLOAD - Bilimsel Veriler
# ============================================================================
print("\n" + "=" * 80)
print("PAKET 2: PAYLOAD GONDERIM (Bilimsel Veriler)")
print("=" * 80)

payload_data = create_payload_packet(timestamp, 2)
payload_str = parse_payload(payload_data)

print(f"\n📡 Payload Verisi:")
print(f"  Radiation: {payload_str['radiation']['cpm']} CPM ({payload_str['radiation']['dose_rate']} µSv/h)")
print(f"  Temperature: {payload_str['temperature']:.1f}°C")
print(f"  Pressure: {payload_str['pressure']:.0f} Pa")
print(f"  Altitude: {payload_str['altitude']:.0f} m")
print(f"  GPS Position: {payload_str['gps']['lat']:.7f}°N, {payload_str['gps']['lon']:.7f}°E")
print(f"  GPS Satellites: {payload_str['gps']['satellites']}")
print(f"  Orientation: Roll={payload_str['euler']['roll']:.1f}°, " +
      f"Pitch={payload_str['euler']['pitch']:.1f}°, " +
      f"Yaw={payload_str['euler']['yaw']:.1f}°")

# Sifreleme
encrypted_payload = encrypt_aes256_ctr(test_key, payload_data, timestamp, 2)
payload_packet = csp_header + encrypted_payload

print(f"\n🔐 Sifreleme:")
print(f"  Plaintext Boyut: {len(payload_data)} bytes")
print(f"  Encrypted Boyut: {len(encrypted_payload)} bytes")
print(f"  Plaintext (ilk 32B): {payload_data[:32].hex().upper()}...")
print(f"  Encrypted (ilk 32B): {encrypted_payload[:32].hex().upper()}...")

print(f"\n📦 Final Paket (HEX - ilk 64 char):")
print(f"  {payload_packet.hex().upper()[:64]}...")
print(f"  Boyut: {len(payload_packet)} bytes")

# ============================================================================
# PAKET 3: TELEMETRI - Detayli Sistem Durumu
# ============================================================================
print("\n" + "=" * 80)
print("PAKET 3: SISTEM DURUMU TELEMETRISI")
print("=" * 80)

# Detayli telemetri (beacon + payload birlestirilmis)
combined_data = beacon_data + payload_data
encrypted_combined = encrypt_aes256_ctr(test_key, combined_data, timestamp, 3)
combined_packet = csp_header + encrypted_combined

print(f"\n📊 Kombinli Telemetri:")
print(f"  Beacon: {len(beacon_data)} bytes")
print(f"  Payload: {len(payload_data)} bytes")
print(f"  Toplam: {len(combined_data)} bytes")

print(f"\n📦 Sifrelenmis Paket:")
print(f"  Boyut: {len(combined_packet)} bytes")
print(f"  Hex (ilk 100 char): {combined_packet.hex().upper()[:100]}...")

# ============================================================================
# OZET
# ============================================================================
print("\n" + "=" * 80)
print("OZET - 3 PAKET GONDERILDI")
print("=" * 80)

summary_data = [
    ("Beacon (Sistem Sagligi)", len(beacon_packet), "15 sn aralik"),
    ("Payload (Bilimsel)", len(payload_packet), "1-10 dakika aralik"),
    ("Telemetri (Detayli)", len(combined_packet), "Talep sonrasi")
]

for name, size, interval in summary_data:
    print(f"  ✓ {name:30} | {size:3} bytes | {interval}")

print(f"\n🌍 Yer Istasyonu Alıyor:")
print(f"  Frekans: 437.270 MHz")
print(f"  Modülasyon: LORA (RFM98W)")
print(f"  Baud: 9600")
print(f"  Encryption: AES-256 CTR")

print(f"\n📥 Uydu Alırsa:")
print(f"  1. CSP header parse")
print(f"  2. AES-256 şifre çöz")
print(f"  3. CRC kontrol")
print(f"  4. İlgili systeme yönlendir")
print(f"  5. Yanıt/ACK gönder")

print("\n" + "=" * 80)
print("✅ Veriler hazır! Gonderim simulasyonu tamamlandi.")
print("=" * 80)
