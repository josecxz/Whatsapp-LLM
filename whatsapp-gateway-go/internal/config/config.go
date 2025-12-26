package config

import (
	"os"
)

// Usamos 'var' en lugar de 'const' para poder modificar los valores
// en tiempo de ejecución (mediante variables de entorno).
var (
	BackendURL string
	StoreDSN   string
)

// Init lee las variables de entorno o usa valores por defecto.
// Esta función es llamada desde el main.go al arrancar.
func Init() {
	// 1. Configuración del Backend C++
	// Busca la variable de entorno BACKEND_URL, si no existe usa localhost:8080
	BackendURL = getEnv("BACKEND_URL", "http://localhost:8080/ingest")

	// 2. Configuración de la Base de Datos SQLite (Sesión de WhatsApp)
	// Cambiamos la ruta por defecto a la raíz ("whatsapp.db") para evitar
	// el error de "directorio no encontrado" si la carpeta 'store/' no existe.
	dbPath := getEnv("WHATSAPP_DB_PATH", "whatsapp.db")

	// Construimos el DSN (Data Source Name) para el driver SQLite
	StoreDSN = "file:" + dbPath + "?_foreign_keys=on"
}

// getEnv es un helper para leer variables de entorno con un valor de respaldo (fallback)
func getEnv(key, fallback string) string {
	if value, exists := os.LookupEnv(key); exists {
		return value
	}
	return fallback
}
