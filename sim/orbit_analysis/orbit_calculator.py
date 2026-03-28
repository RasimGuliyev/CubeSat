# sim/orbit_analysis/orbit_calculator.py
# Ulaş-S1 Yörünge Analiz Modülü
# Yörünge parametreleri, geçiş tahmini, kapsama alanı hesaplaması

import math
import numpy as np
from datetime import datetime, timedelta
from dataclasses import dataclass
from typing import List, Tuple

# Dünya sabitleri
EARTH_RADIUS = 6371.0  # km
EARTH_MU = 398600.4418  # km³/s² (Gravitational parameter)
EARTH_ROTATION_RATE = 7.2921150e-5  # rad/s
EARTH_F = 1.0 / 298.257223563  # Flattening factor

@dataclass
class OrbitalState:
    """Yörünge durumu (TLE tabanlı)"""
    epoch: datetime
    semi_major_axis: float  # km
    eccentricity: float
    inclination: float  # degrees
    raan: float  # Right Ascension of Ascending Node (degrees)
    arg_perigee: float  # Argument of Perigee (degrees)
    mean_anomaly: float  # degrees
    mean_motion: float  # revolutions per day
    
    @property
    def altitude_apogee(self) -> float:
        """Apojee yüksekliği"""
        return self.semi_major_axis * (1 + self.eccentricity) - EARTH_RADIUS
    
    @property
    def altitude_perigee(self) -> float:
        """Perijee yüksekliği"""
        return self.semi_major_axis * (1 - self.eccentricity) - EARTH_RADIUS
    
    @property
    def orbital_period(self) -> float:
        """Yörünge periyodu (dakikalar)"""
        return 1440.0 / self.mean_motion  # 1440 = minutes per day

@dataclass
class GroundStation:
    """Yer istasyonu koordinatları"""
    name: str
    latitude: float  # degrees
    longitude: float  # degrees (0° to 360°)
    altitude: float  # meters
    antenna_elevation: float = 10.0  # degrees (minimum elevation mask)

class OrbitCalculator:
    """Yörünge hesaplamaları"""
    
    def __init__(self, satellite: OrbitalState):
        """
        Initialize with satellite orbital elements
        
        Args:
            satellite: OrbitalState object with TLE parameters
        """
        self.satellite = satellite
    
    def mean_anomaly_at_time(self, time_offset: float) -> float:
        """
        Verilen zaman için ortalama anoaliyi hesapla
        
        Args:
            time_offset: Epoch'tan dakika cinsinden süre
            
        Returns:
            Mean anomaly in degrees (0-360)
        """
        # Mean motion: revolutions per day → degrees per minute
        deg_per_minute = self.satellite.mean_motion * 360.0 / 1440.0
        ma = self.satellite.mean_anomaly + deg_per_minute * time_offset
        return ma % 360.0
    
    def eccentric_anomaly(self, mean_anomaly: float, tolerance: float = 1e-6) -> float:
        """
        Ortalama anomoliden eksantrik anomaliyi Newton-Raphson ile hesapla
        
        Args:
            mean_anomaly: Mean anomaly in degrees
            tolerance: Convergence tolerance
            
        Returns:
            Eccentric anomaly in radians
        """
        ma_rad = math.radians(mean_anomaly)
        e_rad = ma_rad  # Initial guess
        
        # Newton-Raphson iteration
        for _ in range(100):
            error = e_rad - self.satellite.eccentricity * math.sin(e_rad) - ma_rad
            if abs(error) < tolerance:
                break
            e_rad = e_rad - error / (1 - self.satellite.eccentricity * math.cos(e_rad))
        
        return e_rad
    
    def true_anomaly(self, eccentric_anomaly: float) -> float:
        """
        Eksantrik anomoliden gerçek anomaliyi hesapla
        
        Args:
            eccentric_anomaly: Eccentric anomaly in radians
            
        Returns:
            True anomaly in radians
        """
        e = self.satellite.eccentricity
        ta = 2 * math.atan2(
            math.sqrt(1 + e) * math.sin(eccentric_anomaly / 2),
            math.sqrt(1 - e) * math.cos(eccentric_anomaly / 2)
        )
        return ta
    
    def satellite_position(self, time: datetime) -> Tuple[float, float, float]:
        """
        Verilen zaman için uydunun ECEF koordinatlarını hesapla
        
        Args:
            time: DateTime object
            
        Returns:
            (x, y, z) in kilometers (ECEF system)
        """
        # Minutes from epoch
        time_diff = time - self.satellite.epoch
        minutes_from_epoch = time_diff.total_seconds() / 60.0
        
        # Orbital elements
        a = self.satellite.semi_major_axis
        e = self.satellite.eccentricity
        i = math.radians(self.satellite.inclination)
        omega = math.radians(self.satellite.raan)
        w = math.radians(self.satellite.arg_perigee)
        
        # Calculate anomalies
        ma = self.mean_anomaly_at_time(minutes_from_epoch)
        ea = self.eccentric_anomaly(ma)
        ta = self.true_anomaly(ea)
        
        # Orbital radius
        r = a * (1 - e * math.cos(ea))
        
        # Position in orbital plane
        x_orb = r * math.cos(ta)
        y_orb = r * math.sin(ta)
        z_orb = 0.0
        
        # Rotation matrices: orbital → ECEF
        # First rotate by argument of perigee (w)
        x_per = x_orb * math.cos(w) - y_orb * math.sin(w)
        y_per = x_orb * math.sin(w) + y_orb * math.cos(w)
        z_per = z_orb
        
        # Then rotate by inclination (i)
        x_asc = x_per
        y_asc = y_per * math.cos(i) - z_per * math.sin(i)
        z_asc = y_per * math.sin(i) + z_per * math.cos(i)
        
        # Finally rotate by RAAN (omega) and Earth rotation
        sidereal_time = self._greenwich_sidereal_time(time)
        theta = omega + sidereal_time
        
        x_ecef = x_asc * math.cos(theta) - y_asc * math.sin(theta)
        y_ecef = x_asc * math.sin(theta) + y_asc * math.cos(theta)
        z_ecef = z_asc
        
        return (x_ecef, y_ecef, z_ecef)
    
    def _greenwich_sidereal_time(self, time: datetime) -> float:
        """
        Greenwich sidereal time hesapla (radians)
        
        Args:
            time: DateTime object
            
        Returns:
            GST in radians
        """
        # Simplified: use Earth rotation rate
        time_since_epoch = time - self.satellite.epoch
        seconds = time_since_epoch.total_seconds()
        gst = EARTH_ROTATION_RATE * seconds
        return gst % (2 * math.pi)
    
    def distance_to_ground_station(self, satellite_pos: Tuple[float, float, float],
                                  station: GroundStation) -> float:
        """
        Uydu ile yer istasyonu arasındaki mesafeyi hesapla
        
        Args:
            satellite_pos: (x, y, z) in ECEF coordinates (km)
            station: GroundStation object
            
        Returns:
            Distance in kilometers
        """
        # Ground station ECEF coordinates
        lat_rad = math.radians(station.latitude)
        lon_rad = math.radians(station.longitude)
        h = station.altitude / 1000.0  # Convert to km
        
        # WGS84 ellipsoid
        N = EARTH_RADIUS / math.sqrt(1 - EARTH_F * (2 - EARTH_F) * math.sin(lat_rad) ** 2)
        x_gs = (N + h) * math.cos(lat_rad) * math.cos(lon_rad)
        y_gs = (N + h) * math.cos(lat_rad) * math.sin(lon_rad)
        z_gs = (N * (1 - EARTH_F * (2 - EARTH_F)) + h) * math.sin(lat_rad)
        
        # Distance
        dx = satellite_pos[0] - x_gs
        dy = satellite_pos[1] - y_gs
        dz = satellite_pos[2] - z_gs
        
        return math.sqrt(dx**2 + dy**2 + dz**2)
    
    def elevation_angle(self, satellite_pos: Tuple[float, float, float],
                       station: GroundStation) -> float:
        """
        Yer istasyonundan uydu görüş açısını hesapla
        
        Args:
            satellite_pos: (x, y, z) in ECEF coordinates (km)
            station: GroundStation object
            
        Returns:
            Elevation angle in degrees
        """
        lat_rad = math.radians(station.latitude)
        lon_rad = math.radians(station.longitude)
        h = station.altitude / 1000.0
        
        # Ground station position
        N = EARTH_RADIUS / math.sqrt(1 - EARTH_F * (2 - EARTH_F) * math.sin(lat_rad) ** 2)
        x_gs = (N + h) * math.cos(lat_rad) * math.cos(lon_rad)
        y_gs = (N + h) * math.cos(lat_rad) * math.sin(lon_rad)
        z_gs = (N * (1 - EARTH_F * (2 - EARTH_F)) + h) * math.sin(lat_rad)
        
        # Vector from ground station to satellite
        dx = satellite_pos[0] - x_gs
        dy = satellite_pos[1] - y_gs
        dz = satellite_pos[2] - z_gs
        
        # Local horizon frame (East, North, Up)
        east = np.array([-math.sin(lon_rad), math.cos(lon_rad), 0])
        north = np.array([-math.sin(lat_rad) * math.cos(lon_rad),
                         -math.sin(lat_rad) * math.sin(lon_rad),
                         math.cos(lat_rad)])
        up = np.array([math.cos(lat_rad) * math.cos(lon_rad),
                      math.cos(lat_rad) * math.sin(lon_rad),
                      math.sin(lat_rad)])
        
        sat_vec = np.array([dx, dy, dz])
        
        # Project to local horizon
        east_comp = np.dot(sat_vec, east)
        north_comp = np.dot(sat_vec, north)
        up_comp = np.dot(sat_vec, up)
        
        # Elevation angle
        distance = np.linalg.norm(sat_vec)
        elevation = math.degrees(math.asin(up_comp / distance))
        
        return elevation
    
    def find_passes(self, station: GroundStation, start_time: datetime,
                   duration_days: float = 1.0) -> List[dict]:
        """
        Verilen yer istasyonu için geçiş tahmini
        
        Args:
            station: GroundStation object
            start_time: Start time for prediction
            duration_days: Duration to predict (days)
            
        Returns:
            List of pass predictions with rise/culmination/set times
        """
        passes = []
        current_time = start_time
        end_time = start_time + timedelta(days=duration_days)
        
        # Check every minute
        step_minutes = 1.0
        above_horizon = False
        pass_start = None
        max_elevation = 0.0
        max_elevation_time = None
        
        while current_time < end_time:
            sat_pos = self.satellite_position(current_time)
            elevation = self.elevation_angle(sat_pos, station)
            
            if elevation >= station.antenna_elevation:
                if not above_horizon:
                    # Pass rising
                    above_horizon = True
                    pass_start = current_time
                    max_elevation = elevation
                    max_elevation_time = current_time
                else:
                    # Update max elevation
                    if elevation > max_elevation:
                        max_elevation = elevation
                        max_elevation_time = current_time
            else:
                if above_horizon:
                    # Pass setting
                    above_horizon = False
                    pass_info = {
                        'rise_time': pass_start,
                        'set_time': current_time,
                        'duration_seconds': (current_time - pass_start).total_seconds(),
                        'max_elevation': max_elevation,
                        'max_elevation_time': max_elevation_time,
                        'station': station.name
                    }
                    passes.append(pass_info)
            
            current_time += timedelta(minutes=step_minutes)
        
        return passes

# Örnek TLE (iki satırlı element set)
# Ulaş-S1 örneği (gerçek TLE'den farklı - demo amaçlı)
ULAS_S1_TLE = OrbitalState(
    epoch=datetime(2026, 3, 28, 0, 0, 0),
    semi_major_axis=6730.0,  # ~360 km altitude
    eccentricity=0.0001,
    inclination=98.0,  # Sun-synchronous-like
    raan=45.0,
    arg_perigee=0.0,
    mean_anomaly=0.0,
    mean_motion=14.13  # ~90 minute orbit
)

if __name__ == "__main__":
    # Test örneği
    calc = OrbitCalculator(ULAS_S1_TLE)
    
    # Yer istasyonları
    stations = [
        GroundStation("ANKARA", 39.93, 32.86, 950, 10.0),
        GroundStation("ISTANBUL", 41.01, 28.98, 100, 10.0),
        GroundStation("KONYA", 37.87, 32.49, 1035, 10.0),
    ]
    
    # Geçiş tahmini
    start = ULAS_S1_TLE.epoch
    print(f"Yörünge Analizi: {ULAS_S1_TLE.orbital_period:.2f} dakika periyot")
    print(f"Apojee: {ULAS_S1_TLE.altitude_apogee:.1f} km")
    print(f"Perijee: {ULAS_S1_TLE.altitude_perigee:.1f} km\n")
    
    for station in stations:
        passes = calc.find_passes(station, start, duration_days=3.0)
        print(f"\n{station.name} için geçişler (3 gün):")
        for i, p in enumerate(passes[:3], 1):  # Show first 3 passes
            print(f"  {i}. Geçiş: {p['rise_time'].isoformat()} → {p['set_time'].isoformat()}")
            print(f"     Süre: {p['duration_seconds']/60:.1f} min, Max elev: {p['max_elevation']:.1f}°")
