# ThreadXBrowser Emulator
# Builds and runs the host simulation for testing

FROM ubuntu:22.04

# Avoid prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# Install build tools
RUN apt-get update && apt-get install -y \
    build-essential \
    gcc \
    make \
    netcat-openbsd \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy source files
COPY src/ ./src/
COPY Makefile ./

# Build the emulator
RUN make host-sim

# Environment variables
ENV WS_SERVER_HOST=gateway
ENV WS_SERVER_PORT=9000
ENV WAIT_FOR_GATEWAY=1

# Copy run script
COPY emulator/run_emulator.sh ./
RUN chmod +x run_emulator.sh

# Run the emulator
CMD ["./run_emulator.sh"]
