# CubeSat - Simülasyon Modülü

 CubeSat'inin yazılım ve sistem performansını donanım olmadan test etmek için kullanılan simülasyon araçları.

## 📁 Yapı

```
sim/
├── orbit_analysis/          # Yörünge analiz ve pass prediction
│   └── orbit_calculator.py  # Yörünge propagasyonu ve geçiş tahmini
├── sitl/                     # Software-in-the-Loop simülatörü
│   ├── cubesat_simulator.py  # Ana SITL simülatörü
│   └── flight_software_test.py  # Yazılım test suite
└── config.json              # Sistem konfigürasyonu
```

## 🛰️ Yörünge Analizi (orbit_analysis/)

### `orbit_calculator.py`

Uydu yörüngesini hesaplar ve yer istasyonları ile haberleşme olanaklarını tahmin eder.

**Özellikler:**
- TLE (Two-Line Element) tabanlı yörünge propagasyonu
- Yer istasyonu pass prediction (geçiş tahmini)
- Kapsama alanı hesaplaması
- Görüş açısı ve mesafe hesaplama
- ECEF koordinat dönüşümleri

**Kullanım Örneği:**
```python
from orbit_calculator import OrbitCalculator, OrbitalState, GroundStation

# Yörünge tanımla
orbit = OrbitalState(
    epoch=datetime.now(),
    semi_major_axis=6730.0,  # km
    eccentricity=0.0001,
    inclination=98.0,        # degrees
    raan=45.0,
    arg_perigee=0.0,
    mean_anomaly=0.0,
    mean_motion=14.13        # rev/day
)

# Hesaplayıcı oluştur
calc = OrbitCalculator(orbit)

# Yer istasyonu tanımla
ankara = GroundStation("ANKARA", 39.93, 32.86, 950)

# Geçişleri tahmin et
passes = calc.find_passes(ankara, datetime.now(), duration_days=3.0)
```

### Önemli Sabitler

- **EARTH_RADIUS**: 6371.0 km
- **EARTH_MU**: 398600.4418 km³/s²
- **EARTH_ROTATION_RATE**: 7.2921150e-5 rad/s

---

## 🎮 SITL Simülatörü (sitl/)

### `cubesat_simulator.py`

Tam sistem simülasyonu - sensörler, telemetri, güç yönetimi, termal model.

**Özellikler:**
- Gerçekçi sensör veri üretimi
  - Magnetometre (RM3100): 3-axis magnetic field
  - GPS (NEO-M9N): Orbital position simulation
  - Barometer (BMP388): Atmospheric modeling
  - Radiation sensor (GQ-511A): Space radiation levels
  
- Güç yönetimi
  - Batarya şarj/deşarj simülasyonu
  - Güneş paneli modeli (eclipse cycles)
  - Batarya voltajı ve SOC hesaplaması
  
- Termal model
  - CPU ısı üretimi
  - Radyatif soğutma
  - Uzay ortamı sıcaklığı

- Sistem modları
  - NORMAL: Tam fonksiyonel
  - SAFE: Kısıtlı işlem
  - RECOVERY: Hata kurtarma
  - POWER_DOWN: Minimum güç

**Kullanım Örneği:**
```python
from cubesat_simulator import CubeSatSimulator

# Simülatör oluştur (1x gerçek-zamanlı)
sim = CubeSatSimulator(simulation_speed=1.0)

# Başlat
sim.start_simulation()

# Sensör verisi al
sensors = sim.get_sensor_snapshot()
print(f"Battery SOC: {sensors['battery_soc']}%")
print(f"Core Temp: {sensors['cpu_temp']}°C")

# Komut gönder
sim.send_command(0x01)  # Reset
sim.send_command(0x02)  # Enter Safe Mode

# Telemetriyi kaydet
sim.save_telemetry('output.json')

# Durdur
sim.stop_simulation()
```

### Sistem Durumu Sorgulama

```python
status = sim.get_system_status()
# Returns: {
#   'timestamp': <float>,
#   'mode': 'NORMAL'|'SAFE'|'RECOVERY'|'POWER_DOWN',
#   'power_state': 'CRITICAL'|'LOW'|'NOMINAL'|'CHARGING',
#   'uptime_seconds': <int>,
#   'battery_soc': <float>,
#   'core_temp': <float>,
#   'telemetry_count': <int>
# }
```

---

### `flight_software_test.py`

Flight software için otomatik test suite.

**Test Kategorileri:**
1. **Simülatör Başlatma** - Sistem initialization
2. **Sensör Veri Üretimi** - Tüm sensörlerin veri üretim kontrolü
3. **Güç Yönetimi** - Batarya ve güç sistemi
4. **Termal Kontrol** - Sıcaklık simülasyonu
5. **Komut İşleme** - Komutu alma ve işlem
6. **Sistem Modları** - Mod geçişleri
7. **Telemetri Toplama** - Veri logging

**Çalıştırma:**
```bash
python flight_software_test.py
```

**Çıktı:**
```
================================================================================
CUBESAT FLIGHT SOFTWARE - INTEGRATION TEST SUITE
================================================================================

[TEST 1] Simülatör Başlatma
  ✓ Simülatör başarıyla başlatıldı
  ✓ Sistem modu: NORMAL

[TEST 2] Sensör Veri Üretimi
  ✓ Magnetometre (RM3100) veri üretiyor
  ✓ GPS (NEO-M9N) veri üretiyor
  ...

================================================================================
TEST SONUÇLARI
================================================================================
  ✓ PASS | Simülatör Başlatma
  ✓ PASS | Sensör Veri Üretimi
  ...
================================================================================
Sonuç: 7/7 test başarılı
🎉 TÜM TESTLER BAŞARILI!
```

---

## ⚙️ Konfigürasyon (config.json)

Sistem parametreleri ve kalibrasyonu JSON formatında.

### Ana Bölümler

- **satellite**: Uydu özellikleri
- **orbit**: Yörünge parametreleri
- **power_system**: Batarya, güneş panelleri, EPS
- **attitude_determination**: Magnetometre kalibrasyonu, sensörler
- **communication**: LoRa, CSP, AES şifreleme
- **payload**: Sensör konfigürasyonu
- **microcontroller**: STM32H743 özellikleri
- **rtos**: FreeRTOS task tanımları
- **simulation**: Simülasyon ayarları

### Magnetometre Kalibrasyonu

```json
"calibration": {
  "hard_iron_x_uT": 5.2,
  "hard_iron_y_uT": -3.1,
  "hard_iron_z_uT": 2.8,
  "soft_iron_matrix": [
    [1.05, -0.02, 0.01],
    [-0.02, 1.03, 0.00],
    [0.01, 0.00, 1.02]
  ]
}
```

---

## 📊 Telemetri Çıktısı

Simülasyon JSON formatında telemetri kaydı oluşturur.

**Örnek (`sitl_telemetry.json`):**
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
      "battery_voltage": 3.95,
      "battery_soc": 80.5,
      "cpu_temp": 28.3,
      ...
    },
    ...
  ],
  "system_status": {
    "mode": "NORMAL",
    "power_state": "NOMINAL",
    "uptime_seconds": 30,
    ...
  }
}
```

---

## 🔄 Benzetim Döngüsü

Simülatör aşağıdaki periyotta çalışır:

1. **100 ms Tick**
   - Sensör verisi güncelle (orbital mekanik)
   - Güç sistemi güncelle (batarya, paneller)
   - Termal model güncelle (ısı akışı)
   - Komutları işle
   - Sistem durumunu güncelle
   - Telemetri kaydı yap

2. **10 saniye Telemetri Topla**
   - Anlık sensör verilerini arabellek'e kaydet
   - Maksimum 1000 kayıt (otomatik FIFO ile eski veriler silinir)

3. **Komut İşleme (anında)**
   - Reset, Safe Mode, Normal Mode, Antenna Deploy komutları

---

## 🚀 Kullanım Örnekleri

### Örnek 1: Hızlı Sistem Testi

```python
from sitl.cubesat_simulator import CubeSatSimulator
import time

sim = CubeSatSimulator(simulation_speed=10.0)  # 10x hızlı
sim.start_simulation()

for i in range(100):
    time.sleep(0.1)
    status = sim.get_system_status()
    print(f"Mode: {status['mode']}, Battery: {status['battery_soc']:.1f}%")

sim.save_telemetry('fast_test.json')
sim.stop_simulation()
```

### Örnek 2: Yörünge Geçiş Tahmini

```python
from orbit_analysis.orbit_calculator import OrbitCalculator

passes = calc.find_passes(ankara, start_time, duration_days=7)
for p in passes[:5]:
    print(f"Geçiş: {p['rise_time']} - {p['set_time']}")
    print(f"  Süre: {p['duration_seconds']/60:.1f} min")
    print(f"  Maksimum Görüş Açısı: {p['max_elevation']:.1f}°")
```

### Örnek 3: Power Budget Analizi

```python
sim = CubeSatSimulator()
sim.start_simulation()

# 1 saatlik simülasyon
for _ in range(3600):
    sensors = sim.get_sensor_snapshot()
    # SOC değişimini analiz et

telemetry = sim.get_telemetry_buffer()
soc_values = [t['battery_soc'] for t in telemetry]
avg_discharge = (soc_values[0] - soc_values[-1]) / len(telemetry)
```

---

## 📈 Simülasyon Parametreleri

### Batarya Modeli
- Kapasitesi: 20 Ah (72 Wh)
- Şarj hızı: 100 mA
- Deşarj hızı: 200 mA (Normal mod)
- Voltaj aralığı: 3.0V - 4.2V

### Termal Model
- Ambient: -40°C (uzay)
- Radyatif soğutma: 0.1 K/s (temp_diff/100)
- İsı üretimi: 0.5W (mod'a göre değişir)

### Yörünge Modeli
- Periyot: 90 dakika
- İrtifa: 360 km
- Eğim: 98°
- Eclipse oranı: ~35% (sun-sync)

---

## ⚠️ Limitasyonlar

- **Simülasyon hızı**: 10x'e kadar test edilmiş
- **Maximum telemetri kaydı**: 1000 (FIFO buffer)
- **Sensör noise**: Basit sinuzoidal model (gerçek rastgelelik değil)
- **Orbital propagation**: Simplified Kepler model
- **Thermal**: Linearized Stefan-Boltzmann

---

## 🔧 Geliştirilmesi Planlanan Özellikler

- [ ] Momentum dumping simülasyonu
- [ ] Detaylı hareket denklemleri
- [ ] Gerçek TLE entegrasyonu
- [ ] Ground station interface
- [ ] 3D visualization
- [ ] Real-time plot gösterimi
- [ ] Anomaly injection for testing

---

## 📚 İlgili Belgeler

- [Flight Software Architecture](../fsw/README.md)
- [Orbit Analysis](./orbit_analysis/)
- [SITL Simulator](./sitl/)
- [System Configuration](./config.json)

---

**Son Güncelleme**: 2026-03-28  
**Versiyon**: 1.0.0
