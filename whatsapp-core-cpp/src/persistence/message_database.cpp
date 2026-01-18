#include "persistence/message_database.h"
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <iostream>

// ==========================================
// 1. Función Helper para conectar (Tu código original)
// ==========================================
MYSQL* db_connect() {
    MYSQL* conn = mysql_init(nullptr);

    if (!mysql_real_connect(
            conn,
            "mariadb-service",
            "qwenuser",
            "mypassword",
            "STRIX_MAIN",
            0, nullptr, 0)) {
        throw std::runtime_error(mysql_error(conn));
    }

    mysql_set_character_set(conn, "utf8mb4");
    spdlog::info("✅ Conexión a MariaDB establecida correctamente.");
    return conn;
}

// ==========================================
// 2. Implementación de la Clase MessageDatabase
// ==========================================

// Constructor: Recibe la conexión y la guarda en m_conn
MessageDatabase::MessageDatabase(MYSQL* conn) : m_conn(conn) {
    if (m_conn == nullptr) {
        spdlog::critical("MessageDatabase inicializada con conexión NULL");
    }
}

void MessageDatabase::upsert_chat(const nlohmann::json& msg) {
    if (!m_conn) return;

    // Extraemos datos básicos con seguridad (evita crash si faltan campos)
    std::string jid = msg.value("chat_jid", "");
    std::string name = msg.value("chat_name", "Desconocido");

    if (jid.empty()) return;

    // Query simple (Nota: En producción usa prepared statements para evitar SQL Injection)
    std::string query = "INSERT INTO chats (jid, name) VALUES ('" + jid + "', '" + name + "') "
                        "ON DUPLICATE KEY UPDATE name = VALUES(name)";

    if (mysql_query(m_conn, query.c_str())) {
        spdlog::error("Error upsert_chat: {}", mysql_error(m_conn));
    }
}

void MessageDatabase::insert_message(const nlohmann::json& msg) {
    if (!m_conn) return;

    // Extraer campos del JSON
    std::string id = msg.value("id", "");
    std::string chat_jid = msg.value("chat_jid", "");
    std::string sender_jid = msg.value("sender_jid", "");
    std::string sender_name = msg.value("sender", "");
    std::string content = msg.value("content", "");
    long long timestamp = msg.value("timestamp", 0);
    bool is_from_me = msg.value("is_from_me", false);

    if (id.empty()) return;

    // Escapar el contenido para evitar errores SQL con comillas simples ' o caracteres raros
    char* escaped_content = new char[content.length() * 2 + 1];
    mysql_real_escape_string(m_conn, escaped_content, content.c_str(), content.length());

    std::string query = "INSERT IGNORE INTO messages (id, chat_jid, sender, content, timestamp, is_from_me) "
                        "VALUES ('" + id + "', '" + chat_jid + "', '" + sender_name + "', '" + 
                        std::string(escaped_content) + "', " + std::to_string(timestamp) + ", " + (is_from_me ? "1" : "0") + ")";

    if (mysql_query(m_conn, query.c_str())) {
        spdlog::error("Error insert_message: {}", mysql_error(m_conn));
    } else {
        spdlog::info("💾 Mensaje guardado en DB: {}", id);
    }

    delete[] escaped_content;
}

std::vector<DBMessage> MessageDatabase::GetAllMessages(int limit) {
    std::vector<DBMessage> messages;
    if (!m_conn) return messages;

    // Ordenamos descendente para coger los últimos, y luego invertimos o procesamos
    // Aquí cogemos los últimos 'limit' mensajes
    std::string query = "SELECT id, sender, content FROM messages ORDER BY timestamp DESC LIMIT " + std::to_string(limit);
 
    if (mysql_query(m_conn, query.c_str())) {
        spdlog::error("Error cargando historial: {}", mysql_error(m_conn));
        return messages;
    }

    MYSQL_RES* result = mysql_store_result(m_conn);
    if (!result) return messages;

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result))) {
        DBMessage msg;
        msg.id = row[0] ? row[0] : "";
        msg.sender = row[1] ? row[1] : "Unknown";
        msg.content = row[2] ? row[2] : "";
        
        // Solo añadimos si tiene contenido válido
        if (!msg.content.empty() && !msg.id.empty()) {
            messages.push_back(msg);
        }
    }
    mysql_free_result(result);
    
    spdlog::info("📂 Recuperados {} mensajes antiguos de la base de datos.", messages.size());
    return messages;
}

// NUEVO MÉTODO PARA RAG
std::string MessageDatabase::GetMessageContentById(const std::string& id) {
    if (!m_conn) return "";

    // Query para obtener solo el contenido
    std::string query = "SELECT sender, content FROM messages WHERE id = '" + id + "' LIMIT 1";

    if (mysql_query(m_conn, query.c_str())) {
        spdlog::error("Error buscando mensaje por ID: {}", mysql_error(m_conn));
        return "";
    }

    MYSQL_RES* result = mysql_store_result(m_conn);
    if (!result) return "";

    std::string full_text = "";
    if (MYSQL_ROW row = mysql_fetch_row(result)) {
        std::string sender = row[0] ? row[0] : "Desconocido";
        std::string content = row[1] ? row[1] : "";
        
        // Formateamos: "Juan: Hola que tal"
        full_text = sender + ": " + content;
    }

    mysql_free_result(result);
    return full_text;
}