#include "http.hpp"
#include "../third_party/httplib.h"
#include <sstream>

// endpoints:
//
// GET /api/current
//   возвращает последнюю температуру (latest_raw)
//
// GET /api/stats?kind=raw|hourly|daily&from=UNIX&to=UNIX
//   возвращает count/min/max/avg
//
// GET /api/series?kind=...&from=...&to=...&limit=1000
//   возвращает список точек [{ts,value},...]
void HttpSimple::run(const std::string& host, int port) {
    httplib::Server svr;

    // Текущая температура
    svr.Get("/api/current", [&](const httplib::Request&, httplib::Response& res) {
        auto p = m_repo.latest_raw();
        if (!p) { res.set_content("{\"ok\":false}", "application/json"); return; }
        std::ostringstream oss;
        oss << "{\"ok\":true,\"ts\":" << p->ts << ",\"value\":" << p->value << "}";
        res.set_content(oss.str(), "application/json");
    });

    // Статистика
    svr.Get("/api/stats", [&](const httplib::Request& req, httplib::Response& res) {
        if (!req.has_param("kind") || !req.has_param("from") || !req.has_param("to")) {
            res.status = 400;
            res.set_content("{\"ok\":false,\"err\":\"missing params\"}", "application/json");
            return;
        }
        try {
            std::string kind = req.get_param_value("kind");
            auto from = std::stoll(req.get_param_value("from"));
            auto to   = std::stoll(req.get_param_value("to"));
            auto s = m_repo.stats(kind, from, to);

            std::ostringstream oss;
            oss << "{\"ok\":true,\"count\":" << s.count
                << ",\"min\":" << s.min << ",\"max\":" << s.max << ",\"avg\":" << s.avg << "}";
            res.set_content(oss.str(), "application/json");
        } catch (...) {
            res.status = 400;
            res.set_content("{\"ok\":false,\"err\":\"bad request\"}", "application/json");
        }
    });


    svr.Get("/api/series", [&](const httplib::Request& req, httplib::Response& res) {
        if (!req.has_param("kind") || !req.has_param("from") || !req.has_param("to")) {
            res.status = 400;
            res.set_content("{\"ok\":false,\"err\":\"missing params\"}", "application/json");
            return;
        }
        try {
            std::string kind = req.get_param_value("kind");
            auto from = std::stoll(req.get_param_value("from"));
            auto to   = std::stoll(req.get_param_value("to"));
            int limit = req.has_param("limit") ? std::stoi(req.get_param_value("limit")) : 1000;

            auto pts = m_repo.series(kind, from, to, limit);
            std::ostringstream oss;
            oss << "{\"ok\":true,\"points\":[";
            for (size_t i = 0; i < pts.size(); ++i) {
                if (i) oss << ",";
                oss << "{\"ts\":" << pts[i].ts << ",\"value\":" << pts[i].value << "}";
            }
            oss << "]}";
            res.set_content(oss.str(), "application/json");
        } catch (...) {
            res.status = 400;
            res.set_content("{\"ok\":false,\"err\":\"bad request\"}", "application/json");
        }
    });

    std::cerr << "HTTP listening on http://" << host << ":" << port << "\n";
    svr.listen(host.c_str(), port);
}
