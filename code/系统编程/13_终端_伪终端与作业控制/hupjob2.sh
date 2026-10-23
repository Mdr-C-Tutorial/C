#!/bin/sh
rm -f ticks.log hupped.log
sh -c 'trap "echo job-got-HUP >> hupped.log; exit 0" HUP; i=0; while [ $i -lt 200 ]; do echo "$i" >> ticks.log; i=$((i+1)); sleep 0.05; done' &
sleep 0.5
echo "jobs started"
