#!/bin/bash

# Define an array of indices
indices=(0 1 2 3 4 5)

# Define a single string containing all IP addresses space-separated
all_ips="0.0.0.0:50051 0.0.0.0:50052 0.0.0.0:50053 0.0.0.0:50054 0.0.0.0:50055 0.0.0.0:50056"

# Loop through all indices
for index in "${indices[@]}"; do
  # Build the command to start the service with all IPs
  cmd="./build/KVSServer $index $all_ips"

  # Execute the command in the background
  echo "Starting service $index with IPs $all_ips"
  $cmd &
done

# Wait for all background processes to finish
wait
echo "All services have been started."

# ./build/KVSServer 0 0.0.0.0:50051 0.0.0.0:50052 0.0.0.0:50053 0.0.0.0:50054 0.0.0.0:50055 0.0.0.0:50056
# ./build/KVSServer 1 0.0.0.0:50051 0.0.0.0:50052 0.0.0.0:50053 0.0.0.0:50054 0.0.0.0:50055 0.0.0.0:50056
# ./build/KVSServer 2 0.0.0.0:50051 0.0.0.0:50052 0.0.0.0:50053 0.0.0.0:50054 0.0.0.0:50055 0.0.0.0:50056
# ./build/KVSServer 3 0.0.0.0:50051 0.0.0.0:50052 0.0.0.0:50053 0.0.0.0:50054 0.0.0.0:50055 0.0.0.0:50056
# ./build/KVSServer 4 0.0.0.0:50051 0.0.0.0:50052 0.0.0.0:50053 0.0.0.0:50054 0.0.0.0:50055 0.0.0.0:50056
# ./build/KVSServer 5 0.0.0.0:50051 0.0.0.0:50052 0.0.0.0:50053 0.0.0.0:50054 0.0.0.0:50055 0.0.0.0:50056