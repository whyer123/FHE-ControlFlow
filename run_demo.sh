#!/bin/bash
set -e

echo "Starting Docker build and execution for FHE Control Flow prototype..."
# This will build the container (including OpenFHE) and run the demo_loop executable
docker-compose up --build
