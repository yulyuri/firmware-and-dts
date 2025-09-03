#!/bin/bash

DOMAIN=$1
STATE_FILE="/sys/kernel/debug/pm_genpd/${DOMAIN}/current_state"

if sudo grep -q on "$STATE_FILE"; then
  echo 1
else
  echo 0
fi

