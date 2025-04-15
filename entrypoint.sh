#!/bin/bash

check_internet() {
    if curl -s --connect-timeout 5 https://github.com > /dev/null; then
        return 0
    else
        return 1
    fi
}

exec python3 tcp8800a.py  
