// repository.cpp
// Asegúrate de que este archivo tiene extensión .cpp
#include "persistence/repository.h"

// INCLUDES CRÍTICOS
// Dependiendo de tu instalación, puede ser <mariadb/mysql.h> o <mysql/mysql.h>
#include <mariadb/mysql.h> 
#include <nlohmann/json.hpp>

#include <sstream>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <iostream> 

// Función auxiliar para convertir Unix Timestamp (int64) a String MySQL
std::string unix_to_mysql_time(int64_t unix_ts) {
    std::time_t temp = unix_ts;
    std::tm* t = std::gmtime(&temp); 
    std::stringstream ss;
    ss << std::put_time(t, "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

void upsert_chat(MYSQL* conn, const nlohmann::json& msg) {
    const char* sql =
        "INSERT INTO chats (jid, name, last_message_time) "
        "VALUES (?, ?, ?) "
        "ON DUPLICATE KEY UPDATE "
        "name=VALUES(name), last_message_time=VALUES(last_message_time)";

    MYSQL_STMT* stmt = mysql_stmt_init(conn);
    if (!stmt) return;

    if (mysql_stmt_prepare(stmt, sql, strlen(sql)) != 0) {
        std::cerr << "MySQL Prepare Error: " << mysql_stmt_error(stmt) << std::endl;
        mysql_stmt_close(stmt); // Cerrar siempre antes de salir
        return;
    }

    std::string jid = msg.value("chat_jid", "");
    std::string name = msg.value("chat_name", "");
    
    std::string ts_str;
    if (msg.contains("timestamp") && msg["timestamp"].is_number()) {
        ts_str = unix_to_mysql_time(msg["timestamp"].get<int64_t>());
    } else {
        ts_str = msg.value("timestamp", "1970-01-01 00:00:00");
    }

    MYSQL_BIND bind[3];
    memset(bind, 0, sizeof(bind));

    // CORRECCIÓN: Casteo explícito a (char*) es más seguro que (void*) en algunos contextos de C++
    // aunque (void*) es estándar para mysql_bind.
    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = (char*)jid.c_str(); 
    bind[0].buffer_length = jid.size();

    bind[1].buffer_type = MYSQL_TYPE_STRING;
    bind[1].buffer = (char*)name.c_str();
    bind[1].buffer_length = name.size();

    bind[2].buffer_type = MYSQL_TYPE_STRING;
    bind[2].buffer = (char*)ts_str.c_str();
    bind[2].buffer_length = ts_str.size();

    if (mysql_stmt_bind_param(stmt, bind) != 0) {
        std::cerr << "MySQL Bind Error: " << mysql_stmt_error(stmt) << std::endl;
    } else {
        if (mysql_stmt_execute(stmt) != 0) {
            std::cerr << "MySQL Execute Error: " << mysql_stmt_error(stmt) << std::endl;
        }
    }
    
    mysql_stmt_close(stmt);
}

void insert_message(MYSQL* conn, const nlohmann::json& msg) {
    const char* sql =
        "INSERT INTO messages "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)"; // Simplificado para brevedad, usa tu query completa

    MYSQL_STMT* stmt = mysql_stmt_init(conn);
    if (!stmt) return; 

    if (mysql_stmt_prepare(stmt, sql, strlen(sql)) != 0) {
        std::cerr << "Msg Prepare Error: " << mysql_stmt_error(stmt) << std::endl;
        mysql_stmt_close(stmt);
        return;
    }

    std::string id      = msg.value("id", "");
    std::string chat    = msg.value("chat_jid", "");
    std::string sender  = msg.value("sender", "");
    std::string content = msg.value("content", "");
    
    std::string ts_str;
    if (msg.contains("timestamp") && msg["timestamp"].is_number()) {
        ts_str = unix_to_mysql_time(msg["timestamp"].get<int64_t>());
    } else {
        ts_str = msg.value("timestamp", "1970-01-01 00:00:00");
    }

    char from_me_val = msg.value("is_from_me", false) ? 1 : 0;

    MYSQL_BIND bind[9];
    memset(bind, 0, sizeof(bind));

    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = (char*)id.c_str();
    bind[0].buffer_length = id.length();

    bind[1].buffer_type = MYSQL_TYPE_STRING;
    bind[1].buffer = (char*)chat.c_str();
    bind[1].buffer_length = chat.length();

    bind[2].buffer_type = MYSQL_TYPE_STRING;
    bind[2].buffer = (char*)sender.c_str();
    bind[2].buffer_length = sender.length();

    bind[3].buffer_type = MYSQL_TYPE_STRING;
    bind[3].buffer = (char*)content.c_str();
    bind[3].buffer_length = content.length();

    bind[4].buffer_type = MYSQL_TYPE_STRING;
    bind[4].buffer = (char*)ts_str.c_str();
    bind[4].buffer_length = ts_str.size();

    bind[5].buffer_type = MYSQL_TYPE_TINY;
    bind[5].buffer = &from_me_val;

    // Para NULLs
    bind[6].buffer_type = MYSQL_TYPE_NULL;
    bind[7].buffer_type = MYSQL_TYPE_NULL;
    bind[8].buffer_type = MYSQL_TYPE_NULL;

    if (mysql_stmt_bind_param(stmt, bind) != 0) {
        std::cerr << "Msg Bind Error: " << mysql_stmt_error(stmt) << std::endl;
    } else {
        if (mysql_stmt_execute(stmt) != 0) {
            std::cerr << "Msg Execute Error: " << mysql_stmt_error(stmt) << std::endl;
        }
    }

    mysql_stmt_close(stmt);
}