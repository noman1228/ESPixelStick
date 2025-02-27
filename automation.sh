#!/bin/bash

# Set your ESP device and baud rate
PORT="/dev/ttyUSB1"  # Adjust this based on your ESP32/ESP32-S3
BAUD=460800
ESPTOOL="esptool.py"

# Function to check command success
check_success() {
    if [ $? -ne 0 ]; then
        echo "❌ Error: $1 failed!"
        exit 1
    fi
}

echo "🔄 Erasing Flash..."
$ESPTOOL --chip esp32 --port $PORT --baud $BAUD erase_flash
check_success "Flash erase"

echo "⚙️ Running gulp main task..."
gulp
check_success "Gulp task"

echo "🛠️ Compiling project..."
pio run
check_success "Compilation"

echo "📤 Uploading firmware..."
pio run -t upload
check_success "Firmware upload"

echo "📂 Uploading filesystem..."
pio run -t uploadfs
check_success "Filesystem upload"

echo "🖥️ Opening serial monitor..."
pio device monitor -b 115200
