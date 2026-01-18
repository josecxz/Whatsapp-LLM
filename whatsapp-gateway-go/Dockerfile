FROM golang:1.24-alpine AS builder

# 1. INSTALAR COMPILADOR DE C (gcc)
# Necesario para que go-sqlite3 pueda compilarse
RUN apk add --no-cache build-base

WORKDIR /app

COPY . .

RUN go mod download

# 2. ACTIVAR CGO
# Le decimos a Go: "Sí, usa librerías de C"
ENV CGO_ENABLED=1

# Compilamos apuntando a tu carpeta correcta
RUN go build -o whatsapp-bot ./cmd/gateway

# --- Etapa Final ---
FROM alpine:latest

WORKDIR /app

# 3. INSTALAR LIBRERÍAS DE RUNTIME
# sqlite-libs y libc6-compat son vitales para ejecutar el binario CGO
RUN apk --no-cache add ca-certificates tzdata libc6-compat libgcc sqlite-libs

COPY --from=builder /app/whatsapp-bot .

CMD ["./whatsapp-bot"]