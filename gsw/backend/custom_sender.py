#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
CubeSat Telemetri Gonderici - Interaktif Veri Gonderim
Kendi veri setini yaratıp gönder
"""

import struct
import time
import json
from datetime import datetime
from Crypto.Cipher import AES
from Crypto.Util import Counter


class TelemetrySender:
    """Kendi verilerini yaratıp şifrele ve gönder"""

    def __init__(self, aes_key_hex):
        self.aes_key = bytes.fromhex(aes_key_hex)
        self.packets_to_send = []

    def construct_iv(self, timestamp, sequence):
        """IV oluştur"""
        return struct.pack('<II', timestamp, sequence) + b'\x00' * 8

    def encrypt_aes256_ctr(self, plaintext, timestamp, sequence):
        """AES-256 CTR şifreleme"""
        iv = self.construct_iv(timestamp, sequence)
        ctr = Counter.new(128, initial_value=int.from_bytes(iv, byteorder='little'))
        cipher = AES.new(self.aes_key, AES.MODE_CTR, counter=ctr)
        return cipher.encrypt(plaintext)

    def build_csp_header(self, dest=1, dest_port=10, source_port=20):
        """CSP header oluştur"""
        header = 0
        header |= (0 & 0x03)  # Priority
        header |= (2 & 0x1F) << 2  # Source (Ground = 2)
        header |= (dest & 0x1F) << 7  # Destination (CubeSat = 1)
        header |= (dest_port & 0x3F) << 12
        header |= (source_port & 0x3F) << 18
        header |= 1 << 25  # XTEA flag
        header |= 1 << 27  # CRC flag
        return struct.pack('<I', header)

    def create_beacon(self, battery_v, mcu_temp_c, free_ram, mag_x, mag_y, mag_z, 
                     safe_mode=False, timestamp=None, sequence=1):
        """Beacon paketi oluştur"""
        if timestamp is None:
            timestamp = int(time.time())

        beacon = struct.pack(
            '<IBffffBH',
            timestamp,
            1 if safe_mode else 0,
            battery_v,
            mag_x,
            mag_y,
            mag_z,
            mcu_temp_c,
            free_ram
        )

        encrypted = self.encrypt_aes256_ctr(beacon, timestamp, sequence)
        csp_header = self.build_csp_header()
        packet = csp_header + encrypted

        return {
            'type': 'BEACON',
            'timestamp': timestamp,
            'sequence': sequence,
            'hex': packet.hex().upper(),
            'plaintext': beacon.hex().upper(),
            'data': {
                'battery_voltage': battery_v,
                'mcu_temperature': mcu_temp_c,
                'free_ram': free_ram,
                'magnetometer': {'x': mag_x, 'y': mag_y, 'z': mag_z},
                'safe_mode': safe_mode
            }
        }

    def create_payload(self, rad_cpm, dose_rate, temp_c, pressure_pa, altitude_m,
                      lat_raw, lon_raw, gps_satellites, gps_fix,
                      roll_deg, pitch_deg, yaw_deg, timestamp=None, sequence=2):
        """Payload paketi oluştur"""
        if timestamp is None:
            timestamp = int(time.time())

        payload = struct.pack(
            '<IIHfffffiiHBfff',
            timestamp,
            sequence,
            rad_cpm,
            dose_rate,
            temp_c,
            pressure_pa,
            altitude_m,
            0.0,  # Extra field
            lat_raw,
            lon_raw,
            gps_satellites,
            gps_fix,
            roll_deg,
            pitch_deg,
            yaw_deg
        )

        encrypted = self.encrypt_aes256_ctr(payload, timestamp, sequence)
        csp_header = self.build_csp_header()
        packet = csp_header + encrypted

        return {
            'type': 'PAYLOAD',
            'timestamp': timestamp,
            'sequence': sequence,
            'hex': packet.hex().upper(),
            'plaintext': payload.hex().upper(),
            'data': {
                'radiation_cpm': rad_cpm,
                'dose_rate': dose_rate,
                'temperature': temp_c,
                'pressure': pressure_pa,
                'altitude': altitude_m,
                'gps': {
                    'latitude': lat_raw / 1e7,
                    'longitude': lon_raw / 1e7,
                    'satellites': gps_satellites
                },
                'orientation': {
                    'roll': roll_deg,
                    'pitch': pitch_deg,
                    'yaw': yaw_deg
                }
            }
        }

    def save_packets(self, filename="custom_packets.json"):
        """Paketleri dosyaya kaydet"""
        with open(filename, 'w', encoding='utf-8') as f:
            json.dump(self.packets_to_send, f, indent=2, ensure_ascii=False)
        return filename


def interactive_beacon_input():
    """Beacon verilerini kullanıcıdan sor"""
    print("\n" + "=" * 60)
    print("BEACON PAKETI OLUSTUR")
    print("=" * 60)

    try:
        battery = float(input("\nBatarya Voltajı (V) [7.4]: ") or "7.4")
        mcu_temp = int(input("MCU Sıcaklığı (°C) [45]: ") or "45")
        free_ram = int(input("Boş RAM (bytes) [2048]: ") or "2048")
        mag_x = float(input("Manyetometre X (mT) [25.5]: ") or "25.5")
        mag_y = float(input("Manyetometre Y (mT) [-18.3]: ") or "-18.3")
        mag_z = float(input("Manyetometre Z (mT) [42.1]: ") or "42.1")
        safe_mode = input("Safe Mode? (e/h) [h]: ").lower() == 'e'

        return {
            'battery': battery,
            'mcu_temp': mcu_temp,
            'free_ram': free_ram,
            'mag_x': mag_x,
            'mag_y': mag_y,
            'mag_z': mag_z,
            'safe_mode': safe_mode
        }
    except ValueError:
        print("HATA: Geçersiz input!")
        return None


def interactive_payload_input():
    """Payload verilerini kullanıcıdan sor"""
    print("\n" + "=" * 60)
    print("PAYLOAD PAKETI OLUSTUR")
    print("=" * 60)

    try:
        rad_cpm = int(input("\nRadyasyon (CPM) [1250]: ") or "1250")
        dose_rate = float(input("Doz Hızı (µSv/h) [0.85]: ") or "0.85")
        temp = float(input("Sıcaklık (°C) [22.5]: ") or "22.5")
        pressure = float(input("Basınç (Pa) [101325]: ") or "101325")
        altitude = float(input("Yükseklik (m) [0.0]: ") or "0.0")
        
        print("\nGPS Bilgileri:")
        lat_str = input("Enlem (derece) [41.0644600]: ") or "41.0644600"
        lon_str = input("Boylam (derece) [28.9348700]: ") or "28.9348700"
        lat_raw = int(float(lat_str) * 1e7)
        lon_raw = int(float(lon_str) * 1e7)
        
        satellites = int(input("Uydu Sayısı [12]: ") or "12")
        
        print("\nOryantasyon:")
        roll = float(input("Roll (°) [5.2]: ") or "5.2")
        pitch = float(input("Pitch (°) [-2.8]: ") or "-2.8")
        yaw = float(input("Yaw (°) [145.3]: ") or "145.3")

        return {
            'rad_cpm': rad_cpm,
            'dose_rate': dose_rate,
            'temp': temp,
            'pressure': pressure,
            'altitude': altitude,
            'lat_raw': lat_raw,
            'lon_raw': lon_raw,
            'satellites': satellites,
            'roll': roll,
            'pitch': pitch,
            'yaw': yaw
        }
    except ValueError:
        print("HATA: Geçersiz input!")
        return None


def interactive_menu():
    """İnteraktif menü"""
    print("\n" + "=" * 80)
    print("CUBESAT TELEMETRI GONDERICI - Kendi Veri Setini Olustur")
    print("=" * 80)

    # AES Key
    aes_key = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
    sender = TelemetrySender(aes_key)

    sequence = 1

    while True:
        print("\n" + "=" * 80)
        print("MENU")
        print("=" * 80)
        print("1. Beacon Paket Olustur")
        print("2. Payload Paket Olustur")
        print("3. Paketleri Goster")
        print("4. Paketleri Dosyaya Kaydet ve Cikis")
        print("5. Cikis")

        choice = input("\nSecim (1-5): ").strip()

        if choice == '1':
            data = interactive_beacon_input()
            if data:
                packet = sender.create_beacon(
                    data['battery'], data['mcu_temp'], data['free_ram'],
                    data['mag_x'], data['mag_y'], data['mag_z'],
                    data['safe_mode'], sequence=sequence
                )
                sender.packets_to_send.append(packet)
                sequence += 1

                print(f"\n✓ Beacon paketi olusturuldu!")
                print(f"  Sequence: {packet['sequence']}")
                print(f"  HEX: {packet['hex'][:80]}...")

        elif choice == '2':
            data = interactive_payload_input()
            if data:
                packet = sender.create_payload(
                    data['rad_cpm'], data['dose_rate'], data['temp'],
                    data['pressure'], data['altitude'],
                    data['lat_raw'], data['lon_raw'],
                    data['satellites'], 1,
                    data['roll'], data['pitch'], data['yaw'],
                    sequence=sequence
                )
                sender.packets_to_send.append(packet)
                sequence += 1

                print(f"\n✓ Payload paketi olusturuldu!")
                print(f"  Sequence: {packet['sequence']}")
                print(f"  HEX: {packet['hex'][:80]}...")

        elif choice == '3':
            if not sender.packets_to_send:
                print("\nHenüz paket yok!")
            else:
                print("\n" + "=" * 80)
                print("OLUSTURULAN PAKETLER")
                print("=" * 80)

                for i, pkt in enumerate(sender.packets_to_send, 1):
                    print(f"\n[{i}] {pkt['type']} (Sequence: {pkt['sequence']})")
                    print(f"  Timestamp: {datetime.fromtimestamp(pkt['timestamp']).isoformat()}")
                    print(f"  HEX: {pkt['hex']}")

                    if pkt['type'] == 'BEACON':
                        d = pkt['data']
                        print(f"  Batarya: {d['battery_voltage']}V")
                        print(f"  CPU Temp: {d['mcu_temperature']}°C")
                        print(f"  Manyetom: X={d['magnetometer']['x']}, " +
                              f"Y={d['magnetometer']['y']}, Z={d['magnetometer']['z']}")

                    elif pkt['type'] == 'PAYLOAD':
                        d = pkt['data']
                        print(f"  Radyasyon: {d['radiation_cpm']} CPM ({d['dose_rate']} µSv/h)")
                        print(f"  Sicaklik: {d['temperature']}°C")
                        print(f"  GPS: {d['gps']['latitude']:.7f}, {d['gps']['longitude']:.7f}")

        elif choice == '4':
            if sender.packets_to_send:
                filename = sender.save_packets("custom_packets.json")
                print(f"\n✓ Paketler kaydedildi: {filename}")
                print(f"  Toplam paket: {len(sender.packets_to_send)}")
            else:
                print("\nHenüz paket yok!")
            break

        elif choice == '5':
            print("\nCıkılıyor...")
            break

        else:
            print("\nGeçersiz seçim!")


if __name__ == "__main__":
    interactive_menu()
