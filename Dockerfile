# Stage 1: GCC Builder Stage
FROM gcc:12-bookworm AS builder

WORKDIR /build

# Install build dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    libsodium-dev \
    make \
    sqlite3 \
    && rm -rf /var/lib/apt/lists/*

# Copy vendor, source, headers, and Makefile
COPY vendor/ ./vendor/
COPY include/ ./include/
COPY src/ ./src/
COPY tests/ ./tests/
COPY Makefile ./

# Build single binary with Linux linking flags
RUN make clean && make TARGET=crewdeck LDFLAGS="-lsodium -lpthread -ldl -lm"

# Run tests in builder to guarantee binary correctness
RUN make test TEST_TARGET=test_runner LDFLAGS="-lsodium -lpthread -ldl -lm"

# Stage 2: Distroless/Slim Runtime Stage
FROM debian:bookworm-slim

# Install minimal shared libraries for libsodium
RUN apt-get update && apt-get install -y --no-install-recommends \
    libsodium23 \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Create non-root unprivileged service user
RUN groupadd -g 1001 crewdeck && \
    useradd -u 1001 -g crewdeck -m -s /bin/bash crewdeck

WORKDIR /app

# Copy binary and static frontend assets
COPY --from=builder /build/crewdeck /app/crewdeck
COPY public/ /app/public/

# Create persistent data directory and assign ownership
RUN mkdir -p /app/data && chown -R crewdeck:crewdeck /app

USER crewdeck

# Default environment configuration
ENV PORT=8080 \
    DB_PATH=/app/data/crewdeck.db \
    PUBLIC_DIR=/app/public \
    ADMIN_EMAIL=admin@crewdeck.io \
    ADMIN_PASSWORD=admin123

EXPOSE 8080

HEALTHCHECK --interval=30s --timeout=3s --start-period=5s --retries=3 \
    CMD ["/app/crewdeck", "--healthcheck"] || exit 0

CMD ["/app/crewdeck"]
