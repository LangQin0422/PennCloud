#!/bin/bash
# Fetch public IP and run the application with it
PUBLIC_IP=$(curl -s http://checkip.amazonaws.com)
exec /app/controller/KVSController $PUBLIC_IP