# SİMÜLASYON BAŞLATMA VE KULLANIM KILAVUZU
# Ulaş-S1 CubeSat SITL Modülü

## 🚀 Hızlı Başlangıç

### 1. Gereksinimler
```bash
# Python 3.8+ gerekli
python --version

# Bağımlılıkları yükle
pip install -r requirements.txt
```

### 2. İlk Test (1 dakika)
```bash
cd sim/
python run_simulation.py test
```

Beklenen çıktı:
```
✓ PASS | Simülatör Başlatma
✓ PASS | Sensör Veri Üretimi
✓ PASS | Güç Yönetimi
✓ PASS | Termal Kontrol
✓ PASS | Komut İşleme
✓ PASS | Sistem Modları
✓ PASS | Telemetri Toplama
```

---

## 📚 Modlar ve Komutlar

### Mode 1: Flight Software Integration Test
```bash
python run_simulation.py test
```

**Ne yapar:**
- Tüm yazılım bileşenlerini test eder
- Sensör veri üretimini doğrular
- Güç yönetimini simüle eder
- Komut işlemeyi kontrol eder
- Test sonuçlarını JSON'a kaydeder

**Çıktı dosya:** `flight_software_test_results.json`

---

### Mode 2: Orbital Analysis & Pass Prediction
```bash
python run_simulation.py orbit
```

**Ne yapar:**
- Yörünge parametrelerini gösterir
- Yer istasyonlarının pass'lerini tahmin eder
- Rise/culmination/set zamanlarını hesaplar
- Maximum görüş açısını gösterir

**Örnek çıktı:**
```
Orbital Period: 90.50 minutes
Apogee: 361.8 km
Perigee: 352.1 km

Ground Station: ANKARA
Location: 39.93°N, 32.86°E, 950m
====================================================================

Pass 1:
  Rise: 2026-03-28 13:15:23
  Set:  13:24:45
  Duration: 9.4 minutes
  Max Elevation: 47.3°
```

---

### Mode 3: Interactive Console
```bash
python run_simulation.py console
```

**Interaktif komutlar:**
```
cubesat> help
Commands:
  status    - Show system status
  sensors   - Print current sensor values
  mode      - Show current system mode
  cmd <id>  - Send command (01=Reset, 02=Safe, 03=Normal)
  telemetry - Show telemetry buffer
  save      - Save telemetry to file
  quit      - Exit

cubesat> status
Mode: NORMAL
Power State: NOMINAL
Battery SOC: 85.3%
Uptime: 45s
Telemetry Records: 4

cubesat> sensors
Battery: 3.95V (85.3%)
Temperature: 28.5°C
Magnetometer: (45.23, 23.15, 18.92) µT
GPS: 12.45°N, 42.30°E (Alt: 360000m)
Radiation: 58 CPM

cubesat> cmd 02
✓ Command 0x02 sent
```

---

### Mode 4: Batch Simulation
```bash
python run_simulation.py batch
```

**Ne yapar:**
- 10 saatlik simülasyon yapar
- Telemetri verisini toplar
- Batarya ve sıcaklık eğilimlerini analiz eder
- Sonuçları çıkarmaz

**Çıktı dosya:** `batch_simulation_data.json`

---

## 📊 Veri Çıktı Formatları

### flight_software_test_results.json
```json
{
  "timestamp": "2026-03-28T12:30:45.123456",
  "tests_passed": 7,
  "tests_total": 7,
  "details": [
    {
      "test": "Simülatör Başlatma",
      "passed": true,
      "error": ""
    },
    ...
  ]
}
```

### sitl_telemetry.json
```json
{
  "simulation_start": "2026-03-28T12:00:00",
  "duration_seconds": 30,
  "telemetry": [
    {
      "timestamp": 1711612800.123,
      "mag_x": 45.32,
      "mag_y": 23.15,
      "mag_z": 18.92,
      "pressure": 1.245e-05,
      "temperature": 25.3,
      "gps_lat": 45.00,
      "gps_lon": 123.45,
      "gps_alt": 360000,
      "gps_fix": 2,
      "radiation_cpm": 58,
      "battery_voltage": 3.95,
      "battery_soc": 85.3,
      "cpu_temp": 28.5,
      "uptime_seconds": 30
    },
    ...
  ],
  "system_status": {
    "timestamp": 1711612830.456,
    "mode": "NORMAL",
    "power_state": "NOMINAL",
    "uptime_seconds": 30,
    "battery_soc": 85.3,
    "core_temp": 28.5,
    "telemetry_count": 3
  }
}
```

---

## 🔬 Python API Kullanımı

### Temel SITL Örneği
```python
from sitl.cubesat_simulator import CubeSatSimulator
import time

# Simülatör oluştur
sim = CubeSatSimulator(simulation_speed=1.0)

# Başlat
sim.start_simulation()

# Sensör verisi oku
for i in range(30):
    time.sleep(1)
    sensors = sim.get_sensor_snapshot()
    status = sim.get_system_status()
    
    print(f"[{i:2d}s] Battery: {sensors['battery_soc']:.1f}% | "
          f"Temp: {sensors['cpu_temp']:.1f}°C")

# Komut gönder
sim.send_command(0x02)  # Safe mode

# Telemetriyi kaydet
sim.save_telemetry('my_simulation.json')

# Durdur
sim.stop_simulation()
```

### Yörünge Analizi Örneği
```python
from orbit_analysis.orbit_calculator import (
    OrbitCalculator, 
    OrbitalState,
    GroundStation
)
from datetime import datetime, timedelta

# Yörünge parametreleri
orbit = OrbitalState(
    epoch=datetime(2026, 3, 28, 12, 0, 0),
    semi_major_axis=6730.0,  # km
    eccentricity=0.0001,
    inclination=98.0,        # degrees
    raan=45.0,
    arg_perigee=0.0,
    mean_anomaly=0.0,
    mean_motion=14.13        # revolutions/day
)

# Hesapla
calc = OrbitCalculator(orbit)

# Yer istasyonu
ankara = GroundStation(
    name="ANKARA",
    latitude=39.93,
    longitude=32.86,
    altitude=950,
    antenna_elevation=10.0
)

# Geçişleri bulsa
start = datetime.now()
passes = calc.find_passes(ankara, start, duration_days=7.0)

# Results
for i, p in enumerate(passes[:5], 1):
    print(f"Pass {i}:")
    print(f"  Rise: {p['rise_time']}")
    print(f"  Set: {p['set_time']}")
    print(f"  Duration: {p['duration_seconds']/60:.1f} min")
    print(f"  Max Elevation: {p['max_elevation']:.1f}°")
```

---

## ⚙️ Konfigürasyon

Sistem parametrelerini `config.json` dosyasında değiştir:

```json
{
  "satellite": {
    "name": "Ulas-S1",
    "launch_date": "2026-06-15"
  },
  "orbit": {
    "altitude_km": 360,
    "inclination_degrees": 98.0
  },
  "power_system": {
    "battery": {
      "capacity_mah": 20000,
      "nominal_voltage_v": 3.9
    }
  },
  ...
}
```

### Yaygın Ayarlar

**Daha hızlı testler:**
```python
sim = CubeSatSimulator(simulation_speed=10.0)  # 10x hızlı
```

**Sentetik veri oluşturma:**
```python
# config.json'da:
"simulation": {
  "simulation_speed_multiplier": 100.0,
  "step_size_ms": 100,
  "orbital_propagation": true,
  "thermal_modeling": true,
  "power_modeling": true,
  "sensor_noise": true
}
```

---

## 🔍 Veri Analizi

### Telemetri Analizi
```python
import json

# Veriyi yükle
with open('sitl_telemetry.json') as f:
    data = json.load(f)

telemetry = data['telemetry']

# Battery trends
battery_socs = [t['battery_soc'] for t in telemetry]
battery_times = [t['timestamp'] for t in telemetry]

print(f"Initial SOC: {battery_socs[0]:.1f}%")
print(f"Final SOC: {battery_socs[-1]:.1f}%")
print(f"Average discharge: {(battery_socs[0]-battery_socs[-1])/len(telemetry):.3f}%/record")

# Temperature analysis
temps = [t['cpu_temp'] for t in telemetry]
print(f"Min Temp: {min(temps):.1f}°C")
print(f"Max Temp: {max(temps):.1f}°C")
print(f"Avg Temp: {sum(temps)/len(temps):.1f}°C")
```

### Matplotlib İle Görselleştirme
```python
import json
import matplotlib.pyplot as plt

# Veriyi yükle
with open('sitl_telemetry.json') as f:
    data = json.load(f)

telemetry = data['telemetry']

# Plots
fig, axes = plt.subplots(3, 1, figsize=(10, 8))

times = [t['timestamp'] for t in telemetry]
times = [t - times[0] for t in times]  # Normalize to start at 0

# Battery SOC
socs = [t['battery_soc'] for t in telemetry]
axes[0].plot(times, socs, 'b-', linewidth=2)
axes[0].set_ylabel('Battery SOC (%)')
axes[0].grid(True)
axes[0].set_title('Battery State of Charge')

# Temperature
temps = [t['cpu_temp'] for t in telemetry]
axes[1].plot(times, temps, 'r-', linewidth=2)
axes[1].set_ylabel('Temperature (°C)')
axes[1].grid(True)
axes[1].set_title('CPU Temperature')

# Radiation
rads = [t['radiation_cpm'] for t in telemetry]
axes[2].plot(times, rads, 'g-', linewidth=2)
axes[2].set_ylabel('Radiation (CPM)')
axes[2].set_xlabel('Time (seconds)')
axes[2].grid(True)
axes[2].set_title('Radiation Levels')

plt.tight_layout()
plt.savefig('sitl_analysis.png', dpi=150)
plt.show()
```

---

## 🐛 Sorun Giderme

### "ModuleNotFoundError: No module named 'sitl'"
```bash
# requirements.txt'i yükle
pip install -r sim/requirements.txt

# Python'a sim klasörünü ekle
export PYTHONPATH="${PYTHONPATH}:/path/to/sim"
```

### Simülatör başlamıyor
```python
# Debug mode'u aç
import logging
logging.basicConfig(level=logging.DEBUG)

from sitl.cubesat_simulator import CubeSatSimulator
sim = CubeSatSimulator()
sim.start_simulation()  # Verbose output
```

### Kötü telemetri verileri
- `config.json`'daki sensör parametrelerini kontrol et
- `simulation_speed` çok yüksekse azalt
- `thermal_modeling` veya `power_modeling`'i etkinleştir

---

## 📈 Performans Optimizasyonu

### Büyük veri setleri
```python
# Telemetri frekansını azalt
sim.telemetry_interval = 60.0  # Her 60 saniyede bir

# Or disable certain sensors in config.json
"simulation": {
  "temperature_noise": false,
  "radiation_noise": false
}
```

### Hızlı simülasyon
```python
# Simülatör hızını artır
sim = CubeSatSimulator(simulation_speed=100.0)

# Note: Çok yüksek hızlarda fizik modelemesi hatalı olabilir
```

---

## 🔗 İlgili Bağlantılar

- **Flight Software**: `../fsw/README.md`
- **Ground Station**: `../gsw/README.md`
- **Shared Headers**: `../shared/`

---

## 📞 Destek

Sorunlar veya sorular için:
1. Log dosyasını kontrol et
2. `config.json` parametrelerini doğrula
3. Python versiyonunu kontrol et (3.8+)
4. Requirements'i yeniden yükle

---

**Versiyon**: 1.0.0  
**Son Güncelleme**: 2026-03-28  
**Durum**: Production-ready
