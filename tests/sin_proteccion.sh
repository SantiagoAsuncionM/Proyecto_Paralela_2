PUERTO=8080
CANTIDAD=200

for i in $(seq 1 $CANTIDAD); do
    ./bin/pipectl 127.0.0.1 $PUERTO "SUBMIT yamls/sleep.yaml" &
done > respuestas.txt
wait

REPETIDOS=$(grep '^OK' respuestas.txt | sort | uniq -d | wc -l)

if [ "$REPETIDOS" -gt 0 ]; then
    echo "CONDICION DE CARRERA DETECTADA: $REPETIDOS ids repetidos"
    grep '^OK' respuestas.txt | sort | uniq -d | head
    exit 1
else
    echo "OK: ningun id repetido en $CANTIDAD envios simultaneos"
    exit 0
fi
