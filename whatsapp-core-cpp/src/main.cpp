#include <iostream>
#include <memory>
#include <cstdlib> // Necesario para std::getenv
#include <spdlog/spdlog.h>

// Librerías del Servidor
#include "cpp-httplib/httplib.h"

// Componentes del Sistema
#include "utils/logger.h"
#include "ingest/ingest_controller.h"

// Componentes RAG y Persistencia
#include "persistence/message_database.h"
#include "llm/ollama_client.h"
#include "rag/vector_store.h"
#include "rag/rag_service.h"

// Declaración externa de la función de conexión
MYSQL* db_connect(); 

int main() {
    // 1. Inicializar Logger
    init_logger();
    spdlog::info("🚀 WhatsApp Core Backend iniciando...");

    try {
        // ==========================================
        // 2. INICIALIZACIÓN DE CAPA DE DATOS
        // ==========================================
        
        // A. Conexión a MariaDB
        spdlog::info("🔌 Conectando a Base de Datos...");
        MYSQL* conn = db_connect();
        
        // B. Crear Repositorio (MessageDatabase)
        auto db = std::make_shared<MessageDatabase>(conn);

        // ==========================================
        // 3. INICIALIZACIÓN DE CAPA DE IA (RAG)
        // ==========================================
        
        // A. Configuración Dinámica de Ollama (Para conectar con WSL2/GPU)
        const char* env_host = std::getenv("OLLAMA_HOST");
        std::string ollama_url = env_host ? env_host : "http://localhost:11434"; // Fallback por defecto

        spdlog::info("🔌 Conectando a Cerebro IA (Ollama) en: {}", ollama_url);

        // B. Cliente Ollama
        // Usamos "qwen2.5:7b" como modelo principal de Chat (tu GPU lo moverá rápido)
        // Nota: El modelo de embeddings ("nomic-embed-text") está hardcoded dentro de OllamaClient.cpp
        auto ollama = std::make_shared<OllamaClient>(ollama_url, "qwen2.5:7b");
        
        // C. Almacén Vectorial (FAISS)
        // Usamos dimensión 768 porque es lo que genera "nomic-embed-text"
        auto vector_store = std::make_shared<VectorStore>(768); 
        
        // D. Servicio RAG (El orquestador)
        auto rag_service = std::make_shared<RagService>(ollama, vector_store, db);
        spdlog::info("🧠 Servicio RAG inicializado correctamente");

        // ==========================================
        // 🆕 CARGAR MEMORIA DEL PASADO
        // ==========================================
        // Esto leerá los mensajes antiguos de la DB, generará embeddings con Nomic
        // y los indexará en FAISS RAM.
        rag_service->LoadHistoryFromDB();


        // ==========================================
        // 4. SERVIDOR HTTP
        // ==========================================
        httplib::Server server;

        // Instanciamos el controlador pasando:
        // 1. El cerebro (rag_service) para embedding/chat
        // 2. La memoria (db) para guardar mensajes nuevos
        IngestController controller(rag_service, db);
        
        // Registramos las rutas definidas en el controlador
        controller.RegisterRoutes(server);

        // ==========================================
        // 5. ARRANQUE
        // ==========================================
        std::cout << "\n✅ Core backend listening on port 8080\n";
        std::cout << "   - /ingest (POST): Recibe mensajes de WhatsApp\n";
        std::cout << "   - /chat   (POST): Responde preguntas con RAG (Qwen 7B)\n\n";
        
        // Escuchar en todas las interfaces
        server.listen("0.0.0.0", 8080);

    } catch (const std::exception& e) {
        spdlog::critical("🔥 Error fatal en el inicio: {}", e.what());
        return 1;
    }

    return 0;
}