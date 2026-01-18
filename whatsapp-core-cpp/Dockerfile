# USAMOS UBUNTU 24.04 (Noble)
FROM ubuntu:24.04 AS builder

# Evitar preguntas interactivas
ENV DEBIAN_FRONTEND=noninteractive

# 1. Instalar dependencias del sistema
# AÑADIDO: libcurl4-openssl-dev (Necesario para CPR)
RUN apt-get update && apt-get install -y \
    build-essential cmake git \
    libmariadb-dev libssl-dev \
    pkg-config \
    libspdlog-dev libfmt-dev nlohmann-json3-dev \
    libopenblas-dev liblapack-dev \
    libcurl4-openssl-dev \
    && rm -rf /var/lib/apt/lists/*

# 2. Copiar todo el código fuente
WORKDIR /app
COPY . .

# ==========================================
# 3. INSTALAR DEPENDENCIAS VENDOR
# ==========================================

# A) Compilar e Instalar FAISS
WORKDIR /app/vendor/faiss_src
RUN rm -rf build && \
    cmake -B build . \
        -DFAISS_ENABLE_GPU=OFF \
        -DFAISS_ENABLE_PYTHON=OFF \
        -DBUILD_TESTING=OFF \
        -DFAISS_OPT_LEVEL=generic \
        -DCMAKE_BUILD_TYPE=Release && \
    cmake --build build --target install -j2

# B) Compilar e Instalar CPR
WORKDIR /app/vendor/cpr_src
# Ahora sí funcionará porque instalamos libcurl4-openssl-dev arriba
RUN rm -rf build && \
    cmake -B build . \
        -DCPR_USE_SYSTEM_CURL=ON \
        -DBUILD_TESTING=OFF && \
    cmake --build build --target install -j2

# ==========================================
# 4. COMPILAR TU PROYECTO PRINCIPAL
# ==========================================
WORKDIR /app/build
# Limpiamos y compilamos
RUN rm -rf * && cmake .. && make -j2

# --- IMAGEN FINAL DE EJECUCIÓN ---
FROM ubuntu:24.04

# Instalar librerías runtime
# AÑADIDO: libcurl4 (Para que funcione al ejecutar)
RUN apt-get update && apt-get install -y \
libmariadb3 openssl ca-certificates \
    libspdlog1.12 libopenblas0 \
    libcurl4 \
    libgomp1 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copiar el ejecutable
COPY --from=builder /app/build/whatsapp_core .

EXPOSE 8080

CMD ["./whatsapp_core"]