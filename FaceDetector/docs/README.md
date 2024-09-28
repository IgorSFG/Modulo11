# Sistema de Detecção de Faces com ESP32-CAM e Threads

Este projeto consiste em um sistema de detecção de faces utilizando uma câmera ESP32-CAM e um sistema operacional de tempo real. O objetivo é dividir as tarefas executadas pelo microcontrolador em threads, melhorando a eficiência e a responsividade do sistema.

## Implementação

### Arquitetura do Sistema

- **ESP32-CAM**: Captura imagens e as envia para o servidor. Utiliza threads para gerenciar as tarefas de captura, envio e recebimento de dados.
- **Servidor Python**: Recebe as imagens, processa para detectar faces e envia os dados de detecção de volta ao ESP32-CAM. Possui um frontend para visualizar as imagens processadas e o número de faces detectadas.

## Threads Implementadas no ESP32-CAM

### Thread de Aquisição de Imagens (`capturePhotoTask`):

- Responsável por capturar imagens utilizando a câmera do ESP32-CAM.
- Controla um semáforo binário para indicar que uma nova imagem está pronta para ser enviada.

### Thread de Envio de Imagens (`sendPhotoTask`):

- Envia a imagem capturada para o servidor Python via HTTP POST.
- Sincroniza com o semáforo da thread de aquisição para garantir que apenas imagens novas sejam enviadas.

### Thread de Recebimento de Detecção (`handleFaceDetection`):

- Recebe os dados de detecção de faces enviados pelo servidor.
- Processa as coordenadas dos retângulos onde as faces foram detectadas.

## Threads Implementadas no Servidor Python

### Thread de Aquisição de Imagens (`image_acquisition_thread`):

- Envia solicitações ao ESP32-CAM para capturar novas imagens em intervalos regulares.

### Thread de Processamento de Imagens (`image_processing_thread`):

- Processa as imagens recebidas para detectar faces utilizando o OpenCV.
- Envia as coordenadas das faces detectadas de volta ao ESP32-CAM.

## Execução do Projeto

### Requisitos

**Hardware:**
- ESP32-CAM

**Software:**
- Python 3.x
- Bibliotecas Python: opencv-python, flask, requests, numpy, Pillow, ArduinoJson

### Instalar as Dependências:

Execute o comando:

```bash
pip install -r requirements.txt
```

### Executando o Servidor Python

Iniciar o Servidor:

```bash
python app.py
```

Acessar o Frontend:

Abra o navegador e acesse:

```
http://localhost:5000
```

### Configuração e Execução do ESP32-CAM

1. Atualizar as Credenciais Wi-Fi:

   No código do ESP32-CAM, atualize as variáveis `ssid` e `password` com as credenciais da sua rede Wi-Fi.

2. Configurar o Endereço IP do Servidor:

   No código do ESP32-CAM, certifique-se de que o endereço IP do servidor Python está correto.

3. Carregar o Código no ESP32-CAM:

   Utilize a Arduino IDE ou outra ferramenta compatível para compilar e fazer o upload do código para o ESP32-CAM.


### Funcionamento do projeto:

Você pode conferir o projeto funcionando clicando em:

https://drive.google.com/file/d/1ppeKd0WSIQGOef98yB0CxjZgHOcbqamH/view?usp=sharing

