import threading
import time
import json
from io import BytesIO
import os

import cv2
import numpy as np
from PIL import Image
import requests
from flask import Flask, render_template, Response, jsonify, request, send_from_directory

# Configurações do ESP32-CAM
ESP32_CAM_IP = "http://10.128.0.36"
CAPTURE_ENDPOINT = f"{ESP32_CAM_IP}/capture"
FACE_DETECTION_ENDPOINT = f"{ESP32_CAM_IP}/face-detection"

# Inicializa o classificador de faces (Haar Cascade)
face_classifier = cv2.CascadeClassifier(cv2.data.haarcascades + 'haarcascade_frontalface_default.xml')

# Variáveis compartilhadas
image_lock = threading.Lock()
image_available = threading.Event()
processed_image = None
face_count = 0

# Inicializa o aplicativo Flask
app = Flask(__name__)

@app.route('/')
def index():
    return render_template('index.html', face_count=face_count)

def generate_image():
    while True:
        # Aguarda até que a imagem processada esteja disponível
        image_available.wait()
        with image_lock:
            if processed_image is not None:
                # Codifica a imagem em formato JPEG
                ret, jpeg = cv2.imencode('.jpg', processed_image)
                frame = jpeg.tobytes()
                # Envia a imagem como um fluxo de bytes
                yield (b'--frame\r\n'
                       b'Content-Type: image/jpeg\r\n\r\n' + frame + b'\r\n\r\n')
        time.sleep(0.1)

@app.route('/video_feed')
def video_feed():
    return Response(generate_image(), mimetype='multipart/x-mixed-replace; boundary=frame')

@app.route('/face_count')
def get_face_count():
    return jsonify({'face_count': face_count})

# Endpoint para receber a imagem do ESP32-CAM
@app.route('/receive-image', methods=['POST'])
def receive_image():
    global processed_image, face_count
    try:
        # Recebe a imagem enviada pelo ESP32-CAM
        img_data = request.get_data()
        # Converte os dados da imagem para um array NumPy
        img_array = np.frombuffer(img_data, dtype=np.uint8)
        image = cv2.imdecode(img_array, cv2.IMREAD_COLOR)

        if image is not None:
            # Converte para escala de cinza
            gray = cv2.cvtColor(image, cv2.COLOR_BGR2GRAY)
            # Detecta faces na imagem
            faces = face_classifier.detectMultiScale(
                gray,
                scaleFactor=1.1,
                minNeighbors=5,
                minSize=(30, 30)
            )

            face_data = []
            for (x, y, w, h) in faces:
                # Desenha retângulos ao redor das faces detectadas
                cv2.rectangle(image, (x, y), (x + w, y + h), (0, 255, 0), 2)
                face_data.append({'x': int(x), 'y': int(y), 'w': int(w), 'h': int(h)})

            # Envia os dados das faces detectadas para o ESP32-CAM
            if face_data:
                try:
                    headers = {'Content-Type': 'application/json'}
                    json_data = json.dumps(face_data)
                    response = requests.post(FACE_DETECTION_ENDPOINT, data=json_data, headers=headers)
                    if response.status_code == 200:
                        print("Dados de detecção de face enviados para o ESP32-CAM.")
                    else:
                        print(f"Erro ao enviar dados para o ESP32-CAM: {response.status_code}")
                except Exception as e:
                    print(f"Erro ao enviar dados para o ESP32-CAM: {e}")

            # Atualiza a imagem processada e o contador de faces
            with image_lock:
                processed_image = image.copy()
                face_count = len(faces)
                image_available.set()

                # Salva a imagem processada no diretório 'static'
                cv2.imwrite('static/processed_image.jpg', processed_image)

            return "Imagem recebida e processada com sucesso", 200
        else:
            print("Não foi possível decodificar a imagem recebida.")
            return "Erro ao decodificar a imagem", 400
    except Exception as e:
        print(f"Erro ao processar a imagem recebida: {e}")
        return "Erro no servidor ao processar a imagem", 500

def capture_image_thread():
    while True:
        try:
            # Solicita ao ESP32-CAM para capturar uma nova imagem
            response = requests.get(CAPTURE_ENDPOINT)
            if response.status_code == 200:
                print("Captura solicitada ao ESP32-CAM.")
            else:
                print(f"Erro ao solicitar captura: {response.status_code}")
        except Exception as e:
            print(f"Erro ao acessar o ESP32-CAM: {e}")
        time.sleep(5)

if __name__ == '__main__':
    # Cria o diretório 'static' se não existir
    if not os.path.exists('static'):
        os.makedirs('static')

    # Inicia a thread de captura
    threading.Thread(target=capture_image_thread, daemon=True).start()
    # Executa o aplicativo Flask
    app.run(host='0.0.0.0', port=5000, debug=False)
