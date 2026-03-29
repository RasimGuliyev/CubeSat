#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
CubeSat Telemetri Alıcı ve Kayıt Sistemi
Gelen verileri deşifre edip dosyaya kaydet
"""

import struct
import time
import json
from datetime import datetime
from pathlib import Path
from Crypto.Cipher import AES
from Crypto.Util import Counter


class TelemetryDecoder:
    """Gelen telemetri paketlerini deşifre edip parse et"""

    def __init__(self, aes_key_hex):
        self.aes_key = bytes.fromhex(aes_key_hex)
        self.received_packets = []

    def construct_iv(self, timestamp, sequence):
        """IV oluştur: timestamp(4B) + sequence(4B) + padding(8B)"""
        return struct.pack('<II', timestamp, sequence) + b'\x00' * 8

    def decrypt_aes256_ctr(self, ciphertext, timestamp, sequence):
        """AES-256 CTR şifre çöz"""
        iv = self.construct_iv(timestamp, sequence)
        ctr = Counter.new(128, initial_value=int.from_bytes(iv, byteorder='little'))
        cipher = AES.new(self.aes_key, AES.MODE_CTR, counter=ctr)
        return cipher.decrypt(ciphertext)

    def parse_csp_header(self, header_bytes):
        """CSP header'ını parse et"""
        if len(header_bytes) != 4:
            return None
        header = struct.unpack('<I', header_bytes)[0]
        
        return {
            'priority': header & 0x03,
            'source': (header >> 2) & 0x1F,
            'destination': (header >> 7) & 0x1F,
            'dest_port': (header >> 12) & 0x3F,
            'source_port': (header >> 18) & 0x3F,
            'xtea_flag': bool(header & (1 << 25)),
            'crc_flag': bool(header & (1 << 27))
        }

    def parse_beacon(self, data):
        """Beacon paketini parse et (24 bytes)"""
        if len(data) < 24:
            return None
        
        try:
            unpacked = struct.unpack('<IBffffBH', data[:24])
            return {
                'type': 'BEACON',
                'timestamp': unpacked[0],
                'safe_mode': bool(unpacked[1]),
                'battery_voltage': round(unpacked[2], 2),
                'magnetometer': {
                    'x': round(unpacked[3], 4),
                    'y': round(unpacked[4], 4),
                    'z': round(unpacked[5], 4)
                },
                'mcu_temperature': unpacked[6],
                'free_ram': unpacked[7],
                'timestamp_human': datetime.fromtimestamp(unpacked[0]).isoformat()
            }
        except:
            return None

    def parse_payload(self, data):
        """Payload paketini parse et (53 bytes)"""
        if len(data) < 52:
            return None
        
        try:
            unpacked = struct.unpack('<IIHfffffiiHBfff', data[:52])
            return {
                'type': 'PAYLOAD',
                'timestamp': unpacked[0],
                'sample_counter': unpacked[1],
                'radiation': {
                    'cpm': unpacked[2],
                    'dose_rate_usv_h': round(unpacked[3], 4)
                },
                'temperature_c': round(unpacked[4], 2),
                'pressure_pa': round(unpacked[5], 0),
                'altitude_m': round(unpacked[6], 2),
                'gps': {
                    'latitude': round(unpacked[7] / 1e7, 7),
                    'longitude': round(unpacked[8] / 1e7, 7),
                    'satellites': unpacked[9],
                    'fix_valid': bool(unpacked[10])
                },
                'orientation': {
                    'roll_deg': round(unpacked[11], 2),
                    'pitch_deg': round(unpacked[12], 2),
                    'yaw_deg': round(unpacked[13], 2)
                },
                'timestamp_human': datetime.fromtimestamp(unpacked[0]).isoformat()
            }
        except:
            return None

    def process_packet(self, hex_packet, sequence=1):
        """Paket işle ve verileri çıkar"""
        try:
            # Hex'ten bytes'a çevir
            packet_bytes = bytes.fromhex(hex_packet)
            
            # CSP header ayır
            csp_header_bytes = packet_bytes[:4]
            encrypted_data = packet_bytes[4:]
            
            # Header parse et
            csp_header = self.parse_csp_header(csp_header_bytes)
            
            # İlk 8 byte timestamp + sequence
            if len(encrypted_data) < 8:
                return None
            
            timestamp, seq = struct.unpack('<II', encrypted_data[:8])
            
            # Şifre çöz
            decrypted = self.decrypt_aes256_ctr(encrypted_data, timestamp, seq)
            
            # Beacon vs Payload kontrol et
            beacon = self.parse_beacon(decrypted)
            if beacon:
                beacon['csp_header'] = csp_header
                self.received_packets.append(beacon)
                return beacon
            
            payload = self.parse_payload(decrypted)
            if payload:
                payload['csp_header'] = csp_header
                self.received_packets.append(payload)
                return payload
            
            return None
        except Exception as e:
            print(f"Hata: {e}")
            return None

    def save_to_json(self, filename="telemetry_data.json"):
        """Verileri JSON dosyasına kaydet"""
        output_path = Path(filename)
        
        # Var olan verileri yükle
        if output_path.exists():
            with open(output_path, 'r', encoding='utf-8') as f:
                existing_data = json.load(f)
        else:
            existing_data = {'packets': []}
        
        # Yeni verileri ekle
        existing_data['packets'].extend(self.received_packets)
        
        # Kaydet
        with open(output_path, 'w', encoding='utf-8') as f:
            json.dump(existing_data, f, indent=2, ensure_ascii=False)
        
        return output_path

    def save_to_csv(self, filename="telemetry_data.csv"):
        """Verileri CSV dosyasına kaydet"""
        import csv
        
        output_path = Path(filename)
        
        # CSV header'ı belirle
        csv_exists = output_path.exists()
        
        with open(output_path, 'a', newline='', encoding='utf-8') as f:
            writer = csv.DictWriter(
                f,
                fieldnames=[
                    'type', 'timestamp', 'timestamp_human', 'battery_voltage',
                    'mcu_temperature', 'free_ram', 'radiation_cpm',
                    'temperature_c', 'pressure_pa', 'altitude_m',
                    'gps_latitude', 'gps_longitude', 'gps_satellites',
                    'roll_deg', 'pitch_deg', 'yaw_deg'
                ]
            )
            
            if not csv_exists:
                writer.writeheader()
            
            for packet in self.received_packets:
                row = {
                    'type': packet.get('type'),
                    'timestamp': packet.get('timestamp'),
                    'timestamp_human': packet.get('timestamp_human'),
                    'battery_voltage': packet.get('battery_voltage', ''),
                    'mcu_temperature': packet.get('mcu_temperature', ''),
                    'free_ram': packet.get('free_ram', ''),
                    'radiation_cpm': packet.get('radiation', {}).get('cpm', ''),
                    'temperature_c': packet.get('temperature_c', ''),
                    'pressure_pa': packet.get('pressure_pa', ''),
                    'altitude_m': packet.get('altitude_m', ''),
                    'gps_latitude': packet.get('gps', {}).get('latitude', ''),
                    'gps_longitude': packet.get('gps', {}).get('longitude', ''),
                    'gps_satellites': packet.get('gps', {}).get('satellites', ''),
                    'roll_deg': packet.get('orientation', {}).get('roll_deg', ''),
                    'pitch_deg': packet.get('orientation', {}).get('pitch_deg', ''),
                    'yaw_deg': packet.get('orientation', {}).get('yaw_deg', '')
                }
                writer.writerow(row)
        
        return output_path


# TEST VERİSETİ
def main():
    print("=" * 80)
    print("CUBESAT TELEMETRI ALICI - Veri Deşifre ve Kayıt Sistemi")
    print("=" * 80)
    
    # AES Key
    test_key = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
    decoder = TelemetryDecoder(test_key)
    
    # Simüle edilen gelen paketler (öncekilerden)
    received_packets = [
        {
            'hex': '88A0500A244AEE604E5DB4464BAB47D58DDC2FEC11F3AEC4A8D2AAE2',
            'seq': 1,
            'name': 'BEACON #1'
        },
        {
            'hex': '88A0500AABB7CE5E8156A20E2BDBF9CEF820D5AD15D4E8D204734DE317CFB58BDA9996050E236AB9',
            'seq': 2,
            'name': 'PAYLOAD #1'
        }
    ]
    
    print("\n[1] PAKETLERI AL VE DESIFRELE")
    print("-" * 80)
    
    for packet_info in received_packets:
        print(f"\nAlınan: {packet_info['name']}")
        print(f"Hex: {packet_info['hex'][:64]}...")
        
        result = decoder.process_packet(packet_info['hex'], packet_info['seq'])
        
        if result:
            print(f"Deşifre: BAŞARILI ✓")
            print(f"Tür: {result['type']}")
            print(f"Zaman: {result['timestamp_human']}")
            
            if result['type'] == 'BEACON':
                print(f"Batarya: {result['battery_voltage']}V")
                print(f"CPU Temp: {result['mcu_temperature']}°C")
                print(f"RAM: {result['free_ram']} bytes")
                print(f"Manyetometre: X={result['magnetometer']['x']}, " +
                      f"Y={result['magnetometer']['y']}, " +
                      f"Z={result['magnetometer']['z']} mT")
            
            elif result['type'] == 'PAYLOAD':
                print(f"Radyasyon: {result['radiation']['cpm']} CPM " +
                      f"({result['radiation']['dose_rate_usv_h']} µSv/h)")
                print(f"Sıcaklık: {result['temperature_c']}°C")
                print(f"GPS: {result['gps']['latitude']:.7f}°N, " +
                      f"{result['gps']['longitude']:.7f}°E (Uydular: {result['gps']['satellites']})")
                print(f"Oryantasyon: Roll={result['orientation']['roll_deg']}°, " +
                      f"Pitch={result['orientation']['pitch_deg']}°, " +
                      f"Yaw={result['orientation']['yaw_deg']}°")
        else:
            print(f"Deşifre: HATA ✗")
    
    # Dosyalara kaydet
    print("\n" + "=" * 80)
    print("[2] VERİLERİ DOSYALARA KAYDET")
    print("-" * 80)
    
    json_path = decoder.save_to_json("telemetry_data.json")
    print(f"\n✓ JSON dosyası: {json_path}")
    print(f"  Paket sayısı: {len(decoder.received_packets)}")
    
    csv_path = decoder.save_to_csv("telemetry_data.csv")
    print(f"\n✓ CSV dosyası: {csv_path}")
    print(f"  Paket sayısı: {len(decoder.received_packets)}")
    
    # Kaydedilen verileri göster
    print("\n" + "=" * 80)
    print("[3] KAYDEDILEN VERİLER (JSON)")
    print("-" * 80)
    
    with open("telemetry_data.json", 'r', encoding='utf-8') as f:
        data = json.load(f)
        print(json.dumps(data, indent=2, ensure_ascii=False)[:1000] + "...")
    
    # İstatistikler
    print("\n" + "=" * 80)
    print("TELEMETRI İSTATİSTİKLERİ")
    print("=" * 80)
    
    beacon_count = sum(1 for p in decoder.received_packets if p['type'] == 'BEACON')
    payload_count = sum(1 for p in decoder.received_packets if p['type'] == 'PAYLOAD')
    
    print(f"\nAlınan paketler:")
    print(f"  Beacon:  {beacon_count}")
    print(f"  Payload: {payload_count}")
    print(f"  Toplam:  {len(decoder.received_packets)}")
    
    print(f"\nDosya konumları:")
    print(f"  JSON:  {json_path.absolute()}")
    print(f"  CSV:   {csv_path.absolute()}")
    
    print("\n" + "=" * 80)
    print("STATUS: OK - Veriler başarıyla kaydedildi!")
    print("=" * 80)


if __name__ == "__main__":
    main()
