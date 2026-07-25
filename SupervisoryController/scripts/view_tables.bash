#!/usr/bin/env bash
MYSQL_HOST="localhost"
MYSQL_USER="pi"
MYSQL_DB="elevatorg1"
MYSQL_TABLE="elevatorNetwork"

while true; do
  changes=$(mysql -h "$MYSQL_HOST" -u "$MYSQL_USER" -pese -D "$MYSQL_DB" -N -B \
    -e "SELECT `index`, `date`, `time`, currentFloor
        FROM $MYSQL_TABLE
        WHERE `index` = (
            SELECT MAX(`index`)
            FROM elevatorNetwork
        );" )

  if [ -n "$changes" ]; then
    echo "$changes"
  fi

  sleep 0.5
done
