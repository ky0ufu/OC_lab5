#pragma once
#include "sqlite_db.hpp"
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// Точка измерения
struct DbPoint {
    std::int64_t ts;
    double value;
};

// Статистика
struct DbStats {
    long long count = 0;
    double min = 0, max = 0, avg = 0;
};

// Слой доступа к бд
class SqliteRepo {
public:
    explicit SqliteRepo(SqliteDb& db);

    void init_schema();
    
    // Вставка данных
    void insert_raw(std::int64_t ts, double v);
    void insert_hourly(std::int64_t ts, double v);
    void insert_daily(std::int64_t ts, double v);

    // Последняя запись бд
    std::optional<DbPoint> latest_raw();

    // Статистика по таблице <kind> за период <from> .. <to>
    DbStats stats(const std::string& kind, std::int64_t from, std::int64_t to);
    std::vector<DbPoint> series(const std::string& kind, std::int64_t from, std::int64_t to, int limit);

    void retention(const std::string& kind, std::int64_t keep_from);

private:
    SqliteDb& m_db;

    const char* table_of_kind(const std::string& kind) const;
    void insert_any(const char* table, std::int64_t ts, double v);
};
