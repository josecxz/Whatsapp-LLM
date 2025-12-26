#include "ingest/ingest_controller.h"
#include "rag/rag_service.h"
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

using json = nlohmann::json;

// ACTUALIZADO: Inicializamos tanto el servicio RAG como la DB
IngestController::IngestController(std::shared_ptr<RagService> rag_service, std::shared_ptr<Repository> db)
    : m_rag_service(rag_service), m_db(db) {}

void IngestController::RegisterRoutes(httplib::Server& server) {
    
    // ==========================================
    // RUTA 1: INGESTA (Recibir mensajes de WhatsApp)
    // ==========================================
    server.Post("/ingest", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto j = json::parse(req.body);
            
            // Extraer datos básicos
            std::string id = j.value("id", "");
            std::string content = j.value("content", "");
            std::string sender = j.value("sender", "Unknown");

            // Validar
            if (id.empty()) {
                res.status = 400;
                res.set_content("Missing ID", "text/plain");
                return;
            }

            // --- PASO 1: GUARDAR EN BASE DE DATOS SQL (MariaDB) ---
            // Esto asegura que el mensaje persista en disco duro y pueda ser consultado
            
            // A. Asegurar que el chat existe
            m_db->upsert_chat(j);
            
            // B. Insertar el mensaje
            m_db->insert_message(j);
            
            spdlog::info("💾 Mensaje guardado en MariaDB: {}", id);
            // ------------------------------------------------------

            // --- PASO 2: INDEXAR EN IA (RAG) ---
            // Esto guarda el vector en FAISS para búsquedas semánticas
            m_rag_service->IngestMessage(id, content, sender);

            res.set_content("Ack", "text/plain");
            
        } catch (const std::exception& e) {
            spdlog::error("Error procesando ingest: {}", e.what());
            res.status = 400;
            res.set_content("Invalid JSON", "text/plain");
        }
    });

    // ==========================================
    // RUTA 2: CHAT (Preguntar a la IA)
    // ==========================================
    server.Post("/chat", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto j = json::parse(req.body);
            std::string query = j.value("query", "");

            if (query.empty()) {
                res.status = 400;
                res.set_content("Empty query", "text/plain");
                return;
            }

            // Preguntar al servicio RAG
            std::string answer = m_rag_service->Ask(query);

            json response_json = {
                {"status", "success"},
                {"answer", answer}
            };
            res.set_content(response_json.dump(), "application/json");

        } catch (const std::exception& e) {
            spdlog::error("Error en chat endpoint: {}", e.what());
            res.status = 500;
            res.set_content("Internal Server Error", "text/plain");
        }
    });
}