#pragma once
#include <string>
#include <stdexcept>
#include <sqlite3.h>

class SqliteDb {
public:
    explicit SqliteDb(const std::string& path);
    ~SqliteDb();

    SqliteDb(const SqliteDb&) = delete;
    SqliteDb& operator=(const SqliteDb&) = delete;

    sqlite3* handle() const { return m_db; }

    void exec(const std::string& sql);

private:
    sqlite3* m_db = nullptr;
};
