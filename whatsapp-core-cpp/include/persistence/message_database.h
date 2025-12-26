#pragma once
#include "persistence/repository.h"
#include <mariadb/mysql.h>
#include <string>

// --- AÑADE ESTA LÍNEA AQUÍ ---
// Esto le dice al compilador: "Existe una función llamada db_connect, búscala al enlazar"
MYSQL* db_connect(); 

class MessageDatabase : public Repository {
public:
    MessageDatabase(MYSQL* conn);

    void upsert_chat(const nlohmann::json& msg) override;
    void insert_message(const nlohmann::json& msg) override;
    std::string GetMessageContentById(const std::string& id) override;
    std::vector<DBMessage> GetAllMessages(int limit) override;
private:
    MYSQL* m_conn;
};