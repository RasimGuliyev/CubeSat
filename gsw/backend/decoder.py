# gsw/backend/decoder.py
import struct

def decode_beacon_packet(raw_bytes):
    """
    C tarafındaki BeaconPacket_t yapısını çözer.
    Format string açıklaması ('< I B f f f f B H'):
    < : Little-endian (STM32 ARM Cortex-M mimarisi little-endian kullanır)
    I : uint32_t (4 byte) timestamp
    B : uint8_t  (1 byte) safe_mode
    f : float    (4 byte) battery_voltage
    f : float    (4 byte) mag_x
    f : float    (4 byte) mag_y
    f : float    (4 byte) mag_z
    B : uint8_t  (1 byte) mcu_temp
    H : uint16_t (2 byte) free_ram
    Toplam = 4+1+4+4+4+4+1+2 = 24 Byte paket.
    """
    # Gelen bayt dizisinin uzunluğunu kontrol et
    if len(raw_bytes) != 24:
        raise ValueError("Hatalı paket boyutu!")

    # C'deki struct ile aynı düzende veriyi parçala
    unpacked_data = struct.unpack('<IBffffBH', raw_bytes)
    
    # Okunabilir bir sözlük (dictionary) formatına çevir
    telemetry = {
        "timestamp": unpacked_data[0],
        "safe_mode": bool(unpacked_data[1]),
        "battery_voltage": round(unpacked_data[2], 2),
        "mag_x": round(unpacked_data[3], 4),
        "mag_y": round(unpacked_data[4], 4),
        "mag_z": round(unpacked_data[5], 4),
        "mcu_temp": unpacked_data[6],
        "free_ram": unpacked_data[7]
    }
    
    return telemetry

# --- ÖRNEK KULLANIM ---
# Uzaydan gelen 24 baytlık örnek (dummy) veri:
sample_rx_bytes = b'\xa0\x0f\x00\x00\x00\x9a\x99\xec@\x00\x00\x80?\x00\x00\x00@\x00\x00@@\x1e\x00\x10'

parsed_data = decode_beacon_packet(sample_rx_bytes)
print(parsed_data)
# Çıktı: {'timestamp': 4000, 'safe_mode': False, 'battery_voltage': 7.4, 'mag_x': 1.0, ... }