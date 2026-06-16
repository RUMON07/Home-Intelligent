# Smart AI Security Camera using ESP32-CAM & YOLOv8

An advanced, real-time AI security camera system built with ESP32-CAM, Python, YOLOv8, and Face Recognition. This system detects humans, identifies known/unknown faces, and sends instant photo alerts to a Telegram group.

## Features
* **Live Video Streaming:** Uses ESP32-CAM to stream video over a local network.
* **Human Detection:** Utilizes Ultralytics YOLOv8n to accurately detect humans while ignoring other movements (pets, fans, etc.).
* **Face Recognition:** Uses the `face_recognition` (dlib) library to match detected faces with a local database.
* **Zero-Lag Processing:** Implements multi-threading, frame-skipping (processing every 3rd frame), and dynamic frame resizing (0.5x) for smooth, real-time performance.
* **Telegram Alerts:** Automatically captures a photo of an "Unknown" person and sends it to a Telegram group with date and time. Built-in 5-second cooldown prevents spamming.
* **Auto-Storage Management:** A background daemon thread automatically deletes backup images older than 24 hours to save disk space.

## Hardware Required
1. **ESP32-CAM Module** (with OV2640 Camera Sensor)
2. **FTDI Programmer** (CP2102 or CH340) for uploading code
3. Jumper Wires
4. 5V Power Supply

## Software & AI Models
* **Language:** Python 3.x, C++ (Arduino IDE)
* **AI Models:** YOLOv8n (`yolov8n.pt`), dlib Face Recognition Model
* **Libraries:** OpenCV (`cv2`), `ultralytics`, `face_recognition`, `requests`

## Complete Setup Guide

### Step 1: Hardware & Driver Setup
1. Connect the ESP32-CAM to the FTDI programmer (Make sure `IO0` is connected to `GND` during upload).
2. Install the **CH340** or **CP210x** USB to UART bridge driver on your PC based on your FTDI module.

### Step 2: ESP32-CAM Setup (Arduino IDE)
1. Open Arduino IDE and add the ESP32 board manager URL in Preferences.
2. Install the ESP32 board package and select **AI Thinker ESP32-CAM**.
3. Open `esp32_cam_code.ino`, update your Wi-Fi `SSID` and `PASSWORD`.
4. Upload the code, open the Serial Monitor (115200 baud), and copy the IP address (e.g., `http://192.168.137.143:81/stream`).

### Step 3: Python Server Setup
1. Clone this repository:

```bash
git clone [https://github.com/yourusername/Smart-AI-Security-Camera.git](https://github.com/yourusername/Smart-AI-Security-Camera.git)
cd Smart-AI-Security-Camera
```

2. Install required Python packages:

```bash
pip install opencv-python ultralytics face_recognition requests cmake dlib
```

3. Add images of known people inside the `database/` folder. Name the file with the person's name (e.g., `rumon.jpg`).
4. Update the `STREAM_URL` in `main.py` with your ESP32-CAM IP address.

### Step 4: Telegram Bot Setup
1. **Create a Bot:** Open Telegram, search for **@BotFather**, and send the `/newbot` command. Follow the steps to name your bot. You will receive an **HTTP API Token**.
2. **Create a Group:** Create a new Telegram group and add your friends and the newly created bot to it.
3. **Make Bot Admin:** Go to the group settings and promote your bot to an **Administrator** (allow it to send messages).
4. **Get Group Chat ID:**
   * Send a test message (e.g., "Hello") in the group.
   * Open your web browser and go to this URL (replace `YOUR_BOT_TOKEN` with your actual token):

```text
https://api.telegram.org/botYOUR_BOT_TOKEN/getUpdates
```

   * Look for `"chat":{"id": -100xxxxxxxxxx}` in the text. Copy that entire negative number (including the minus sign).
5. **Update Code:** Open `main.py` and update the `TOKEN` and `CHAT_ID` variables with your newly generated details.

### Step 5: Run the System
Start the AI security camera by running:

```bash
python main.py
```

Press `q` on the video window to stop the system.

## How It Works (System Architecture)
1. **Capture:** The ESP32-CAM continuously captures frames and streams them over HTTP.
2. **Read & Optimize:** A background Python thread reads the stream using `cv2.VideoCapture` with a minimal buffer size. Frames are downscaled by 50%.
3. **Detect:** YOLOv8 scans the frame for the `person` class (class 0) with a confidence threshold of 0.3.
4. **Recognize:** If a person is found, `face_recognition` runs (every 3rd frame to prevent lag). It generates face encodings and compares them against the `database`.
5. **Alert:** If the face does not match, it is flagged as "Unknown". A red bounding box is drawn, and the frame is saved to `backups/`. The `requests` module sends this image to the Telegram API.