# IRC Communicator - Client & Server

---

## Project Description

This project is a functional, multi-room **IRC-style communicator** implemented in **Python (Client)** and **C (Server)**. It supports basic chat functionalities, nickname changes, room management, and includes a GUI built with `tkinter` for the client.

Designed for local testing or LAN communication, the system features private and public rooms, real-time user list updates, and multi-client support using threads.

## Authors

**Mateusz Graja & Piotr Żurek**

## Features

### ✅ Client (Python)
- GUI using `tkinter` with a dark mode theme.
- Realtime message feed with timestamps.
- Room-based chatting with automatic user list updates.
- Fullscreen toggle.
- Detects room events (joins, leaves, nickname changes) and updates user lists accordingly.

### ✅ Server (C)
- Multi-client handling with threads.
- Room creation, joining, listing, leaving, deleting.
- User nickname management.
- Room visibility (public/secret).
- Kick user back to Lobby if a room is deleted.
- Signal handling for graceful server shutdown.

## How to Run

### 🖥️ Server (Linux/macOS)
```bash
gcc server.c -o server -pthread
./server 5555
```

### 🖱️ Client (Python)
```bash
pip install tk
python client.py
```

### Optional: Change the IP and port
In `client.py`:
```python
server_address = ("127.0.0.1", 5555)  # IP & Port of server
```

## Client GUI Overview

- **Left Panel**: Chat area (auto-scroll)
- **Right Panel**: User list in the current room
- **Bottom**: Message entry + buttons for Send and Fullscreen toggle

Example GUI:
```
 -----------------------------------------------------------
| Chat Window                                | User List   |
|-------------------------------------------|-------------|
| [12:34:56] Anon1: Hello!                   | Anon1       |
| [12:34:58] Anon2: Hi there!                | Anon2       |
| ...                                        |             |
 -----------------------------------------------------------
| [ Type your message here... ]   [Send] [Fullscreen]       |
 -----------------------------------------------------------
```

## Available Commands (Client-side via chat)

- `/nick newname` – Change nickname
- `/join roomname` – Join an existing room
- `/leave` – Leave current room (go back to Lobby)
- `/create roomname` – Create a public room
- `/create_secret roomname` – Create a private room
- `/delete roomname` – Delete room (if you're creator)
- `/list` – List all available public rooms
- `/who` – Show users in the current room
- `/exit` – Leave the server

## Server Highlights

- Max 30 concurrent clients
- Usernames are auto-generated (Anon1, Anon2...) and guaranteed unique
- Server prints logs of user joins, leaves, messages, room creation/deletion
- Graceful cleanup of resources on SIGINT (Ctrl+C)

## Example Output (Server Console)
```
Serwer IRC działa na porcie 5555.
Nadano unikalny domyślny nick: Anon1
Użytkownik 'Anon1' połączony i dołączony do pokoju 'Lobby'.
Użytkownik 'Anon1' dołączył do pokoju 'Lobby'.
```

## Notes

- The server handles multiple clients via `pthread` threads.
- GUI is responsive and allows sending messages via button or Enter key.
- Rooms are cleaned up manually using `/delete` command.
- Secret rooms will not appear in the `/list` command.

---

Enjoy chatting and building upon your custom IRC communicator! ☕💬

