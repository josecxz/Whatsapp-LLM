#include "llm/ollama_client.h"
#include <cpr/cpr.h>
#include <iostream>
#include <spdlog/spdlog.h>

using json = nlohmann::json;

// Constructor: Recibe la URL (Host) y el nombre del modelo de Chat (Qwen)
OllamaClient::OllamaClient(const std::string& host, const std::string& model)
    : m_host(host), m_model(model) {}

std::vector<float> OllamaClient::GetEmbedding(const std::string& text) {
    // ⚠️ IMPORTANTE: Aquí mantenemos HARDCODED "nomic-embed-text".
    // No usamos m_model porque Qwen no es bueno haciendo embeddings, 
    // y necesitamos compatibilidad exacta (768 dimensiones) con tu base de datos FAISS.
    json payload = {
        {"model", "nomic-embed-text"},
        {"prompt", text}
    };

    try {
        auto response = cpr::Post(
            cpr::Url{m_host + "/api/embeddings"}, // Usa la IP dinámica (WSL2)
            cpr::Body{payload.dump()},
            cpr::Header{{"Content-Type", "application/json"}},
            cpr::Timeout{10000} // 10s es suficiente para Nomic en GPU (antes 60s)
        );

        if (response.status_code == 200) {
            auto j = json::parse(response.text);
            return j["embedding"].get<std::vector<float>>();
        } else {
            spdlog::error("❌ Ollama Embedding Error {}: {}", response.status_code, response.text);
        }
    } catch (const std::exception& e) {
        spdlog::error("🔥 Ollama Connection Exception (Embedding): {}", e.what());
    }
    return {};
}

std::optional<std::string> OllamaClient::Chat(const std::string& system_prompt, const std::string& user_query) {
    json payload = {
        {"model", m_model}, // Aquí sí usamos Qwen 2.5
        {"stream", false},
        {"messages", {
            {{"role", "system"}, {"content", system_prompt}},
            {{"role", "user"}, {"content", user_query}}
        }},
        // ⚙️ OPCIONES DE RENDIMIENTO PARA QWEN EN GPU
        {"options", {
            {"num_ctx", 8192},    // Aumentamos memoria de contexto (tienes 8GB VRAM, úsalos)
            {"temperature", 0.3}, // Baja creatividad para ser fiel a los datos (RAG)
            {"top_k", 40},
            {"top_p", 0.9}
        }}
    };

    try {
        auto response = cpr::Post(
            cpr::Url{m_host + "/api/chat"},
            cpr::Body{payload.dump()},
            cpr::Header{{"Content-Type", "application/json"}},
            // Aunque la GPU es rápida, dejamos margen por si procesa mucho texto
            cpr::Timeout{60000} 
        );

        if (response.status_code == 200) {
            auto j = json::parse(response.text);
            return j["message"]["content"].get<std::string>();
        } else {
            // Añadido log de error que faltaba aquí
            spdlog::error("❌ Ollama Chat Error {}: {}", response.status_code, response.text);
        }
    } catch (const std::exception& e) {
        spdlog::error("🔥 Ollama Chat Exception: {}", e.what());
    }
    return std::nullopt;
}