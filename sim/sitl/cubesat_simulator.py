# sim/sitl/cubesat_simulator.py
# CubeSat Software-in-the-Loop (SITL) Simülatörü
# Yazılım testleri için donanım olmadan uydu simülasyonu

import time
import json
import math
from datetime import datetime, timedelta
from dataclasses import dataclass, asdict
from typing import Dict, List, Optional
from enum import Enum
import threading
from queue import Queue

class SystemMode(Enum):
    """Sistem çalışma modları"""
    SAFESAFE = "SAFE"
    NORMAL = "NORMAL"
    RECOVERY = "RECOVERY"
    POWER_DOWN = "POWER_DOWN"

class PowerState(Enum):
    """Güç durumu"""
    CRITICAL = 0      # < 3.0V
    LOW = 1            # 3.0-3.5V
    NOMINAL = 2        # 3.5-4.1V
    CHARGING = 3       # > 4.1V

@dataclass
class SensorData:
    """Sensör ölçümleri"""
    timestamp: float
    
    # Magnetometre (RM3100)
    mag_x: float = 0.0  # µT
    mag_y: float = 0.0
    mag_z: float = 0.0
    
    # Barometrik sensör (BMP388)
    pressure: float = 101.325  # kPa
    temperature: float = 20.0  # °C
    altitude: float = 0.0  # meters
    
    # GPS (NEO-M9N)
    gps_lat: float = 0.0  # degrees
    gps_lon: float = 0.0
    gps_alt: float = 0.0  # meters
    gps_fix: int = 0      # 0=no fix, 1=2D, 2=3D
    
    # Radiation sensörü (GQ-511A)
    radiation_cpm: int = 50  # Counts per minute
    
    # Güç (BQ24295)
    battery_voltage: float = 3.9  # V
    charge_current: float = 0.5   # A
    battery_soc: float = 80.0     # %
    
    # Sistem durum
    cpu_temp: float = 30.0  # °C
    uptime_seconds: int = 0

@dataclass
class CommandData:
    """Komut veri yapısı"""
    timestamp: float
    command_id: int
    payload: bytes = b''

class CubeSatSimulator:
    """CubeSat SITL Simülatörü"""
    
    def __init__(self, simulation_speed: float = 1.0):
        """
        Initialize simulator
        
        Args:
            simulation_speed: Simulation speed multiplier (1.0 = real-time)
        """
        self.simulation_speed = simulation_speed
        self.system_mode = SystemMode.NORMAL
        self.power_state = PowerState.NOMINAL
        self.running = False
        
        # Sensor state
        self.sensor_data = SensorData(timestamp=time.time())
        self.command_queue: Queue[CommandData] = Queue()
        self.telemetry_buffer: List[SensorData] = []
        self.startup_time = time.time()
        self.simulation_start = datetime(2026, 3, 28, 12, 0, 0)
        
        # Orbital mechanics (simplified)
        self.orbital_altitude = 360.0  # km
        self.orbital_inclination = 98.0  # degrees
        self.current_latitude = 0.0
        self.current_longitude = 0.0
        
        # Battery simulation
        self.battery_capacity = 20000.0  # mAh
        self.charge_rate = 100.0  # mA
        self.discharge_rate = 200.0  # mA
        self.battery_current_charge = self.battery_capacity * 0.8  # Start at 80%
        
        # Thermal model
        self.core_temp = 25.0
        self.dissipation_rate = 0.5  # W
        self.ambient_temp = -40.0  # Space temperature
        
        # Thread for simulation loop
        self.sim_thread = None
    
    def start_simulation(self):
        """Simülasyonu başlat"""
        if self.running:
            return
        
        self.running = True
        self.sim_thread = threading.Thread(target=self._simulation_loop, daemon=True)
        self.sim_thread.start()
        print("[SITL] Simülasyon başladı")
    
    def stop_simulation(self):
        """Simülasyonu durdur"""
        self.running = False
        if self.sim_thread:
            self.sim_thread.join(timeout=5.0)
        print("[SITL] Simülasyon durduruldu")
    
    def _simulation_loop(self):
        """Ana simülasyon döngüsü"""
        last_tick = time.time()
        tick_rate = 0.1  # 100 ms
        
        while self.running:
            current_time = time.time()
            dt = current_time - last_tick
            
            if dt >= tick_rate:
                self._update_sensors(dt)
                self._update_power()
                self._update_thermal(dt)
                self._process_commands()
                self._update_system_state()
                
                last_tick = current_time
            
            time.sleep(0.01)  # CPU yield
    
    def _update_sensors(self, dt: float):
        """Sensör verilerini güncelle"""
        # Magnetometre - Dünya manyetik alanı
        earth_mag = 50.0  # µT
        angle = (time.time() - self.startup_time) * 0.1
        self.sensor_data.mag_x = earth_mag * math.cos(angle)
        self.sensor_data.mag_y = earth_mag * math.sin(angle)
        self.sensor_data.mag_z = earth_mag * 0.5
        
        # GPS - Orbital konumu simüle et
        orbital_period = 90 * 60  # 90 minutes in seconds
        time_in_orbit = (time.time() - self.startup_time) % orbital_period
        
        # Şekil 8 yörüngesi
        self.sensor_data.gps_lat = 45.0 * math.sin(2 * math.pi * time_in_orbit / orbital_period)
        self.sensor_data.gps_lon = (360.0 * time_in_orbit / orbital_period) % 360.0
        self.sensor_data.gps_alt = self.orbital_altitude * 1000
        self.sensor_data.gps_fix = 2  # 3D fix
        
        # Barometrik sensör - sabit yükseklikte
        self.sensor_data.pressure = 101.325 * math.exp(-self.orbital_altitude / 8.5)
        self.sensor_data.temperature = 20.0 + 5.0 * math.sin(angle)
        self.sensor_data.altitude = self.orbital_altitude * 1000
        
        # Radiation - Yörünge konumuna bağlı
        base_rad = 50  # CPM space background
        self.sensor_data.radiation_cpm = base_rad + int(10 * math.sin(angle))
        
        self.sensor_data.timestamp = time.time()
    
    def _update_power(self):
        """Güç sistemini güncelle"""
        # Güneşteki veya gölgedeki mi?
        orbital_period = 90 * 60
        time_in_orbit = (time.time() - self.startup_time) % orbital_period
        eclipse_fraction = 0.35  # ~35% eclipse for sun-sync orbit
        
        is_in_eclipse = (time_in_orbit % orbital_period) < (eclipse_fraction * orbital_period)
        
        if is_in_eclipse:
            # Güneş panelleri kapalı, batarya deşarj
            power_change = -self.discharge_rate
        else:
            # Güneş panelleri açık, batarya şarj
            power_change = self.charge_rate
        
        # Batarya şarjını güncelle
        self.battery_current_charge += power_change / 3600.0  # Convert mA to mAh/s
        self.battery_current_charge = max(0, min(self.battery_capacity, self.battery_current_charge))
        
        # Batarya voltajı hesapla (simplified)
        soc = self.battery_current_charge / self.battery_capacity
        self.sensor_data.battery_soc = soc * 100.0
        
        # Batarya voltajı: 3.0V - 4.2V
        self.sensor_data.battery_voltage = 3.0 + (soc * 1.2)
        self.sensor_data.charge_current = self.charge_rate if not is_in_eclipse else -self.discharge_rate
        
        # Durum güncelle
        if self.sensor_data.battery_voltage < 3.0:
            self.power_state = PowerState.CRITICAL
            self.system_mode = SystemMode.SAFE
        elif self.sensor_data.battery_voltage < 3.5:
            self.power_state = PowerState.LOW
        elif self.sensor_data.battery_voltage > 4.1:
            self.power_state = PowerState.CHARGING
            self.system_mode = SystemMode.NORMAL
        else:
            self.power_state = PowerState.NOMINAL
    
    def _update_thermal(self, dt: float):
        """Termal modeli güncelle"""
        # CPU ısı akışı (simplified Stefan-Boltzmann)
        ambient = self.ambient_temp  # -40°C to -60°C in space
        temp_diff = self.core_temp - ambient
        
        # Isı kaybı
        if temp_diff > 0:
            cooling_rate = 0.1 * (temp_diff / 100.0)  # Radiative cooling
        else:
            cooling_rate = 0
        
        # Güç tüketimi tabanlı ısı üretimi
        load_factor = 0.5 if self.system_mode == SystemMode.NORMAL else 0.2
        heat_generation = self.dissipation_rate * load_factor
        
        # Sıcaklık güncelle
        self.core_temp = ambient + (self.core_temp - ambient) * math.exp(-cooling_rate * dt)
        self.core_temp += heat_generation * dt * 10
        
        self.sensor_data.cpu_temp = self.core_temp
    
    def _process_commands(self):
        """Komutları işle"""
        try:
            cmd = self.command_queue.get_nowait()
            print(f"[SITL] Komut alındı: ID={cmd.command_id}")
            
            if cmd.command_id == 0x01:  # Reset
                self._execute_reset()
            elif cmd.command_id == 0x02:  # Enter safe mode
                self.system_mode = SystemMode.SAFE
            elif cmd.command_id == 0x03:  # Enter normal mode
                self.system_mode = SystemMode.NORMAL
            elif cmd.command_id == 0x04:  # Antenna deploy
                print("[SITL] Anten açılması komutu")
            
        except:
            pass  # No command in queue
    
    def _update_system_state(self):
        """Sistem durumunu güncelle"""
        self.sensor_data.uptime_seconds = int(time.time() - self.startup_time)
        
        # Telemetri arabelleğine ekle (her 10 saniyede bir)
        if len(self.telemetry_buffer) == 0 or \
           (time.time() - self.telemetry_buffer[-1].timestamp) > 10.0:
            self.telemetry_buffer.append(SensorData(**asdict(self.sensor_data)))
            
            # Keep only last 1000 records
            if len(self.telemetry_buffer) > 1000:
                self.telemetry_buffer.pop(0)
    
    def _execute_reset(self):
        """Sistem sıfırla"""
        print("[SITL] Sistem sıfırlanıyor...")
        self.startup_time = time.time()
        self.system_mode = SystemMode.NORMAL
        self.sensor_data.uptime_seconds = 0
    
    def send_command(self, command_id: int, payload: bytes = b''):
        """Komutu kuyruğa ekle"""
        cmd = CommandData(
            timestamp=time.time(),
            command_id=command_id,
            payload=payload
        )
        self.command_queue.put(cmd)
    
    def get_sensor_snapshot(self) -> Dict:
        """Mevcut sensör verisini al"""
        return asdict(self.sensor_data)
    
    def get_telemetry_buffer(self) -> List[Dict]:
        """Telemetri arabelleğini al"""
        return [asdict(d) for d in self.telemetry_buffer]
    
    def get_system_status(self) -> Dict:
        """Sistem durumunu al"""
        return {
            'timestamp': time.time(),
            'mode': self.system_mode.value,
            'power_state': self.power_state.name,
            'uptime_seconds': self.sensor_data.uptime_seconds,
            'battery_soc': self.sensor_data.battery_soc,
            'core_temp': self.sensor_data.cpu_temp,
            'telemetry_count': len(self.telemetry_buffer)
        }
    
    def save_telemetry(self, filename: str):
        """Telemetriyi JSON dosyasına kaydet"""
        data = {
            'simulation_start': self.simulation_start.isoformat(),
            'duration_seconds': int(time.time() - self.startup_time),
            'telemetry': self.get_telemetry_buffer(),
            'system_status': self.get_system_status()
        }
        
        with open(filename, 'w') as f:
            json.dump(data, f, indent=2)
        print(f"[SITL] Telemetri kaydedildi: {filename}")

# Test örneği
if __name__ == "__main__":
    sim = CubeSatSimulator(simulation_speed=1.0)
    sim.start_simulation()
    
    try:
        # Simülasyonu 30 saniye çalıştır
        for i in range(30):
            time.sleep(1)
            status = sim.get_system_status()
            sensors = sim.get_sensor_snapshot()
            
            print(f"[{i:2d}s] Mode: {status['mode']:6s} | "
                  f"Power: {status['power_state']:10s} | "
                  f"SOC: {sensors['battery_soc']:5.1f}% | "
                  f"Temp: {sensors['cpu_temp']:5.1f}°C | "
                  f"Telemetry: {status['telemetry_count']} records")
        
        # Telemetriyi kaydet
        sim.save_telemetry('sitl_telemetry.json')
        
    finally:
        sim.stop_simulation()
