cmake_minimum_required(VERSION 3.20)
project(whatsapp_core LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# =========================
# 1. Rutas Locales (Vendor)
# =========================
set(VENDOR_DIR "/home/josecxz/projects/whatsapp-llm/whatsapp-core-cpp/vendor")

list(APPEND CMAKE_PREFIX_PATH 
    "${VENDOR_DIR}/faiss_dist"
    "${VENDOR_DIR}/cpr_dist"
)

# =========================
# 2. Dependencias
# =========================

# --- A. Librerías del Sistema ---
find_package(PkgConfig REQUIRED)
pkg_check_modules(MARIADB REQUIRED libmariadb)
find_package(spdlog REQUIRED)

# CORRECCIÓN: Buscamos OpenMP antes de cargar FAISS
find_package(OpenMP REQUIRED)

# --- B. Librerías Locales (Vendor) ---
find_package(faiss REQUIRED)
message(STATUS "✅ FAISS local encontrado.")

find_package(cpr REQUIRED)
message(STATUS "✅ CPR local encontrado.")

# --- C. JSON ---
include(FetchContent)
find_package(nlohmann_json QUIET)
if(NOT nlohmann_json_FOUND)
    FetchContent_Declare(
        json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG        v3.11.2
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(json)
endif()

# =========================
# 3. Executable
# =========================
add_executable(whatsapp_core
    src/main.cpp
    
    # Ingest
    src/ingest/ingest_controller.cpp
    
    # Persistence
    src/persistence/message_database.cpp
    src/persistence/database_pool.cpp
    src/persistence/repository.cpp
    
    # Validation
    src/validation/message_validator.cpp
    
    # Utils
    src/utils/logger.cpp

    # --- NUEVOS MÓDULOS ---
    src/llm/ollama_client.cpp
    src/rag/vector_store.cpp
    src/rag/rag_service.cpp
)

# =========================
# 4. Includes
# =========================
target_include_directories(whatsapp_core PRIVATE
    include
    ${MARIADB_INCLUDE_DIRS}
)

# =========================
# 5. Linking
# =========================
target_link_libraries(whatsapp_core PRIVATE
    ${MARIADB_LIBRARIES}
    spdlog::spdlog
    faiss
    cpr::cpr
    nlohmann_json::nlohmann_json
    openblas
    OpenMP::OpenMP_CXX  # Es buena práctica enlazarlo explícitamente también
)