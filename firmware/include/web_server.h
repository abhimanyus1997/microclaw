#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <WebServer.h>
#include <ArduinoJson.h>

class WebInterface {
public:
    WebInterface(int port = 80) : server(port) {}

    // Callback type for processing messages
    typedef std::function<String(String)> MessageHandler;

    void begin(MessageHandler handler) {
        _handler = handler;

        // Serve HTML
        server.on("/", HTTP_GET, [this]() {
            server.send(200, "text/html", getHtml());
        });

        // Handle Chat API
        server.on("/api/chat", HTTP_POST, [this]() {
            if (!server.hasArg("plain")) {
                server.send(400, "application/json", "{\"error\":\"No body\"}");
                return;
            }

            DynamicJsonDocument doc(1024);
            deserializeJson(doc, server.arg("plain"));
            String text = doc["text"].as<String>();

            if (text.length() > 0 && _handler) {
                String reply = _handler(text);
                
                DynamicJsonDocument responseDoc(2048);
                responseDoc["reply"] = reply;
                String responseStr;
                serializeJson(responseDoc, responseStr);
                
                server.send(200, "application/json", responseStr);
            } else {
                server.send(400, "application/json", "{\"error\":\"Empty message\"}");
            }
        });

        server.begin();
        Serial.println("Web Server started on port 80");
    }

    void handleClient() {
        server.handleClient();
    }

private:
    WebServer server;
    MessageHandler _handler;

    String getHtml() {
        // Simple dark-mode chat UI
        return R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>MicroClaw Chat</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: sans-serif; background: #1e1e1e; color: #eee; display: flex; flex-direction: column; height: 100vh; margin: 0; }
    #chat { flex: 1; overflow-y: auto; padding: 20px; display: flex; flex-direction: column; gap: 10px; }
    .msg { max-width: 80%; padding: 10px 15px; border-radius: 15px; line-height: 1.4; }
    .user { align-self: flex-end; background: #0b93f6; color: white; border-bottom-right-radius: 2px; }
    .agent { align-self: flex-start; background: #333; color: #eee; border-bottom-left-radius: 2px; }
    #input-area { padding: 20px; background: #252525; display: flex; gap: 10px; }
    input { flex: 1; padding: 10px; border-radius: 5px; border: none; background: #333; color: white; }
    button { padding: 10px 20px; border-radius: 5px; border: none; background: #0b93f6; color: white; cursor: pointer; }
    button:disabled { background: #555; }
  </style>
</head>
<body>
  <div id="chat">
    <div class="msg agent">Hello! I am MicroClaw. How can I help you?</div>
  </div>
  <div id="input-area">
    <input type="text" id="msgBtn" placeholder="Type a message..." onkeydown="if(event.key==='Enter') send()">
    <button onclick="send()" id="sendBtn">Send</button>
  </div>
  <script>
    async function send() {
      const input = document.getElementById('msgBtn');
      const text = input.value.trim();
      if (!text) return;

      addMsg(text, 'user');
      input.value = '';
      input.disabled = true;
      document.getElementById('sendBtn').disabled = true;

      try {
        const res = await fetch('/api/chat', {
          method: 'POST',
          headers: {'Content-Type': 'application/json'},
          body: JSON.stringify({text})
        });
        const data = await res.json();
        addMsg(data.reply || "Error: " + JSON.stringify(data), 'agent');
      } catch (e) {
        addMsg("Connection Error: " + e, 'agent');
      }

      input.disabled = false;
      document.getElementById('sendBtn').disabled = false;
      input.focus();
    }

    function addMsg(text, sender) {
      const chat = document.getElementById('chat');
      const div = document.createElement('div');
      div.className = 'msg ' + sender;
      div.textContent = text;
      chat.appendChild(div);
      chat.scrollTop = chat.scrollHeight;
    }
  </script>
</body>
</html>
)rawliteral";
    }
};

#endif
