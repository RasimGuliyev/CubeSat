#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Test: Komut gonder ve sifre
Ulas-S1 CubeSat - Ground Station komut test scripti
"""

import struct
import time
from datetime import datetime
from Crypto.Cipher import AES
from Crypto.Util import Counter


class CommandGenerator:
    """Uydunun anlayacağı şekilde komut paketi oluştur"""

    # Komut türleri (commands.h'dan)
    CMD_INVALID = 0x00
    CMD_MODE_CHANGE = 0x01
    CMD_RESET_SYSTEM = 0x02
    CMD_DATA_REQUEST = 0x03
    CMD_CONFIG_ADCS = 0x04
    CMD_CONFIG_PAYLOAD = 0x05
    CMD_SD_CARD_DUMP = 0x06
    CMD_CLEAR_BUFFER = 0x07
    CMD_ENABLE_LOG = 0x08
    CMD_GET_SYSTEM_STATUS = 0x09
    CMD_ACK = 0xFF

    # CSP Header bit shifts
    CSP_PRIORITY = 0  # Priority (2 bits)
    CSP_SOURCE = 2    # Source (5 bits)
    CSP_DEST = 7      # Destination (5 bits)
    CSP_DEST_PORT = 12      # Destination port (6 bits)
    CSP_SOURCE_PORT = 18    # Source port (6 bits)
    CSP_FLAGS_HMAC = 24
    CSP_FLAGS_XTEA = 25    # Encryption flag
    CSP_FLAGS_RDP = 26
    CSP_FLAGS_CRC = 27

    def __init__(self, aes_key_hex: str):
        """Initialize with AES-256 key (64 hex chars = 32 bytes)"""
        if len(aes_key_hex) != 64:
            raise ValueError("AES-256 key must be 64 hex characters")
        self.aes_key = bytes.fromhex(aes_key_hex)

    def crc8(self, data: bytes) -> int:
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

    def construct_iv(self, timestamp: int, sequence: int) -> bytes:
        """IV = timestamp(4B) + sequence(4B) + padding(8B)"""
        return struct.pack('<II', timestamp, sequence) + b'\x00' * 8

    def encrypt_payload(self, payload: bytes, timestamp: int, sequence: int) -> bytes:
        """AES-256 CTR şifreleme"""
        iv = self.construct_iv(timestamp, sequence)
        # CTR modunda IV ilk 16 byte
        ctr = Counter.new(128, initial_value=int.from_bytes(iv, byteorder='little'))
        cipher = AES.new(self.aes_key, AES.MODE_CTR, counter=ctr)
        return cipher.encrypt(payload)

    def build_csp_header(self, dest: int = 1, dest_port: int = 10, source_port: int = 20, flags_xtea: bool = True) -> bytes:
        """CSP header oluştur"""
        header = 0
        header |= (0 & 0x03)  # Priority = 0
        header |= (2 & 0x1F) << self.CSP_SOURCE  # Source = 2 (Ground)
        header |= (dest & 0x1F) << self.CSP_DEST  # Destination = 1 (CubeSat)
        header |= (dest_port & 0x3F) << self.CSP_DEST_PORT
        header |= (source_port & 0x3F) << self.CSP_SOURCE_PORT
        if flags_xtea:
            header |= 1 << self.CSP_FLAGS_XTEA
        header |= 1 << self.CSP_FLAGS_CRC
        return struct.pack('<I', header)

    def create_command(self, cmd_type: int, timestamp: int = None, sequence: int = 0, payload_data: bytes = b'') -> dict:
        """Komut paketi oluştur"""
        if timestamp is None:
            timestamp = int(time.time())

        # Command header yapısı (13 bytes minimum)
        cmd_header = struct.pack(
            '<IBBHBxxx',  # timestamp, cmd_id, seq_num, payload_len, checksum, padding
            timestamp,
            cmd_type,
            sequence,
            len(payload_data),
            0  # CRC placeholder
        )

        # CRC8 hesapla
        crc = self.crc8(cmd_header[:8] + payload_data)
        cmd_header = struct.pack(
            '<IBBHBxxx',
            timestamp,
            cmd_type,
            sequence,
            len(payload_data),
            crc
        )

        full_payload = cmd_header + payload_data

        # Şifrele
        encrypted = self.encrypt_payload(full_payload, timestamp, sequence)

        # CSP paketine sok
        csp_header = self.build_csp_header()

        packet = csp_header + encrypted

        return {
            'timestamp': timestamp,
            'command_type': cmd_type,
            'command_name': self._cmd_name(cmd_type),
            'sequence': sequence,
            'crc': crc,
            'csp_header': csp_header.hex().upper(),
            'encrypted_payload': encrypted.hex().upper(),
            'full_packet': packet.hex().upper(),
            'packet_size': len(packet),
            'created_at': datetime.now().isoformat()
        }

    def _cmd_name(self, cmd_type: int) -> str:
        """Komut adını getir"""
        names = {
            self.CMD_MODE_CHANGE: "MODE_CHANGE",
            self.CMD_RESET_SYSTEM: "RESET_SYSTEM",
            self.CMD_DATA_REQUEST: "DATA_REQUEST",
            self.CMD_CONFIG_ADCS: "CONFIG_ADCS",
            self.CMD_CONFIG_PAYLOAD: "CONFIG_PAYLOAD",
            self.CMD_SD_CARD_DUMP: "SD_CARD_DUMP",
            self.CMD_CLEAR_BUFFER: "CLEAR_BUFFER",
            self.CMD_ENABLE_LOG: "ENABLE_LOG",
            self.CMD_GET_SYSTEM_STATUS: "GET_SYSTEM_STATUS",
        }
        return names.get(cmd_type, "UNKNOWN")


def main():
    """Test: Sistem durumunu sor"""
    print("=" * 70)
    print("CUBESAT KOMUT TEST - Sistem Durumu Sorgulama")
    print("=" * 70)

    # TEST AES-256 KEY (24 saat içinde değiştirilmesi gerekir!)
    # Gerçek anahtarı config dosyasından al
    test_key = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"

    try:
        generator = CommandGenerator(test_key)

        # TEST 1: Sistem Durumunu Sor
        print("\n[TEST 1] CMD_GET_SYSTEM_STATUS Gönder")
        print("-" * 70)
        cmd1 = generator.create_command(
            cmd_type=CommandGenerator.CMD_GET_SYSTEM_STATUS,
            sequence=1
        )

        print(f"⏱️  Timestamp: {cmd1['timestamp']} ({datetime.fromtimestamp(cmd1['timestamp']).strftime('%Y-%m-%d %H:%M:%S')})")
        print(f"📋 Komut: {cmd1['command_name']} (0x{cmd1['command_type']:02X})")
        print(f"🔐 Sequence: {cmd1['sequence']}")
        print(f"✓  CRC8: 0x{cmd1['crc']:02X}")
        print(f"📦 Paket Boyutu: {cmd1['packet_size']} bytes")
        print(f"\n🔗 CSP Header: {cmd1['csp_header']}")
        print(f"🔐 Encrypted Payload: {cmd1['encrypted_payload'][:64]}...")
        print(f"\n✅ FULL PACKET HEX:\n{cmd1['full_packet']}")

        # TEST 2: Veri Talep Et
        print("\n\n[TEST 2] CMD_DATA_REQUEST Gönder")
        print("-" * 70)
        cmd2 = generator.create_command(
            cmd_type=CommandGenerator.CMD_DATA_REQUEST,
            sequence=2
        )

        print(f"⏱️  Timestamp: {cmd2['timestamp']}")
        print(f"📋 Komut: {cmd2['command_name']} (0x{cmd2['command_type']:02X})")
        print(f"🔐 Sequence: {cmd2['sequence']}")
        print(f"✓  CRC8: 0x{cmd2['crc']:02X}")
        print(f"📦 Paket Boyutu: {cmd2['packet_size']} bytes")
        print(f"\n✅ FULL PACKET HEX:\n{cmd2['full_packet']}")

        # TEST 3: MODE CHANGE - Safe Mode'a geç
        print("\n\n[TEST 3] CMD_MODE_CHANGE -> Safe Mode")
        print("-" * 70)
        mode_payload = struct.pack('B', 1)  # 1 = Safe Mode
        cmd3 = generator.create_command(
            cmd_type=CommandGenerator.CMD_MODE_CHANGE,
            sequence=3,
            payload_data=mode_payload
        )

        print(f"⏱️  Timestamp: {cmd3['timestamp']}")
        print(f"📋 Komut: {cmd3['command_name']} (0x{cmd3['command_type']:02X})")
        print(f"🎯 Hedef Mode: Safe (1)")
        print(f"🔐 Sequence: {cmd3['sequence']}")
        print(f"📦 Paket Boyutu: {cmd3['packet_size']} bytes")
        print(f"\n✅ FULL PACKET HEX:\n{cmd3['full_packet']}")

        # ÖZET
        print("\n\n" + "=" * 70)
        print("ÖZET - 3 Komut Gönderildi")
        print("=" * 70)
        print(f"1. {cmd1['command_name']:20} | {cmd1['packet_size']:3} bytes")
        print(f"2. {cmd2['command_name']:20} | {cmd2['packet_size']:3} bytes")
        print(f"3. {cmd3['command_name']:20} | {cmd3['packet_size']:3} bytes")
        print("\n💡 Bu paketler LoRa modülü üzerinden uydunun alıcısına gönderilir.")
        print("💡 Uydu şifreli paketleri çözer ve ilgili Task'lara yönlendirir.")
        print("=" * 70)

    except Exception as e:
        print(f"❌ HATA: {e}")
        return 1

    return 0


if __name__ == "__main__":
    exit(main())
