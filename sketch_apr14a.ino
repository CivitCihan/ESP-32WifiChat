#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <map>

// WiFi bilgilerini buraya gir
const char* ssid = "Civit";
const char* password = "362800Ea";

AsyncWebServer server(80);
AsyncWebSocket ws("/ws"); // WebSocket endpoint

// Basit HTML + JS WebSocket chat arayüzü
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>Screet ESP32 Chat</title>
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <style>
    * {
      box-sizing: border-box;
      font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
    }

    body {
      background-color: #f7f7f7;
      margin: 0;
      padding: 20px;
      display: flex;
      justify-content: center;
      align-items: flex-start;
      min-height: 100vh;
    }

    .chat-container {
      width: 100%;
      max-width: 500px;
      background: #ffffff;
      border-radius: 10px;
      box-shadow: 0 4px 12px rgba(0, 0, 0, 0.1);
      padding: 20px;
    }

    h2 {
      margin-top: 0;
      text-align: center;
      color: #405D72;
    }

    #chat {
      height: 250px;
      border: 1px solid #ccc;
      border-radius: 6px;
      overflow-y: auto;
      padding: 10px;
      background: #f0f0f0;
      font-size: 14px;
    }

    #msg {
      width: 100%;
      padding: 10px;
      margin-top: 10px;
      border: 1px solid #ccc;
      border-radius: 6px;
      font-size: 14px;
    }

    .send-btn {
      width: 100%;
      margin-top: 10px;
      padding: 10px;
      background-color: #405D72;
      color: white;
      border: none;
      border-radius: 6px;
      cursor: pointer;
      font-size: 16px;
      transition: background-color 0.3s ease;
    }

    .send-btn:hover {
      background-color: #2e4457;
    }
  </style>
</head>
<body>
  <div class="chat-container">
    <h2>Screet ESP32 Chat</h2>
    <div id="chat"></div>
    <input id="msg" type="text" placeholder="Type your message...">
    <button class="send-btn" onclick="sendMessage()">Send</button>
  </div>

  <script>
    const chatBox = document.getElementById('chat');
    const msgInput = document.getElementById('msg');
    const socket = new WebSocket(`ws://${location.host}/ws`);

    let userName = "";
    let isUserNameSet = false;

    socket.onmessage = function(event) {
      const newMsg = document.createElement('div');
      newMsg.innerHTML = event.data;
      chatBox.appendChild(newMsg);
      chatBox.scrollTop = chatBox.scrollHeight;
    };

    msgInput.addEventListener("keyup", function(event) {
      if (event.key === "Enter") {
        sendMessage();
      }
    });

    function sendMessage() {
      const message = msgInput.value.trim();
      if (message !== "") {
        if (!userName) {
          userName = message; // İlk mesaj kullanıcı adı olur
          socket.send("__USERNAME__:" + userName);
        } else {
          socket.send(message);
        }
        msgInput.value = '';
      }
    }

  </script>
</body>
</html>
)rawliteral";

// Yeni gelen mesajı tüm istemcilere yay
std::map<uint32_t, String> userColors;
std::map<uint32_t, String> userNames;
String lastUser = "";

String getUserColor(uint32_t id) {
  // Basit sabit renk atamaları
  if (userColors.find(id) == userColors.end()) {
    const char* colors[] = {"red", "green", "blue", "purple", "orange", "brown", "teal"};
    userColors[id] = colors[userColors.size() % 7];
  }
  return userColors[id];
}

void notifyAllClientsFormatted(uint32_t clientId, const String& message) {
  // Eğer kullanıcı adı ayarlanmadıysa, bu mesaj username olabilir
  if (message.startsWith("__USERNAME__:")) {
    String username = message.substring(13);
    userNames[clientId] = username;
    Serial.printf("User %u set username: %s\n", clientId, username.c_str());
    ws.text(clientId, "<i>Username set as <b>" + username + "</b></i>");
    return;
  }

  String color = getUserColor(clientId);
  String name = userNames.count(clientId) ? userNames[clientId] : "User " + String(clientId);
  String formattedMessage = "";

  if (name != lastUser) {
    formattedMessage = "<span style='color:" + color + "'><b>" + name + ":</b></span> " + message;
  } else {
    formattedMessage = "&emsp;" + message;
  }

  lastUser = name;
  ws.textAll(formattedMessage);
}


// WebSocket olay yöneticisi
void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
                      AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    Serial.printf("Client %u connected\n", client->id());
  } else if (type == WS_EVT_DISCONNECT) {
    Serial.printf("Client %u disconnected\n", client->id());
  } else if (type == WS_EVT_DATA) {
  String msg = "";
  for (size_t i = 0; i < len; i++) {
    msg += (char)data[i];
  }
  Serial.printf("Client %u sent message: %s\n", client->id(), msg.c_str());
  notifyAllClientsFormatted(client->id(), msg);
}

}

void setup() {
  Serial.begin(115200);

  WiFi.begin(ssid, password);
  Serial.print("WiFi bağlanıyor");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi bağlandı, IP adresi: " + WiFi.localIP().toString());

  ws.onEvent(onWebSocketEvent);
  server.addHandler(&ws);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", index_html);
  });

  server.begin();
}

void loop() {
  // WebSocket olayı loop içinde sürekli dinlenir
}
