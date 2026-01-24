#pragma once
#include <string>
#include <vector>
#include <optional>
#include <nlohmann/json.hpp>

class OllamaClient {
public:
    OllamaClient(const std::string& base_url, const std::string& chat_model);    
    // Convierte texto a vector (Embedding)
    std::vector<float> GetEmbedding(const std::string& text);
    
    // Genera respuesta chat (Contexto + Pregunta)
    std::optional<std::string> Chat(const std::string& system_prompt, const std::string& user_query);

private:
    std::string m_host;
    std::string m_model;
};