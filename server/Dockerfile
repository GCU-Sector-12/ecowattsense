# Slim multi-arch base (amd64 and arm64)
FROM python:3.12-slim

# No .pyc files, unbuffered logs for docker logs
ENV PYTHONDONTWRITEBYTECODE=1 PYTHONUNBUFFERED=1

# Non-root user, data folder owned by it (compose bind-mounts /app/data)
RUN groupadd -g 1000 app && useradd -u 1000 -g 1000 -m app \
    && mkdir -p /app/data && chown app:app /app/data

# server/ lands in /app/server so config.py ROOT is /app
WORKDIR /app/server

# Dependencies first, so this layer is cached when only code changes
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

# Application code
COPY --chown=app:app . .

USER app
EXPOSE 8000

# curl is not in slim, so use python urllib
HEALTHCHECK --interval=30s --timeout=5s --start-period=10s --retries=3 \
    CMD python3 -c "import urllib.request; urllib.request.urlopen('http://127.0.0.1:8000/api/health', timeout=4)"

# Behind nginx and Cloudflare, so trust forwarded headers
CMD ["uvicorn", "ecowattsense_server.main:app", "--host", "0.0.0.0", "--port", "8000", "--proxy-headers", "--forwarded-allow-ips=*"]
