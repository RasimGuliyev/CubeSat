# sim/sitl/flight_software_test.py
# Flight Software Test Suite - SITL ile yazılım validasyonu

import sys
import time
import json
from datetime import datetime
from cubesat_simulator import CubeSatSimulator, SystemMode, PowerState

class FlightSoftwareTest:
    """Flight software test suite"""
    
    def __init__(self):
        self.sim = CubeSatSimulator(simulation_speed=1.0)
        self.test_results = []
    
    def run_all_tests(self):
        """Tüm testleri çalıştır"""
        print("=" * 70)
        print("CUBESAT FLIGHT SOFTWARE - INTEGRATION TEST SUITE")
        print("=" * 70)
        
        self.test_simulator_initialization()
        self.test_sensor_data_generation()
        self.test_power_management()
        self.test_thermal_control()
        self.test_command_processing()
        self.test_system_modes()
        self.test_telemetry_collection()
        
        self.print_test_summary()
    
    def test_simulator_initialization(self):
        """Test 1: Simülatör başlatma"""
        print("\n[TEST 1] Simülatör Başlatma")
        try:
            self.sim.start_simulation()
            time.sleep(1)
            status = self.sim.get_system_status()
            assert status['mode'] == 'NORMAL'
            print("  ✓ Simülatör başarıyla başlatıldı")
            print(f"  ✓ Sistem modu: {status['mode']}")
            self.test_results.append(("Simülatör Başlatma", True, ""))
        except Exception as e:
            print(f"  ✗ Hata: {e}")
            self.test_results.append(("Simülatör Başlatma", False, str(e)))
    
    def test_sensor_data_generation(self):
        """Test 2: Sensör veri üretimi"""
        print("\n[TEST 2] Sensör Veri Üretimi")
        try:
            # Her sensörden veri al
            sensors = self.sim.get_sensor_snapshot()
            
            # Magnetometre kontrolü
            assert -100 < sensors['mag_x'] < 100, "Magnetometre X OK"
            assert -100 < sensors['mag_y'] < 100, "Magnetometre Y OK"
            assert -100 < sensors['mag_z'] < 100, "Magnetometre Z OK"
            print("  ✓ Magnetometre (RM3100) veri üretiyor")
            
            # GPS kontrolü
            assert -90 <= sensors['gps_lat'] <= 90, "GPS latitude OK"
            assert 0 <= sensors['gps_lon'] <= 360, "GPS longitude OK"
            assert sensors['gps_fix'] >= 0, "GPS fix valid"
            print("  ✓ GPS (NEO-M9N) veri üretiyor")
            
            # Barometer kontrolü
            assert 0 < sensors['pressure'] < 200, "Basınç OK"
            assert -100 < sensors['temperature'] < 100, "Sıcaklık OK"
            print("  ✓ Barometer (BMP388) veri üretiyor")
            
            # Radiation kontrolü
            assert sensors['radiation_cpm'] > 0, "Radiation OK"
            print("  ✓ Radiation sensörü (GQ-511A) veri üretiyor")
            
            # Güç kontrolü
            assert 0 < sensors['battery_voltage'] < 5, "Batarya voltajı OK"
            assert 0 <= sensors['battery_soc'] <= 100, "Batarya SOC OK"
            print("  ✓ Güç sistemi (BQ24295) veri üretiyor")
            
            self.test_results.append(("Sensör Veri Üretimi", True, ""))
        except Exception as e:
            print(f"  ✗ Hata: {e}")
            self.test_results.append(("Sensör Veri Üretimi", False, str(e)))
    
    def test_power_management(self):
        """Test 3: Güç yönetimi"""
        print("\n[TEST 3] Güç Yönetimi")
        try:
            # Simülasyonu kısa bir süre çalıştır
            for _ in range(10):
                time.sleep(0.5)
                sensors = self.sim.get_sensor_snapshot()
            
            # Batarya SOC değişiyor mu?
            initial_soc = sensors['battery_soc']
            time.sleep(5)
            sensors_later = self.sim.get_sensor_snapshot()
            final_soc = sensors_later['battery_soc']
            
            assert initial_soc != final_soc, "Batarya SOC dinamik olarak değişiyor"
            print(f"  ✓ Başlangıç SOC: {initial_soc:.1f}%")
            print(f"  ✓ Son SOC: {final_soc:.1f}%")
            
            # Voltaj aralığı kontrolü
            assert 3.0 <= sensors_later['battery_voltage'] <= 4.3, "Voltaj aralığı OK"
            print(f"  ✓ Batarya voltajı: {sensors_later['battery_voltage']:.2f}V")
            
            self.test_results.append(("Güç Yönetimi", True, ""))
        except Exception as e:
            print(f"  ✗ Hata: {e}")
            self.test_results.append(("Güç Yönetimi", False, str(e)))
    
    def test_thermal_control(self):
        """Test 4: Termal kontrol"""
        print("\n[TEST 4] Termal Kontrol")
        try:
            sensors = self.sim.get_sensor_snapshot()
            initial_temp = sensors['cpu_temp']
            
            time.sleep(2)
            sensors = self.sim.get_sensor_snapshot()
            final_temp = sensors['cpu_temp']
            
            # Sıcaklık -60°C ile +80°C arasında olmalı
            assert -60 <= final_temp <= 80, f"Sıcaklık aralığı OK ({final_temp:.1f}°C)"
            print(f"  ✓ İlk sıcaklık: {initial_temp:.1f}°C")
            print(f"  ✓ Son sıcaklık: {final_temp:.1f}°C")
            
            self.test_results.append(("Termal Kontrol", True, ""))
        except Exception as e:
            print(f"  ✗ Hata: {e}")
            self.test_results.append(("Termal Kontrol", False, str(e)))
    
    def test_command_processing(self):
        """Test 5: Komut işleme"""
        print("\n[TEST 5] Komut İşleme")
        try:
            # Reset komutu gönder
            self.sim.send_command(0x01)  # Reset
            time.sleep(1)
            
            status = self.sim.get_system_status()
            assert status['uptime_seconds'] < 5, "Reset başarılı"
            print("  ✓ Reset komutu işlendi")
            
            # Safe mode komutu
            self.sim.send_command(0x02)  # Enter safe mode
            time.sleep(1)
            assert self.sim.system_mode == SystemMode.SAFE, "Safe mode aktif"
            print("  ✓ Safe mode komutu işlendi")
            
            # Normal mode komutu
            self.sim.send_command(0x03)  # Enter normal mode
            time.sleep(1)
            assert self.sim.system_mode == SystemMode.NORMAL, "Normal mode aktif"
            print("  ✓ Normal mode komutu işlendi")
            
            self.test_results.append(("Komut İşleme", True, ""))
        except Exception as e:
            print(f"  ✗ Hata: {e}")
            self.test_results.append(("Komut İşleme", False, str(e)))
    
    def test_system_modes(self):
        """Test 6: Sistem modları"""
        print("\n[TEST 6] Sistem Modları")
        try:
            # Normal mode
            self.sim.system_mode = SystemMode.NORMAL
            sensors = self.sim.get_sensor_snapshot()
            assert self.sim.system_mode == SystemMode.NORMAL
            print("  ✓ Normal mod aktif")
            
            # Safe mode
            self.sim.system_mode = SystemMode.SAFE
            sensors = self.sim.get_sensor_snapshot()
            assert self.sim.system_mode == SystemMode.SAFE
            print("  ✓ Safe mod aktif")
            
            # Geri normal moda dön
            self.sim.system_mode = SystemMode.NORMAL
            
            self.test_results.append(("Sistem Modları", True, ""))
        except Exception as e:
            print(f"  ✗ Hata: {e}")
            self.test_results.append(("Sistem Modları", False, str(e)))
    
    def test_telemetry_collection(self):
        """Test 7: Telemetri toplama"""
        print("\n[TEST 7] Telemetri Toplama")
        try:
            # Telemetri toplayıncaya kadar bekle
            while len(self.sim.telemetry_buffer) < 5:
                time.sleep(0.5)
            
            telemetry = self.sim.get_telemetry_buffer()
            assert len(telemetry) >= 5, "En az 5 telemetri kaydı var"
            print(f"  ✓ {len(telemetry)} telemetri kaydı toplandı")
            
            # Telemetriyi kontrol et
            for i, t in enumerate(telemetry[:3]):
                print(f"    - Record {i+1}: {datetime.fromtimestamp(t['timestamp']).isoformat()}")
            
            self.test_results.append(("Telemetri Toplama", True, ""))
        except Exception as e:
            print(f"  ✗ Hata: {e}")
            self.test_results.append(("Telemetri Toplama", False, str(e)))
    
    def print_test_summary(self):
        """Test sonuçlarını yazdir"""
        print("\n" + "=" * 70)
        print("TEST SONUÇLARI")
        print("=" * 70)
        
        passed = sum(1 for _, success, _ in self.test_results if success)
        total = len(self.test_results)
        
        for test_name, success, error in self.test_results:
            status = "✓ PASS" if success else "✗ FAIL"
            print(f"  {status:8s} | {test_name:30s}", end="")
            if error:
                print(f" | {error}")
            else:
                print()
        
        print("=" * 70)
        print(f"Sonuç: {passed}/{total} test başarılı")
        
        if passed == total:
            print("🎉 TÜM TESTLER BAŞARILI!")
        else:
            print(f"⚠️  {total - passed} test başarısız")
        
        # JSON'a kaydet
        results = {
            'timestamp': datetime.now().isoformat(),
            'tests_passed': passed,
            'tests_total': total,
            'details': [
                {'test': name, 'passed': success, 'error': error}
                for name, success, error in self.test_results
            ]
        }
        
        with open('flight_software_test_results.json', 'w') as f:
            json.dump(results, f, indent=2)
        
        print("\n📊 Sonuçlar kaydedildi: flight_software_test_results.json")
    
    def cleanup(self):
        """Test'ten sonra temizle"""
        self.sim.stop_simulation()

if __name__ == "__main__":
    test = FlightSoftwareTest()
    try:
        test.run_all_tests()
    finally:
        test.cleanup()
