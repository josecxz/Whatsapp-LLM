#pragma once
#include <mariadb/mysql.h>
#include <queue>
#include <mutex>

class DBPool {
public:
    static MYSQL* acquire();
    static void release(MYSQL* conn);
};
