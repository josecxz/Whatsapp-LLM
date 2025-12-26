package transport

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"time"
)

// Definimos un cliente global con Timeout para evitar fugas de gorutinas
var client = &http.Client{
	Timeout: 5 * time.Second,
}

func SendToBackend(payload map[string]interface{}, url string) error {
	// 1. Serializar JSON
	data, err := json.Marshal(payload)
	if err != nil {
		return fmt.Errorf("error marshaling json: %w", err)
	}
	// --- DEBUG: Agrega esto ---
	fmt.Printf("\n--- 🔍 PAYLOAD QUE GO ESTÁ ENVIANDO ---\n%s\n---------------------------------------\n", string(data))
	// -
	// 2. Crear la Petición (Request)
	// Usamos NewRequest en lugar de Post para tener más control si necesitamos headers extra
	req, err := http.NewRequest("POST", url, bytes.NewBuffer(data))
	if err != nil {
		return fmt.Errorf("error creating request: %w", err)
	}
	req.Header.Set("Content-Type", "application/json")

	// 3. Ejecutar la Petición
	resp, err := client.Do(req)
	if err != nil {
		return fmt.Errorf("error sending request to C++ backend: %w", err)
	}
	// Siempre cerramos el body, incluso si hay error de status
	defer resp.Body.Close()

	// 4. Manejo de Errores HTTP
	if resp.StatusCode >= 300 {
		// Leemos el cuerpo de la respuesta para saber qué dijo el backend (útil para debug)
		bodyBytes, _ := io.ReadAll(resp.Body)
		errorMsg := string(bodyBytes)

		if len(errorMsg) > 200 {
			// Truncamos si es muy largo para no ensuciar logs
			errorMsg = errorMsg[:200] + "..."
		}

		return fmt.Errorf("backend returned status %d: %s", resp.StatusCode, errorMsg)
	}

	return nil
}
