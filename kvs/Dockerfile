FROM ubuntu:20.04

ENV DEBIAN_FRONTEND=noninteractive

# Install necessary packages
RUN apt-get update && apt-get install -y \
    build-essential \
    libtool \
    pkg-config \
    cmake \
    git \
    xterm \
    libssl-dev \
    curl

# Set the working directory
WORKDIR /app

# Copy the compiled application and any other necessary files into the container
COPY ./build /app
COPY ./entrypoint.sh /app

RUN chmod +x /app/entrypoint.sh

# Expose the specific ports used by the application
EXPOSE 40050 50050-50060

# Set the entry point to your application
ENTRYPOINT ["/app/entrypoint.sh"]
