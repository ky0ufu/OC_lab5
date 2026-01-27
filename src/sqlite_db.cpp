#include "sqlite_db.hpp"

// Открываем бд
SqliteDb::SqliteDb(const std::string& path) {
    if (sqlite3_open(path.c_str(), &m_db) != SQLITE_OK) {
        throw std::runtime_error("sqlite3_open failed");
    }
    
    exec("PRAGMA journal_mode=WAL;");
    exec("PRAGMA synchronous=NORMAL;");
}

// Закрываем бд
SqliteDb::~SqliteDb() {
    if (m_db) sqlite3_close(m_db);
}

void SqliteDb::exec(const std::string& sql) {
    char* err = nullptr;
    if (sqlite3_exec(m_db, sql.c_str(), nullptr, nullptr, &err) != SQLITE_OK) {
        std::string msg = err ? err : "sqlite exec failed";
        sqlite3_free(err);
        throw std::runtime_error(msg);
    }
}
