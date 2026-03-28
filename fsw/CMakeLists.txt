# fsw/CMakeLists.txt dosyasının ilgili bölümü

# 1. libcsp'ye FreeRTOS kullandığımızı belirtiyoruz
set(CSP_HAVE_FREERTOS ON CACHE BOOL "Enable FreeRTOS support in CSP" FORCE)

# 2. Klasörü CMake'e dahil et
add_subdirectory(Middleware/libcsp)

# 3. Kendi uçuş yazılımımızı (cubesat_fsw) derlerken CSP'yi ona bağla (Link)
target_link_libraries(cubesat_fsw PRIVATE csp)

# 4. CSP'nin başlık (header) dosyalarının yolunu göster
target_include_directories(cubesat_fsw PRIVATE Middleware/libcsp/include)