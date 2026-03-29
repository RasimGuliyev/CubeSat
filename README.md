# Measuring of Radiation inside Stratosphere due to Solar Storms by CUBESAT
TUA Astro Hackathon Null-Space Team Cubesat software prototype by:                                 
Aslı Nur Yüksel                     
Esma Nur Öztürk                            
Ayşe Karar                           
Ahmet Yıldız                         
Rasim Guliyev                         

# This prototype includes
```
Own Flight Software (FSW)
Own Ground Software (GSW)

```

!! However this project stands highly just for architecture and main skeleton, and it is not fully functional !!

For extra information and hardware components -> [Project inventory](PROJECT_INVENTORY.md)


## 📁 File Hierarchy

```
CubeSat/
├── fsw/                           # Flight Software
│   ├── CMakeLists.txt
│   ├── main.c                     # System dispatcher + FreeRTOS setup
│   ├── include/
│   │   └── FreeRTOS/
│   │       ├── FreeRTOS.h         # Mock FreeRTOS header
│   │       ├── FreeRTOS.c         # Mock implementations
│   │       ├── task.h
│   │       ├── queue.h
│   │       └── semphr.h
│   ├── Drivers/
│   │   ├── Payload/               # Payload sensors
│   │   │   ├── radiation_sensor.h/c
│   │   │   ├── barometer.h/c
│   │   │   └── gps.h/c
│   │   ├── Storage/               # Data storage
│   │   │   └── sd_card.h/c        # FatFS SD card driver
│   │   ├── Magnetometer/
│   │   │   ├── rm3100.h           # RM3100 magnetometer header
│   │   │   └── rm3100.c           # I2C driver + calibration
│   │   └── LoRa_RFM98W/
│   │       └── lora_csp_interface.c
│   ├── Middleware/
│   │   ├── AES256/
│   │   │   ├── crypto.h/c         # AES-256 CTR encryption
│   │   │   ├── aes.h              # TinyAES mock header
│   │   │   └── aes.c              # TinyAES mock implementation
│   │   └── libcsp/                # Cubesat Space Protocol
│   │       ├── include/csp/autoconfig.h
│   │       └── src/
│   ├── Tasks/Src/
│   │   ├── task_eps.h/c           # Power management (BQ24295)
│   │   ├── task_adcs.h/c          # Attitude control + Kalman
│   │   ├── task_payload.h/c       # Sensor fusion
│   │   └── task_comm.h/c          # LoRa + CSP communication
│   ├── Tests/
│   │   └── test_system.c          # Integration test
│   └── build/
│       ├── test_system.exe        # System integration test binary
│       └── test_cubesat.exe       # Main CubeSat binary
│
├── gsw/                           # Ground Station Software
│   ├── app.py                     # Flask web server
│   ├── decoder.py                 # CSP packet decoder + AES decryption
│   ├── docker-compose.yml
│   └── requirements.txt            # Python dependencies
│
├── shared/                        # Shared headers & definitions
│   ├── telemetry.h                # Telemetry packet structures
│   ├── commands.h                 # Command definitions
│   └── telemetry_packets.json
│
├── sim/                           # Simulation & Analysis
│   ├── config.json                # System configuration
│   ├── requirements.txt            # Python dependencies
│   ├── run_simulation.py           # Main simulation launcher
│   ├── README.md                  # Simulation documentation
│   ├── QUICKSTART.md              # Usage guide
│   ├── orbit_analysis/
│   │   ├── __init__.py
│   │   └── orbit_calculator.py    # Orbital mechanics + pass prediction
│   └── sitl/
│       ├── __init__.py
│       ├── cubesat_simulator.py   # SITL simulator
│       └── flight_software_test.py # Test suite
│
└── README.md                      # Project overview
```
