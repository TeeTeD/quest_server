#!/bin/bash

check_internet() {
    if curl -s --connect-timeout 5 https://github.com > /dev/null; then
        return 0
    else
        return 1
    fi
}

if check_internet; then
    echo "Интернет доступен, клонируем репозиторий..."
    git clone https://github.com/TeeTeD/quest_server.git /from_git
    cd /from_git/bookcase_latest
    echo "Использую версию от: $(find . -name "*.txt")"
    exec python3 tcp8800a.py  
else
    echo "Интернет не доступен, использую последнюю локальную копию..."
    cd /mnt/bookcase_latest

    echo "Последняя локальная копия от: $(find . -name "*.txt")"
    exec python3 tcp8800a.py  
fi