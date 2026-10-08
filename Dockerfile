FROM python:3.11-slim

# Install dependencies sistem yang diperlukan untuk build & native testing
RUN apt-get update && apt-get install -y --no-install-recommends \
    git \
    build-essential \
    && rm -rf /var/lib/apt/lists/*

# Install PlatformIO Core
RUN pip install --no-cache-dir platformio

# Tentukan working directory di dalam kontainer
WORKDIR /project

# Salin seluruh isi proyek ke dalam kontainer
COPY . /project

# Command default untuk menjalankan native unit test
CMD ["pio", "test", "-e", "native"]