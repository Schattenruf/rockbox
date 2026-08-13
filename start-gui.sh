#!/usr/bin/env bash
set -e

sudo apt-get update
sudo apt-get install -y \
  xvfb \
  x11vnc \
  fluxbox \
  websockify \
  novnc \
  x11-utils \
  xdotool \
  imagemagick \
  libsdl2-dev \
  libx11-dev \
  libxext-dev \
  libxrender-dev \
  libpng-dev \
  build-essential \
  make \
  gcc \
  g++ \
  perl \
  python3 \
  zip

echo "Starting virtual display..."
Xvfb :1 -screen 0 1024x768x24 -ac &
export DISPLAY=:1

echo "Starting window manager..."
fluxbox &

echo "Starting VNC server..."
x11vnc -display :1 -nopw -forever -shared -rfbport 5900 &

echo "Starting noVNC on port 6080..."
websockify --web=/usr/share/novnc 6080 localhost:5900 &

echo ""
echo "GUI ready."
echo "Open the forwarded port 6080 in Codespaces."
echo ""
