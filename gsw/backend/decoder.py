#!/usr/bin/env python3
"""
Ulaş-S1 CubeSat Ground Station Backend
CSP Packet Decoder with AES-256 CTR Decryption and InfluxDB Storage

Features:
- CSP protocol header parsing
- AES-256 CTR mode decryption with dynamic IV
- Beacon and Payload telemetry decoding
- InfluxDB v2 time-series storage
- Command uplink generation
- UDP listener for LoRa radio packets
"""

import socket
import struct
import time
import argparse
import json
from datetime import datetime
from typing import Dict, Any, Optional, Tuple
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
from cryptography.hazmat.backends import default_backend
import influxdb_client
from influxdb_client.client.write_api import SYNCHRONOUS


class CryptoHandler:
    """AES-256 CTR mode encryption/decryption handler"""

    def __init__(self, key_hex: Optional[str] = None):
        self.key = None
        if key_hex:
            self.set_key(key_hex)

    def set_key(self, key_hex: str):
        """Set AES-256 key from hex string (64 characters)"""
        if len(key_hex) != 64:
            raise ValueError("AES-256 key must be 64 hex characters (32 bytes)")
        self.key = bytes.fromhex(key_hex)

    def construct_iv(self, timestamp: int, sequence: int) -> bytes:
        """Construct dynamic IV: timestamp(4B) + sequence(4B) + padding(8B)"""
        iv = struct.pack('<II', timestamp, sequence) + b'\x00' * 8
        return iv

    def decrypt(self, ciphertext: bytes, timestamp: int, sequence: int) -> bytes:
        """Decrypt AES-256 CTR mode data"""
        if not self.key:
            raise ValueError("AES key not set")

        iv = self.construct_iv(timestamp, sequence)
        cipher = Cipher(algorithms.AES(self.key), modes.CTR(iv), backend=default_backend())
        decryptor = cipher.decryptor()
        return decryptor.update(ciphertext) + decryptor.finalize()

    def encrypt(self, plaintext: bytes, timestamp: int, sequence: int) -> bytes:
        """Encrypt AES-256 CTR mode data"""
        if not self.key:
            raise ValueError("AES key not set")

        iv = self.construct_iv(timestamp, sequence)
        cipher = Cipher(algorithms.AES(self.key), modes.CTR(iv), backend=default_backend())
        encryptor = cipher.encryptor()
        return encryptor.update(plaintext) + encryptor.finalize()


class CSPPacketParser:
    """CubecSat Space Protocol packet parser"""

    # CSP header bit fields
    CSP_HEADER_MASK_PRIORITY = 0x03
    CSP_HEADER_MASK_SOURCE = 0x1F << 2
    CSP_HEADER_MASK_DESTINATION = 0x1F << 7
    CSP_HEADER_MASK_DEST_PORT = 0x3F << 12
    CSP_HEADER_MASK_SOURCE_PORT = 0x3F << 18
    CSP_HEADER_MASK_HMAC = 0x01 << 24
    CSP_HEADER_MASK_XTEA = 0x01 << 25
    CSP_HEADER_MASK_RDP = 0x01 << 26
    CSP_HEADER_MASK_CRC = 0x01 << 27

    def __init__(self):
        self.crypto = CryptoHandler()

    def parse_header(self, header_bytes: bytes) -> Dict[str, Any]:
        """Parse 32-bit CSP header"""
        if len(header_bytes) != 4:
            raise ValueError("CSP header must be 4 bytes")

        header = struct.unpack('<I', header_bytes)[0]

        return {
            'priority': header & self.CSP_HEADER_MASK_PRIORITY,
            'source': (header & self.CSP_HEADER_MASK_SOURCE) >> 2,
            'destination': (header & self.CSP_HEADER_MASK_DESTINATION) >> 7,
            'dest_port': (header & self.CSP_HEADER_MASK_DEST_PORT) >> 12,
            'source_port': (header & self.CSP_HEADER_MASK_SOURCE_PORT) >> 18,
            'flags': {
                'hmac': bool(header & self.CSP_HEADER_MASK_HMAC),
                'xtea': bool(header & self.CSP_HEADER_MASK_XTEA),
                'rdp': bool(header & self.CSP_HEADER_MASK_RDP),
                'crc': bool(header & self.CSP_HEADER_MASK_CRC)
            }
        }

    def parse_packet(self, raw_packet: bytes, aes_key: Optional[str] = None) -> Dict[str, Any]:
        """Parse complete CSP packet with optional decryption"""
        if len(raw_packet) < 4:
            raise ValueError("Packet too short for CSP header")

        header = self.parse_header(raw_packet[:4])
        payload = raw_packet[4:]

        # Decrypt payload if AES key provided and encryption flag set
        if aes_key and header['flags']['xtea']:
            # Extract timestamp and sequence from payload (first 8 bytes)
            if len(payload) < 8:
                raise ValueError("Encrypted payload too short")
            timestamp, sequence = struct.unpack('<II', payload[:8])
            encrypted_data = payload[8:]

            self.crypto.set_key(aes_key)
            payload = self.crypto.decrypt(encrypted_data, timestamp, sequence)

        return {
            'header': header,
            'payload': payload,
            'raw_length': len(raw_packet)
        }


class BeaconDecoder:
    """Beacon telemetry packet decoder (37 bytes)"""

    FORMAT = '<IBffffBBBBBH'  # Little-endian struct format
    SIZE = 37

    def decode(self, payload: bytes) -> Dict[str, Any]:
        """Decode beacon packet payload"""
        if len(payload) != self.SIZE:
            raise ValueError(f"Beacon packet must be {self.SIZE} bytes, got {len(payload)}")

        unpacked = struct.unpack(self.FORMAT, payload)

        return {
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
            'task_health': {
                'adcs': bool(unpacked[8] & 0x01),
                'payload': bool(unpacked[8] & 0x02),
                'comm': bool(unpacked[8] & 0x04),
                'eps': bool(unpacked[8] & 0x08)
            },
            'error_flags': unpacked[9],
            'uptime_seconds': unpacked[10]
        }


class PayloadDecoder:
    """Science payload telemetry packet decoder (60+ bytes)"""

    FORMAT = '<IBffffBBBBBHffffBBBBH'  # Extended format
    MIN_SIZE = 60

    def decode(self, payload: bytes) -> Dict[str, Any]:
        """Decode payload packet payload"""
        if len(payload) < self.MIN_SIZE:
            raise ValueError(f"Payload packet must be at least {self.MIN_SIZE} bytes")

        # Unpack fixed-size header
        header_size = struct.calcsize(self.FORMAT)
        if len(payload) < header_size:
            raise ValueError("Payload too short for header")

        unpacked = struct.unpack(self.FORMAT, payload[:header_size])

        telemetry = {
            'timestamp': unpacked[0],
            'sequence_number': unpacked[1],
            'radiation': {
                'cpm': unpacked[2],
                'usv_per_hour': round(unpacked[3], 4)
            },
            'temperature': round(unpacked[4], 2),
            'pressure': round(unpacked[5], 2),
            'altitude': round(unpacked[6], 2),  # Barometric altitude
            'gps': {
                'latitude': round(unpacked[7] / 1e7, 7),   # Convert from 1e-7 degrees
                'longitude': round(unpacked[8] / 1e7, 7),
                'altitude': round(unpacked[9], 2),
                'satellites': unpacked[10]
            },
            'ahrs': {
                'roll': round(unpacked[11], 2),
                'pitch': round(unpacked[12], 2),
                'yaw': round(unpacked[13], 2)
            },
            'eps_status': {
                'battery_soc': unpacked[14],
                'power_mode': unpacked[15],
                'charging': bool(unpacked[16])
            },
            'error_flags': unpacked[17],
            'sd_card_status': unpacked[18],
            'free_space_kb': unpacked[19]
        }

        return telemetry


class TelemetryDatabase:
    """InfluxDB v2 time-series database interface"""

    def __init__(self, url: str, token: str, org: str, bucket: str):
        self.client = influxdb_client.InfluxDBClient(
            url=url,
            token=token,
            org=org
        )
        self.bucket = bucket
        self.write_api = self.client.write_api(write_options=SYNCHRONOUS)

    def store_beacon(self, data: Dict[str, Any]):
        """Store beacon telemetry in InfluxDB"""
        timestamp = data['timestamp'] * 1000000000  # Convert to nanoseconds

        # Create InfluxDB point
        point = influxdb_client.Point("beacon") \
            .time(timestamp) \
            .field("battery_voltage", data['battery_voltage']) \
            .field("mag_x", data['magnetometer']['x']) \
            .field("mag_y", data['magnetometer']['y']) \
            .field("mag_z", data['magnetometer']['z']) \
            .field("mcu_temperature", data['mcu_temperature']) \
            .field("free_ram", data['free_ram']) \
            .field("uptime_seconds", data['uptime_seconds']) \
            .field("safe_mode", 1 if data['safe_mode'] else 0) \
            .tag("satellite", "ulas-s1")

        # Add task health as fields
        for task, healthy in data['task_health'].items():
            point = point.field(f"task_{task}_healthy", 1 if healthy else 0)

        self.write_api.write(bucket=self.bucket, org=self.client.org, record=point)

    def store_payload(self, data: Dict[str, Any]):
        """Store payload telemetry in InfluxDB"""
        timestamp = data['timestamp'] * 1000000000

        point = influxdb_client.Point("payload") \
            .time(timestamp) \
            .field("radiation_cpm", data['radiation']['cpm']) \
            .field("radiation_usv_h", data['radiation']['usv_per_hour']) \
            .field("temperature", data['temperature']) \
            .field("pressure", data['pressure']) \
            .field("altitude_baro", data['altitude']) \
            .field("gps_latitude", data['gps']['latitude']) \
            .field("gps_longitude", data['gps']['longitude']) \
            .field("gps_altitude", data['gps']['altitude']) \
            .field("gps_satellites", data['gps']['satellites']) \
            .field("ahrs_roll", data['ahrs']['roll']) \
            .field("ahrs_pitch", data['ahrs']['pitch']) \
            .field("ahrs_yaw", data['ahrs']['yaw']) \
            .field("eps_battery_soc", data['eps_status']['battery_soc']) \
            .field("eps_power_mode", data['eps_status']['power_mode']) \
            .field("eps_charging", 1 if data['eps_status']['charging'] else 0) \
            .field("sd_free_space_kb", data['free_space_kb']) \
            .tag("satellite", "ulas-s1") \
            .tag("sequence", str(data['sequence_number']))

        self.write_api.write(bucket=self.bucket, org=self.client.org, record=point)

    def query_recent(self, measurement: str, hours: int = 24) -> list:
        """Query recent telemetry data"""
        query = f'''
        from(bucket: "{self.bucket}")
        |> range(start: -{hours}h)
        |> filter(fn: (r) => r._measurement == "{measurement}")
        |> filter(fn: (r) => r.satellite == "ulas-s1")
        |> sort(columns: ["_time"], desc: true)
        |> limit(n: 100)
        '''

        result = self.client.query_api().query(query)
        return result


class CommandGenerator:
    """Uplink command generation"""

    CMD_MODE_CHANGE = 0x01
    CMD_RESET_SYSTEM = 0x02
    CMD_CONFIG_ADCS = 0x03
    CMD_CONFIG_PAYLOAD = 0x04
    CMD_DATA_REQUEST = 0x05

    def __init__(self, crypto: CryptoHandler):
        self.crypto = crypto

    def generate_mode_change(self, new_mode: int, timestamp: int, sequence: int) -> bytes:
        """Generate MODE_CHANGE command"""
        payload = struct.pack('<BBI', self.CMD_MODE_CHANGE, new_mode, timestamp)
        if self.crypto.key:
            payload = self.crypto.encrypt(payload, timestamp, sequence)
        return payload

    def generate_reset(self, timestamp: int, sequence: int) -> bytes:
        """Generate RESET_SYSTEM command"""
        payload = struct.pack('<BI', self.CMD_RESET_SYSTEM, timestamp)
        if self.crypto.key:
            payload = self.crypto.encrypt(payload, timestamp, sequence)
        return payload

    def generate_config_adcs(self, kp: float, ki: float, kd: float, timestamp: int, sequence: int) -> bytes:
        """Generate ADCS configuration command"""
        payload = struct.pack('<BfffI', self.CMD_CONFIG_ADCS, kp, ki, kd, timestamp)
        if self.crypto.key:
            payload = self.crypto.encrypt(payload, timestamp, sequence)
        return payload


class GroundStationReceiver:
    """UDP listener for CSP packets from LoRa radio"""

    def __init__(self, host: str = "0.0.0.0", port: int = 5000, aes_key: Optional[str] = None,
                 influx_url: Optional[str] = None, influx_token: Optional[str] = None,
                 influx_org: Optional[str] = None, influx_bucket: Optional[str] = None):

        self.host = host
        self.port = port
        self.sock = None

        # Initialize components
        self.parser = CSPPacketParser()
        self.beacon_decoder = BeaconDecoder()
        self.payload_decoder = PayloadDecoder()

        if aes_key:
            self.parser.crypto.set_key(aes_key)

        self.database = None
        if all([influx_url, influx_token, influx_org, influx_bucket]):
            self.database = TelemetryDatabase(influx_url, influx_token, influx_org, influx_bucket)

        self.command_gen = CommandGenerator(self.parser.crypto)

        # Statistics
        self.packets_received = 0
        self.beacons_decoded = 0
        self.payloads_decoded = 0
        self.errors = 0

    def start(self):
        """Start UDP listener"""
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.bind((self.host, self.port))
        self.sock.settimeout(1.0)  # 1 second timeout for clean shutdown

        print(f"[GS] Ground station listening on {self.host}:{self.port}")
        if self.database:
            print("[GS] InfluxDB storage enabled")
        else:
            print("[GS] InfluxDB storage disabled")

        try:
            while True:
                try:
                    data, addr = self.sock.recvfrom(1024)
                    self.packets_received += 1
                    self._process_packet(data, addr)

                except socket.timeout:
                    continue  # No data received, continue loop

        except KeyboardInterrupt:
            print("\n[GS] Shutting down...")
        finally:
            if self.sock:
                self.sock.close()
            self._print_stats()

    def _process_packet(self, data: bytes, addr: Tuple[str, int]):
        """Process received CSP packet"""
        try:
            # Parse CSP packet
            packet = self.parser.parse_packet(data)

            # Route based on destination port
            dest_port = packet['header']['dest_port']

            if dest_port == 10:  # Beacon port
                beacon_data = self.beacon_decoder.decode(packet['payload'])
                self.beacons_decoded += 1

                print(f"[BEACON] From {addr[0]}: V={beacon_data['battery_voltage']}V, "
                      f"Mag=({beacon_data['magnetometer']['x']:.2f}, "
                      f"{beacon_data['magnetometer']['y']:.2f}, "
                      f"{beacon_data['magnetometer']['z']:.2f}) µT")

                if self.database:
                    self.database.store_beacon(beacon_data)

            elif dest_port == 11:  # Payload port
                payload_data = self.payload_decoder.decode(packet['payload'])
                self.payloads_decoded += 1

                print(f"[PAYLOAD] From {addr[0]}: Rad={payload_data['radiation']['cpm']} CPM, "
                      f"GPS=({payload_data['gps']['latitude']:.4f}, "
                      f"{payload_data['gps']['longitude']:.4f}), "
                      f"Alt={payload_data['altitude']:.1f}m")

                if self.database:
                    self.database.store_payload(payload_data)

            else:
                print(f"[UNKNOWN] Port {dest_port} from {addr[0]}")

        except Exception as e:
            self.errors += 1
            print(f"[ERROR] Failed to process packet from {addr[0]}: {e}")

    def _print_stats(self):
        """Print reception statistics"""
        print("\n[GS] Session Statistics:")
        print(f"  Packets received: {self.packets_received}")
        print(f"  Beacons decoded: {self.beacons_decoded}")
        print(f"  Payloads decoded: {self.payloads_decoded}")
        print(f"  Errors: {self.errors}")


def main():
    """Main entry point"""
    parser = argparse.ArgumentParser(description="Ulaş-S1 Ground Station Backend")
    parser.add_argument("--port", type=int, default=5000, help="UDP listening port")
    parser.add_argument("--host", default="0.0.0.0", help="Listening IP address")
    parser.add_argument("--aes-key", help="AES-256 key (64 hex characters)")
    parser.add_argument("--influx-url", help="InfluxDB URL")
    parser.add_argument("--influx-token", help="InfluxDB token")
    parser.add_argument("--influx-org", help="InfluxDB organization")
    parser.add_argument("--influx-bucket", help="InfluxDB bucket")

    args = parser.parse_args()

    # Create and start ground station
    gs = GroundStationReceiver(
        host=args.host,
        port=args.port,
        aes_key=args.aes_key,
        influx_url=args.influx_url,
        influx_token=args.influx_token,
        influx_org=args.influx_org,
        influx_bucket=args.influx_bucket
    )

    gs.start()


if __name__ == "__main__":
    main()