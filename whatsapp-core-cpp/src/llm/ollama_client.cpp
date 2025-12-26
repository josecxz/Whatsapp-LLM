#include "llm/ollama_client.h"
#include <cpr/cpr.h>
#include <iostream>
#include <spdlog/spdlog.h>

using json = nlohmann::json;

OllamaClient::OllamaClient(const std::string& host, const std::string& model)
    : m_host(host), m_model(model) {}

std::vector<float> OllamaClient::GetEmbedding(const std::string& text) {
    json payload = {
        {"model", m_model},
        {"prompt", text}
    };

    try {
        auto response = cpr::Post(
            cpr::Url{m_host + "/api/embeddings"},
            cpr::Body{payload.dump()},
            cpr::Header{{"Content-Type", "application/json"}},
            cpr::Timeout{5000} // 5s timeout
        );

        if (response.status_code == 200) {
            auto j = json::parse(response.text);
            return j["embedding"].get<std::vector<float>>();
        } else {
            spdlog::error("Ollama Embedding Error {}: {}", response.status_code, response.text);
        }
    } catch (const std::exception& e) {
        spdlog::error("Ollama Connection Exception: {}", e.what());
    }
    return {};
}

std::optional<std::string> OllamaClient::Chat(const std::string& system_prompt, const std::string& user_query) {
    json payload = {
        {"model", m_model},
        {"stream", false},
        {"messages", {
            {{"role", "system"}, {"content", system_prompt}},
            {{"role", "user"}, {"content", user_query}}
        }}
    };

    try {
        auto response = cpr::Post(
            cpr::Url{m_host + "/api/chat"},
            cpr::Body{payload.dump()},
            cpr::Header{{"Content-Type", "application/json"}},
            cpr::Timeout{30000} // 30s timeout para generación
        );

        if (response.status_code == 200) {
            auto j = json::parse(response.text);
            return j["message"]["content"].get<std::string>();
        }
    } catch (const std::exception& e) {
        spdlog::error("Ollama Chat Exception: {}", e.what());
    }
    return std::nullopt;
}