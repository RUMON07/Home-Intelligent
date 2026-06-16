import cv2
import threading
import time
import os
import requests
import face_recognition
from ultralytics import YOLO
from datetime import datetime

# --- Configuration ---
TOKEN = "8630270967:AAEpNahDspEU2wG2yTcOb305oTE2RKxEfx0"
CHAT_ID = "-1004297425384"
STREAM_URL = "http://192.168.137.85:81/stream"
DATABASE_DIR = "database"
BACKUP_DIR = "backups"

# Create folder
os.makedirs(DATABASE_DIR, exist_ok=True)
os.makedirs(BACKUP_DIR, exist_ok=True)

# face load from database
print("Loading Database Faces...")
known_face_encodings = []
known_face_names = []
for file in os.listdir(DATABASE_DIR):
    if file.endswith((".jpg", ".jpeg", ".png")):
        image = face_recognition.load_image_file(os.path.join(DATABASE_DIR, file))
        encodings = face_recognition.face_encodings(image)
        if encodings:
            known_face_encodings.append(encodings[0])
            known_face_names.append(os.path.splitext(file)[0])

print("Loading YOLOv8 AI Model...")
model = YOLO("yolov8n.pt")

# --- Background Process Function ---
def send_telegram_alert(img_path, time_str):
    print(f"\n[+] Sending Telegram Alert... Time: {time_str}")
    url = f"https://api.telegram.org/bot{TOKEN}/sendPhoto"
    with open(img_path, 'rb') as photo:
        payload = {'chat_id': CHAT_ID, 'caption': f"⚠️ Unknown Person Detected!\nTime: {time_str}"}
        try:
            response = requests.post(url, data=payload, files={'photo': photo}, timeout=10)
            if response.status_code == 200:
                print("[+] Telegram Alert Sent Successfully!\n")
            else:
                print(f"[-] Telegram API Error: {response.text}\n")
        except Exception as e:
            print(f"[-] Telegram Connection Error: {e}\n")

def delete_old_backups():
    while True:
        now = time.time()
        for f in os.listdir(BACKUP_DIR):
            f_path = os.path.join(BACKUP_DIR, f)
            if os.path.isfile(f_path) and (now - os.path.getmtime(f_path) > 86400):
                os.remove(f_path)
        time.sleep(3600)

threading.Thread(target=delete_old_backups, daemon=True).start()

# --- Camera multi-threading class ---
class CameraStream:
    def __init__(self, src=0):
        self.stream = cv2.VideoCapture(src)
        self.stream.set(cv2.CAP_PROP_BUFFERSIZE, 2)
        (self.grabbed, self.frame) = self.stream.read()
        self.stopped = False

    def start(self):
        threading.Thread(target=self.update, args=(), daemon=True).start()
        return self

    def update(self):
        while not self.stopped:
            if not self.grabbed:
                self.stop()
            else:
                (self.grabbed, self.frame) = self.stream.read()

    def read(self):
        return self.frame

    def stop(self):
        self.stopped = True

cam = CameraStream(STREAM_URL).start()
print("System is LIVE! Press 'q' to stop.")

last_alert_time = 0
frame_count = 0

# Variables to reduce lag
face_locations = []
face_names = []
unknown_detected = False

while True:
    frame = cam.read()
    if frame is None:
        continue

    # Configuration update: Size and confidence changes
    small_frame = cv2.resize(frame, (0, 0), fx=0.5, fy=0.5)
    rgb_small_frame = cv2.cvtColor(small_frame, cv2.COLOR_BGR2RGB)
    
    results = model(frame, classes=[0], conf=0.3, verbose=False)
    annotated_frame = results[0].plot()
    
    person_detected = False
    for r in results:
        if len(r.boxes) > 0:
            person_detected = True
            break

    if person_detected:
        # Face scan will be done once every 3 frames to eliminate lag and keep the box stable
        if frame_count % 3 == 0:
            face_locations = face_recognition.face_locations(rgb_small_frame)
            face_encodings = face_recognition.face_encodings(rgb_small_frame, face_locations)

            face_names = []
            unknown_detected = False

            for face_encoding in face_encodings:
                matches = face_recognition.compare_faces(known_face_encodings, face_encoding, tolerance=0.5)
                name = "Unknown"
                if True in matches:
                    name = known_face_names[matches.index(True)]
                else:
                    unknown_detected = True
                face_names.append(name)

        current_time_obj = datetime.now()
        time_str_file = current_time_obj.strftime("%d-%m-%Y_%I-%M-%S_%p")
        time_str_msg = current_time_obj.strftime("%d-%m-%Y %I:%M:%S %p")
        
        for (top, right, bottom, left), name in zip(face_locations, face_names):
            # Scaling is now 2 times
            top *= 2
            right *= 2
            bottom *= 2
            left *= 2
            
            color = (0, 255, 0) if name != "Unknown" else (0, 0, 255)
            cv2.rectangle(annotated_frame, (left, top), (right, bottom), color, 2)
            cv2.putText(annotated_frame, name, (left, top - 10), cv2.FONT_HERSHEY_SIMPLEX, 0.9, (255, 255, 255), 2)

        if unknown_detected and (time.time() - last_alert_time > 5):
            print("\n[!] Unknown person detected! Triggering alert process...")
            img_path = os.path.join(BACKUP_DIR, f"{time_str_file}.jpg")
            cv2.imwrite(img_path, annotated_frame) 
            threading.Thread(target=send_telegram_alert, args=(img_path, time_str_msg), daemon=True).start()
            last_alert_time = time.time()
            
        elif len(face_names) > 0 and frame_count % 10 == 0:
            img_path = os.path.join(BACKUP_DIR, f"{time_str_file}.jpg")
            cv2.imwrite(img_path, annotated_frame)

    frame_count += 1
    cv2.imshow("AI Security Camera - Live View", annotated_frame)
    if cv2.waitKey(1) & 0xFF == ord('q'):
        break

cam.stop()
cv2.destroyAllWindows()