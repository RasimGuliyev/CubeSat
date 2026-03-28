# fsw/CMakeLists.txt dosyasının ilgili bölümü

# 1. libcsp'ye FreeRTOS kullandığımızı belirtiyoruz
set(CSP_HAVE_FREERTOS ON CACHE BOOL "Enable FreeRTOS support in CSP" FORCE)

# 2. Klasörü CMake'e dahil et
add_subdirectory(Middleware/libcsp)

# 3. Kendi uçuş yazılımımızı (cubesat_fsw) derlerken CSP'yi ona bağla (Link)
target_link_libraries(cubesat_fsw PRIVATE csp)

# 4. CSP'nin başlık (header) dosyalarını göster
target_include_directories(cubesat_fsw PRIVATE Middleware/libcsp/include)

# ============================================================================
# TEST EXECUTABLE - Sistem testi
# ============================================================================

add_executable(test_cubesat 
    Tests/test_system.c
    Tasks/Src/task_adcs.c
    Tasks/Src/task_payload.c
    Tasks/Src/task_comm.c
    Tasks/Src/task_eps.c
    Drivers/Payload/radiation_sensor.c
    Drivers/Payload/barometer.c
    Drivers/Payload/gps.c
    Drivers/Storage/sd_card.c
    Drivers/Magnetometer/rm3100.c
    Middleware/AES256/crypto.c
)

target_include_directories(test_cubesat PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/FreeRTOS
    ${CMAKE_CURRENT_SOURCE_DIR}/Tasks/Src
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/Payload
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/Storage
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/Magnetometer
    ${CMAKE_CURRENT_SOURCE_DIR}/Middleware/AES256
)

target_link_libraries(test_cubesat PRIVATE 
    m
    csp
)

# Test çalıştırılması
enable_testing()
add_test(NAME SystemTest COMMAND test_cubesat)