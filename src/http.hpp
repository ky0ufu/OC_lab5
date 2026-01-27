#pragma once
#include "sqlite_repo.hpp"
#include <string>

class HttpSimple {
public:
    explicit HttpSimple(SqliteRepo& repo) : m_repo(repo) {}
    void run(const std::string& host, int port);

private:
    SqliteRepo& m_repo;
};
