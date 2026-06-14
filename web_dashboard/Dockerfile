FROM python:3.11-slim

LABEL org.opencontainers.image.title="Solar Ball Web Dashboard"
LABEL org.opencontainers.image.description="Web configuration dashboard for Solar Ball directional light sensor"
LABEL org.opencontainers.image.version="1.3.0"

WORKDIR /app

RUN apt-get update && apt-get install -y --no-install-recommends \
    curl \
    && rm -rf /var/lib/apt/lists/*

COPY web_dashboard/requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

COPY web_dashboard/ .

RUN mkdir -p /app/data

EXPOSE 5000

HEALTHCHECK --interval=30s --timeout=5s --start-period=10s --retries=3 \
    CMD curl -f http://localhost:5000/api/stats || exit 1

ENTRYPOINT ["python", "app.py"]
CMD ["--host", "0.0.0.0", "--port", "5000"]
