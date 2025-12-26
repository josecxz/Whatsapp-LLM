#pragma once
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

// Estructura simple para mover datos de DB a RAG
struct DBMessage {
    std::string id;
    std::string sender;
    std::string content;
};
// Interfaz Abstracta
class Repository {
public:
    virtual ~Repository() = default;

    // Métodos existentes (adaptados para no pedir MYSQL* en cada llamada, 
    // ya que la conexión será interna de la clase)
    virtual void upsert_chat(const nlohmann::json& msg) = 0;
    virtual void insert_message(const nlohmann::json& msg) = 0;

    // EL NUEVO MÉTODO QUE NECESITAS PARA RAG
    virtual std::string GetMessageContentById(const std::string& id) = 0;
    virtual std::vector<DBMessage> GetAllMessages(int limit = 100) = 0;
};