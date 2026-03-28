# sim/run_simulation.py
# CubeSat SITL - Ana başlatıcı script

import sys
import os
from pathlib import Path

# Add modules to path
sys.path.insert(0, str(Path(__file__).parent))

def main():
    """Main simulation launcher"""
    print("""
    ╔═══════════════════════════════════════════════════════════════════════╗
    ║         ULAS-S1 CUBESAT - SOFTWARE-IN-THE-LOOP SIMULATOR             ║
    ╚═══════════════════════════════════════════════════════════════════════╝
    
    Available modes:
    
    1. Flight Software Integration Test
       Run: python run_simulation.py test
       
    2. Orbital Analysis & Pass Prediction
       Run: python run_simulation.py orbit
       
    3. Interactive SITL Console
       Run: python run_simulation.py console
       
    4. Batch Simulation & Data Collection
       Run: python run_simulation.py batch
    
    ─────────────────────────────────────────────────────────────────────────
    """)
    
    if len(sys.argv) < 2:
        print("Usage: python run_simulation.py <mode>")
        print("\nModes: test, orbit, console, batch")
        return 1
    
    mode = sys.argv[1].lower()
    
    if mode == "test":
        print("🧪 Running Flight Software Integration Tests...\n")
        from sitl.flight_software_test import FlightSoftwareTest
        test = FlightSoftwareTest()
        try:
            test.run_all_tests()
        finally:
            test.cleanup()
        return 0
    
    elif mode == "orbit":
        print("🛰️  Running Orbital Analysis & Pass Prediction...\n")
        from orbit_analysis.orbit_calculator import OrbitCalculator, ULAS_S1_TLE, GroundStation
        from datetime import datetime
        
        calc = OrbitCalculator(ULAS_S1_TLE)
        
        stations = [
            GroundStation("ANKARA", 39.93, 32.86, 950, 10.0),
            GroundStation("ISTANBUL", 41.01, 28.98, 100, 10.0),
            GroundStation("KONYA", 37.87, 32.49, 1035, 10.0),
        ]
        
        print(f"Orbital Period: {ULAS_S1_TLE.orbital_period:.2f} minutes")
        print(f"Apogee: {ULAS_S1_TLE.altitude_apogee:.1f} km")
        print(f"Perigee: {ULAS_S1_TLE.altitude_perigee:.1f} km\n")
        
        start = ULAS_S1_TLE.epoch
        
        for station in stations:
            print(f"\n{'='*70}")
            print(f"Ground Station: {station.name}")
            print(f"Location: {station.latitude:.2f}°N, {station.longitude:.2f}°E, {station.altitude}m")
            print(f"{'='*70}")
            
            passes = calc.find_passes(station, start, duration_days=7.0)
            
            if not passes:
                print("No passes in next 7 days\n")
                continue
            
            for i, p in enumerate(passes[:5], 1):
                rise = p['rise_time'].strftime("%Y-%m-%d %H:%M:%S")
                set_time = p['set_time'].strftime("%H:%M:%S")
                duration = p['duration_seconds'] / 60.0
                elevation = p['max_elevation']
                
                print(f"\nPass {i}:")
                print(f"  Rise: {rise}")
                print(f"  Set:  {set_time}")
                print(f"  Duration: {duration:.1f} minutes")
                print(f"  Max Elevation: {elevation:.1f}°")
        
        return 0
    
    elif mode == "console":
        print("📟 Interactive SITL Console\n")
        from sitl.cubesat_simulator import CubeSatSimulator
        import time
        
        sim = CubeSatSimulator(simulation_speed=1.0)
        sim.start_simulation()
        
        print("Type 'help' for commands, 'quit' to exit\n")
        
        try:
            while True:
                cmd = input("cubesat> ").strip().lower()
                
                if cmd == "quit":
                    break
                elif cmd == "help":
                    print("""
Commands:
  status    - Show system status
  sensors   - Print current sensor values
  mode      - Show current system mode
  cmd <id>  - Send command (01=Reset, 02=Safe, 03=Normal)
  telemetry - Show telemetry buffer size
  save      - Save telemetry to file
  quit      - Exit simulator
                    """)
                elif cmd == "status":
                    status = sim.get_system_status()
                    print(f"Mode: {status['mode']}")
                    print(f"Power State: {status['power_state']}")
                    print(f"Battery SOC: {status['battery_soc']:.1f}%")
                    print(f"Uptime: {status['uptime_seconds']}s")
                    print(f"Telemetry Records: {status['telemetry_count']}")
                
                elif cmd == "sensors":
                    sensors = sim.get_sensor_snapshot()
                    print(f"Battery: {sensors['battery_voltage']:.2f}V ({sensors['battery_soc']:.1f}%)")
                    print(f"Temperature: {sensors['cpu_temp']:.1f}°C")
                    print(f"Magnetometer: ({sensors['mag_x']:.1f}, {sensors['mag_y']:.1f}, {sensors['mag_z']:.1f}) µT")
                    print(f"GPS: {sensors['gps_lat']:.2f}°N, {sensors['gps_lon']:.2f}°E (Alt: {sensors['gps_alt']:.0f}m)")
                    print(f"Radiation: {sensors['radiation_cpm']} CPM")
                
                elif cmd.startswith("cmd "):
                    try:
                        cmd_id = int(cmd.split()[1], 16)
                        sim.send_command(cmd_id)
                        print(f"✓ Command 0x{cmd_id:02X} sent")
                    except:
                        print("Invalid command format. Use: cmd <hex_id>")
                
                elif cmd == "telemetry":
                    telem = sim.get_telemetry_buffer()
                    print(f"Telemetry records: {len(telem)}")
                    if telem:
                        print(f"  First: {telem[0]['timestamp']}")
                        print(f"  Last: {telem[-1]['timestamp']}")
                
                elif cmd == "save":
                    sim.save_telemetry("console_telemetry.json")
                    print("✓ Telemetry saved to console_telemetry.json")
                
                elif cmd == "mode":
                    print(f"Current mode: {sim.system_mode.value}")
                
                elif cmd == "":
                    pass
                else:
                    print(f"Unknown command: {cmd}")
        
        except KeyboardInterrupt:
            print("\nInterrupted by user")
        finally:
            sim.stop_simulation()
        
        return 0
    
    elif mode == "batch":
        print("📊 Batch Simulation - Collecting Long-Term Data...\n")
        from sitl.cubesat_simulator import CubeSatSimulator
        import time
        import json
        
        sim = CubeSatSimulator(simulation_speed=10.0)  # 10x speed for faster collection
        sim.start_simulation()
        
        print("Running for 10 simulated hours (36 seconds real-time)...\n")
        
        progress_interval = 36 / 10  # Update every minute simulated
        
        for minute in range(1, 601):
            time.sleep(0.06)  # 360 seconds / 6000 = 0.06 per 10 seconds
            
            if minute % 100 == 0:
                status = sim.get_system_status()
                print(f"[{minute//60}h {minute%60:02d}m] "
                      f"Mode: {status['mode']:8s} | "
                      f"Battery: {status['battery_soc']:5.1f}% | "
                      f"Records: {status['telemetry_count']:4d}")
        
        print("\nSimulation complete. Saving data...\n")
        sim.save_telemetry("batch_simulation_data.json")
        
        # Print summary
        telemetry = sim.get_telemetry_buffer()
        if telemetry:
            socs = [t['battery_soc'] for t in telemetry]
            temps = [t['cpu_temp'] for t in telemetry]
            
            print(f"Telemetry Summary:")
            print(f"  Records: {len(telemetry)}")
            print(f"  Battery SOC: {min(socs):.1f}% - {max(socs):.1f}%")
            print(f"  Temperature: {min(temps):.1f}°C - {max(temps):.1f}°C")
        
        sim.stop_simulation()
        return 0
    
    else:
        print(f"Unknown mode: {mode}")
        return 1

if __name__ == "__main__":
    sys.exit(main())
