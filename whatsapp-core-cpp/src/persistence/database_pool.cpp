#include "persistence/database_pool.h"
#include "persistence/message_database.h"

static std::queue<MYSQL*> pool;
static std::mutex mtx;

MYSQL* DBPool::acquire() {
    std::lock_guard<std::mutex> lock(mtx);
    if (pool.empty())
        return db_connect();
    auto c = pool.front();
    pool.pop();
    return c;
}

void DBPool::release(MYSQL* conn) {
    std::lock_guard<std::mutex> lock(mtx);
    pool.push(conn);
}
