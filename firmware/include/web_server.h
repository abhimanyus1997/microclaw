#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <WebServer.h>
#include "common.h"

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

            if (server.hasArg("plain") && _handler) {
                String reply = _handler(server.arg("plain"));
                
                DynamicJsonDocument responseDoc(4096);
                
                // Try to parse reply as JSON if it's a structural thought/tool block
                // (main.cpp handles parsing AI output into a final reply string currently)
                // However, the user wants the UI to display which tool was called.
                // We should ideally pass the structured data from main.cpp back here.
                
                // For now, let's just send the raw reply string if main.cpp already 
                // appended the [Tool: ...] info, OR better yet, let main.cpp return JSON.
                
                // [DEBT] main.cpp handleAgentRequest currently returns a String. 
                // Let's modify handleAgentRequest to return a JSON string.
                
                server.send(200, "application/json", reply);
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
        return R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>MicroClaw AI</title>
    <link href="https://fonts.googleapis.com/css2?family=Outfit:wght@300;400;600&display=swap" rel="stylesheet">
    <script src="https://cdn.jsdelivr.net/npm/marked/marked.min.js"></script>
    <style>
        :root {
            --bg-dark: #0f1014;
            --bg-card: #181a20;
            --accent: #ff6b6b;
            --text-primary: #ffffff;
            --text-secondary: #8b9bb4;
            --user-msg: #2d313a;
            --ai-msg: #ff6b6b;
        }
        body {
            font-family: 'Outfit', sans-serif;
            background-color: var(--bg-dark);
            color: var(--text-primary);
            margin: 0;
            display: flex;
            flex-direction: column;
            height: 100vh;
        }
        header {
            padding: 15px 20px;
            background: rgba(24, 26, 32, 0.9);
            border-bottom: 1px solid #2d313a;
            display: flex;
            justify-content: space-between;
            align-items: center;
            font-weight: 600;
            font-size: 1.2rem;
            letter-spacing: 0.5px;
        }
        header div { display: flex; align-items: center; gap: 8px; }
        header span { color: var(--accent); }
        .btn-clear {
            background: none;
            border: 1px solid #444;
            color: var(--text-secondary);
            padding: 4px 10px;
            font-size: 0.75rem;
            border-radius: 4px;
            cursor: pointer;
        }
        .btn-clear:hover { border-color: var(--accent); color: var(--accent); }

        #chat-container {
            flex: 1;
            overflow-y: auto;
            padding: 20px;
            display: flex;
            flex-direction: column;
            gap: 15px;
        }
        .message {
            max-width: 85%;
            padding: 12px 18px;
            border-radius: 12px;
            line-height: 1.6;
            font-size: 0.95rem;
            animation: fadeIn 0.3s ease;
        }
        .agent {
            align-self: flex-start;
            background: var(--bg-card);
            border: 1px solid #2d313a;
            color: var(--text-primary);
            border-bottom-left-radius: 2px;
        }
        .user {
            align-self: flex-end;
            background: var(--accent);
            color: white;
            border-bottom-right-radius: 2px;
            box-shadow: 0 4px 15px rgba(255, 107, 107, 0.2);
        }
        /* Markdown Styling */
        .message p { margin: 8px 0; }
        .message ul, .message ol { padding-left: 20px; margin: 8px 0; }
        .message code { background: rgba(255,255,255,0.1); padding: 2px 5px; border-radius: 4px; font-family: monospace; }
        .message pre { background: #000; padding: 10px; border-radius: 6px; overflow-x: auto; margin: 10px 0; }

        #input-area {
            padding: 20px;
            background: var(--bg-card);
            border-top: 1px solid #2d313a;
            display: flex;
            gap: 10px;
        }
        input {
            flex: 1;
            padding: 12px 16px;
            border-radius: 8px;
            border: 1px solid #2d313a;
            background: var(--bg-dark);
            color: white;
            font-family: inherit;
            outline: none;
            transition: border 0.2s;
        }
        input:focus { border-color: var(--accent); }
        .btn-send {
            padding: 12px 24px;
            border-radius: 8px;
            border: none;
            background: var(--accent);
            color: white;
            font-weight: 600;
            cursor: pointer;
            transition: transform 0.1s;
        }
        .btn-send:active { transform: scale(0.95); }
        .btn-send:disabled { opacity: 0.5; cursor: not-allowed; }
        
        .debug-panel {
            margin-top: 10px;
            padding-top: 10px;
            border-top: 1px solid #333;
            display: none; /* Hidden by default */
        }
        .debug-toggle {
            font-size: 0.7rem;
            color: var(--text-secondary);
            cursor: pointer;
            text-decoration: underline;
            margin-top: 8px;
            display: block;
        }
        .thought {
            font-size: 0.8rem;
            color: var(--text-secondary);
            font-style: italic;
            margin-bottom: 5px;
        }
        .tool-info {
            font-size: 0.8rem;
            color: #61dafb;
            background: rgba(97, 218, 251, 0.05);
            padding: 6px;
            border-radius: 4px;
            font-family: monospace;
        }
        
        @keyframes fadeIn { from { opacity: 0; transform: translateY(5px); } to { opacity: 1; transform: translateY(0); } }
    </style>
</head>
<body>
    <header>
        <div>Micro<span>Claw</span> AI</div>
        <button class="btn-clear" onclick="clearHistory()">Clear Chat</button>
    </header>
    <div id="chat-container">
    </div>
    <div id="input-area">
        <input type="text" id="msgInput" placeholder="Type a message..." onkeydown="if(event.key==='Enter') send()">
        <button onclick="send()" id="sendBtn" class="btn-send">Send</button>
    </div>
    <script>
        let chatHistory = JSON.parse(localStorage.getItem('microclaw_history') || '[]');
        
        window.onload = () => {
            if (chatHistory.length === 0) {
                addMsg("Hello! I'm your ESP32-powered AI. How can I assist you?", "agent");
            } else {
                chatHistory.forEach(m => addMsg(m.text, m.sender, m.thought, m.tool, m.tool_result));
            }
        };

        async function send() {
            const input = document.getElementById('msgInput');
            const text = input.value.trim();
            if (!text) return;

            addMsg(text, 'user');
            saveToHistory(text, 'user');
            input.value = '';
            setInputState(false);

            try {
                const res = await fetch('/api/chat', {
                    method: 'POST',
                    headers: {'Content-Type': 'application/json'},
                    body: JSON.stringify({
                        text: text,
                        history: chatHistory.slice(-6)
                    })
                });
                const data = await res.json();
                addMsg(data.reply || "No reply", 'agent', data.thought, data.tool, data.tool_result);
                saveToHistory(data.reply || "No reply", 'agent', data.thought, data.tool, data.tool_result);
            } catch (e) {
                addMsg("Connection Error: " + e, 'agent');
            }
            setInputState(true);
            input.focus();
        }

        function clearHistory() {
            if(confirm("Clear chat history?")) {
                localStorage.removeItem('microclaw_history');
                chatHistory = [];
                document.getElementById('chat-container').innerHTML = '';
                addMsg("Chat cleared. Hello again!", "agent");
            }
        }

        function saveToHistory(text, sender, thought, tool, tool_result) {
            chatHistory.push({text, sender, thought, tool, tool_result});
            if (chatHistory.length > 20) chatHistory.shift();
            localStorage.setItem('microclaw_history', JSON.stringify(chatHistory));
        }

        function addMsg(text, sender, thought, tool, tool_result) {
            const chat = document.getElementById('chat-container');
            const div = document.createElement('div');
            div.className = 'message ' + sender;
            
            // Render text as Markdown for agents
            let bubbleHtml = (sender === 'agent') ? marked.parse(text) : `<div>${text}</div>`;
            
            // Add technical details (hidden by default)
            if (sender === 'agent' && (thought || (tool && tool !== 'none'))) {
                const debugId = 'debug-' + Math.random().toString(36).substr(2, 9);
                bubbleHtml += `<a class="debug-toggle" onclick="document.getElementById('${debugId}').style.display = document.getElementById('${debugId}').style.display === 'block' ? 'none' : 'block'">Show Technical Details</a>`;
                bubbleHtml += `<div id="${debugId}" class="debug-panel">`;
                if (thought) bubbleHtml += `<div class="thought">🤔 Thought: ${thought}</div>`;
                if (tool && tool !== 'none') {
                    bubbleHtml += `<div class="tool-info">🛠️ Tool: ${tool}`;
                    if (tool_result) bubbleHtml += `<br>↳ Output: ${tool_result}`;
                    bubbleHtml += `</div>`;
                }
                bubbleHtml += `</div>`;
            }

            div.innerHTML = bubbleHtml;
            chat.appendChild(div);
            chat.scrollTop = chat.scrollHeight;
        }

        function setInputState(enabled) {
            document.getElementById('msgInput').disabled = !enabled;
            document.getElementById('sendBtn').disabled = !enabled;
        }
    </script>
</body>
</html>
)rawliteral";
    }
};

#endif
