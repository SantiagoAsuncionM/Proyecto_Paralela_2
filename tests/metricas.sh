#!/bin/bash

AGENTES=$1
REPS=${2:-5}
PUERTO=8080
CLIENTE="./bin/pipectl 127.0.0.1 $PUERTO"

if [ -z "$AGENTES" ]; then
    echo "Uso: $0 <agentes> [repeticiones]"
    exit 1
fi

medir_una() {
    local id
    id=$($CLIENTE "SUBMIT $1" | awk '/^OK/ {print $2}')
    if [ -z "$id" ]; then
        echo "error"
        return
    fi

    while $CLIENTE "STATUS $id" | head -1 | grep -qE 'queued|running'; do
        sleep 0.2
    done

    $CLIENTE "STATUS $id" | head -1 | awk '{print $4}'
}

medir() {
    local nombre=$1 yaml=$2

    echo "== $nombre ($AGENTES agentes)"
    for r in $(seq 1 "$REPS"); do
        local micros
        micros=$(medir_una "$yaml")
        echo "  repeticion $r/$REPS: $micros microsegundos"
    done
    echo
}

medir ancho    yamls/ancho.yaml
medir profundo yamls/profundo.yaml

