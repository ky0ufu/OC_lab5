### README

Для сборки программы нужно выполнить:     
Windows
```bat
./build_win.bat
```
UNIX
```sh
./build.sh
```

Для запуска симулятора нужно выполнить:     
Windows
```bat
./win_simulator_start.sh
```
! Для создания виртуальных портов (Windows) использовался com0com    \
UNIX
```sh
./simulator_start.sh
```
! Для создания портов использовался ```socat```
```sh
socat -d -d pty,raw,echo=0 pty,raw,echo=0
```

Для запуска с произвольным портом необходимо выполнить скрипты:   
Симулятор
```sh
./build/temp_simulator --out serial --port <port> --baud 9600 --interval <sec>
```
Сервер
```sh
./build/temp_server --db <db_file.db> --source serial --port <port> --https-host <host> --http-port <port>
```
! При изменении хоста и порта сервера необходимо указать для клиента новый хост и порт в `client/.env:TEMP_SERVER_BASE` \
Параметры для изменения времени хранения данных:
`--raw-keep-sec` - время в секундах, сколько будут хранится данные с порта. \
! `--port <port>` - необходимый параметр для сервера и симулятора. \
Например: \
```sh
./build/temp_server --port <port>
./build/temp_simulator --port <port>
```

#### Логирование
Все записи сохраняются в таблицу `raw_measurments` в бд `<db_file.db>` - запоминает последние `24ч`

Средняя температура за час сохраняется в таблицу `hourly_avg.` - запоминает за последние `30д`

Средняя температура за день сохраняется в таблицу `daily_avg` - запоминает за последний `1год`

#### Клиент
Клиент сделан на flask. Для запуска необходимо выполнить
```sh
cd client
python -m venv .venv
pip install -r requirements.txt
python app.py
```

#### Данные с порта должны приходить в виде
`ts - временная метка` \
`value -  значение`


