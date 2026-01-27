import os
from datetime import datetime, timezone

from flask import Flask, render_template, request, Response
import requests
from dotenv import load_dotenv

load_dotenv()

app = Flask(__name__)

TEMP_SERVER_BASE = os.getenv("TEMP_SERVER_BASE", "http://127.0.0.1:8080").rstrip("/")

TIMEOUT = float(os.getenv("TEMP_CLIENT_TIMEOUT", "3.0"))


@app.get("/")
def index():
    return render_template("index.html", server_base=TEMP_SERVER_BASE)


def _proxy_get(path: str, params: dict | None = None) -> Response:
    """
    Проксируем GET запрос к temp_server
    path
    params: query параметры (kind/from/to/limit)
    """
    url = TEMP_SERVER_BASE + path
    if params is None:
        params = dict(request.args)

    try:
        r = requests.get(url, params=params, timeout=TIMEOUT)
        return Response(
            r.content,
            status=r.status_code,
            content_type=r.headers.get("Content-Type", "application/json"),
        )
    except requests.RequestException as e:
        return Response(
            f'{{"ok":false,"err":"proxy_error","details":"{str(e)}"}}'.encode("utf-8"),
            status=502,
            content_type="application/json",
        )


@app.get("/api/current")
def api_current():
    return _proxy_get("/api/current")


@app.get("/api/stats")
def api_stats():
    return _proxy_get("/api/stats")


@app.get("/api/series")
def api_series():
    return _proxy_get("/api/series")


@app.get("/api/series_all")
def api_series_all():
    kind = request.args.get("kind", "hourly")
    limit = int(request.args.get("limit", "20000"))

    now = int(datetime.now(tz=timezone.utc).timestamp())

    if kind == "hourly":
        from_ts = now - 60 * 24 * 3600
    elif kind == "daily":
        y = datetime.now(tz=timezone.utc).year
        from_ts = int(datetime(y, 1, 1, tzinfo=timezone.utc).timestamp())
    elif kind == "raw":
        from_ts = now - 24 * 3600
    else:
        return Response(b'{"ok":false,"err":"bad kind"}', status=400, content_type="application/json")

    params = {
        "kind": kind,
        "from": str(from_ts),
        "to": str(now),
        "limit": str(limit),
    }
    return _proxy_get("/api/series", params=params)


@app.get("/health")
def health():
    return {"ok": True, "temp_server": TEMP_SERVER_BASE}


if __name__ == "__main__":
    host = os.getenv("FLASK_HOST", "127.0.0.1")
    port = int(os.getenv("FLASK_PORT", "5000"))
    debug = os.getenv("FLASK_DEBUG", "0") == "1"
    app.run(host=host, port=port, debug=debug)
