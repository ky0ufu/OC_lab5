#include "sqlite_repo.hpp"
#include <stdexcept>

static void bind_i64(sqlite3_stmt* st, int idx, std::int64_t v) {
    if (sqlite3_bind_int64(st, idx, (sqlite3_int64)v) != SQLITE_OK)
        throw std::runtime_error("sqlite bind int64 failed");
}

static void bind_d(sqlite3_stmt* st, int idx, double v) {
    if (sqlite3_bind_double(st, idx, v) != SQLITE_OK)
        throw std::runtime_error("sqlite bind double failed");
}

SqliteRepo::SqliteRepo(SqliteDb& db) : m_db(db) {}

// По kind возвращаем таблицу
const char* SqliteRepo::table_of_kind(const std::string& kind) const {
    if (kind == "raw") return "raw_measurements";
    if (kind == "hourly") return "hourly_avg";
    if (kind == "daily") return "daily_avg";
    throw std::runtime_error("wrong kind");
}

// Создаём таблицы
void SqliteRepo::init_schema() {
    m_db.exec(
        "CREATE TABLE IF NOT EXISTS raw_measurements(ts INTEGER NOT NULL, value REAL NOT NULL);"
        "CREATE INDEX IF NOT EXISTS idx_raw_ts ON raw_measurements(ts);"
        "CREATE TABLE IF NOT EXISTS hourly_avg(ts INTEGER NOT NULL, value REAL NOT NULL);"
        "CREATE INDEX IF NOT EXISTS idx_hourly_ts ON hourly_avg(ts);"
        "CREATE TABLE IF NOT EXISTS daily_avg(ts INTEGER NOT NULL, value REAL NOT NULL);"
        "CREATE INDEX IF NOT EXISTS idx_daily_ts ON daily_avg(ts);"
    );
}

// вставка в таблицу
void SqliteRepo::insert_any(const char* table, std::int64_t ts, double v) {
    // INSERT INTO <table>(ts,value) VALUES(?,?)
    std::string sql = std::string("INSERT INTO ") + table + "(ts,value) VALUES(?,?)";
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(m_db.handle(), sql.c_str(), -1, &st, nullptr) != SQLITE_OK)
        throw std::runtime_error("sqlite prepare insert failed");

    bind_i64(st, 1, ts);
    bind_d(st, 2, v);

    if (sqlite3_step(st) != SQLITE_DONE) {
        sqlite3_finalize(st);
        throw std::runtime_error("sqlite step insert failed");
    }
    sqlite3_finalize(st);
}

void SqliteRepo::insert_raw(std::int64_t ts, double v)    { insert_any("raw_measurements", ts, v); }
void SqliteRepo::insert_hourly(std::int64_t ts, double v) { insert_any("hourly_avg", ts, v); }
void SqliteRepo::insert_daily(std::int64_t ts, double v)  { insert_any("daily_avg", ts, v); }

std::optional<DbPoint> SqliteRepo::latest_raw() {
    const char* sql = "SELECT ts,value FROM raw_measurements ORDER BY ts DESC LIMIT 1";
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(m_db.handle(), sql, -1, &st, nullptr) != SQLITE_OK)
        throw std::runtime_error("sqlite prepare latest failed");

    int rc = sqlite3_step(st);
    if (rc == SQLITE_ROW) {
        DbPoint p;
        p.ts = (std::int64_t)sqlite3_column_int64(st, 0);
        p.value = sqlite3_column_double(st, 1);
        sqlite3_finalize(st);
        return p;
    }
    sqlite3_finalize(st);
    return std::nullopt;
}

// Статистика за период
DbStats SqliteRepo::stats(const std::string& kind, std::int64_t from, std::int64_t to) {
    const char* table = table_of_kind(kind);

    // SELECT COUNT(*), MIN(value), MAX(value), AVG(value) FROM table WHERE ts>=? AND ts<=?
    std::string sql = std::string("SELECT COUNT(*), MIN(value), MAX(value), AVG(value) FROM ")
                    + table + " WHERE ts>=? AND ts<=?";

    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(m_db.handle(), sql.c_str(), -1, &st, nullptr) != SQLITE_OK)
        throw std::runtime_error("sqlite prepare stats failed");

    bind_i64(st, 1, from);
    bind_i64(st, 2, to);

    DbStats s{};
    if (sqlite3_step(st) == SQLITE_ROW) {
        s.count = (long long)sqlite3_column_int64(st, 0);
        if (s.count > 0) {
            s.min = sqlite3_column_double(st, 1);
            s.max = sqlite3_column_double(st, 2);
            s.avg = sqlite3_column_double(st, 3);
        }
    }
    sqlite3_finalize(st);
    return s;
}

std::vector<DbPoint> SqliteRepo::series(const std::string& kind, std::int64_t from, std::int64_t to, int limit) {
    const char* table = table_of_kind(kind);
    if (limit <= 0) limit = 1000;

    std::string sql = std::string("SELECT ts,value FROM ") + table +
                      " WHERE ts>=? AND ts<=? ORDER BY ts ASC LIMIT ?";

    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(m_db.handle(), sql.c_str(), -1, &st, nullptr) != SQLITE_OK)
        throw std::runtime_error("sqlite prepare series failed");

    bind_i64(st, 1, from);
    bind_i64(st, 2, to);
    if (sqlite3_bind_int(st, 3, limit) != SQLITE_OK)
        throw std::runtime_error("sqlite bind limit failed");

    std::vector<DbPoint> out;
    while (sqlite3_step(st) == SQLITE_ROW) {
        DbPoint p;
        p.ts = (std::int64_t)sqlite3_column_int64(st, 0);
        p.value = sqlite3_column_double(st, 1);
        out.push_back(p);
    }
    sqlite3_finalize(st);
    return out;
}

void SqliteRepo::retention(const std::string& kind, std::int64_t keep_from) {
    const char* table = table_of_kind(kind);
    std::string sql = std::string("DELETE FROM ") + table + " WHERE ts < ?";

    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(m_db.handle(), sql.c_str(), -1, &st, nullptr) != SQLITE_OK)
        throw std::runtime_error("sqlite prepare retention failed");

    bind_i64(st, 1, keep_from);
    sqlite3_step(st);
    sqlite3_finalize(st);
}
