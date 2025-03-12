import tkinter as tk
from tkinter import scrolledtext, messagebox
import socket
import threading
from datetime import datetime

# Stała określająca rozmiar bufora
BUFFER_SIZE = 256

def receive_message(sock):
    """Pętla odbierająca wiadomości z serwera."""
    while True:
        try:
            message = sock.recv(BUFFER_SIZE).decode('utf-8')
            if not message:
                break

            # Sprawdzenie, czy wiadomość zaczyna się od "USERLIST/"
            if message.startswith("USERLIST/"):
                update_user_list(message[9:])
            else:
                # Wyświetlamy wiadomość w oknie czatu
                display_message(message)

                # Jeśli wiadomość sugeruje, że nastąpiła zmiana w pokoju (ktoś dołączył, opuścił, zmienił nick),
                # to wysyłamy /who, aby zaktualizować listę użytkowników.
                triggers = ["dołączył do pokoju", "opuścił pokój", "zmienił nick", "został przeniesiony"]
                if any(trigger in message for trigger in triggers):
                    try:
                        sock.send("/who".encode('utf-8'))
                    except:
                        messagebox.showerror("Error", "Nie można wysłać komendy /who.")

        except ConnectionAbortedError:
            break
        except OSError:
            break

    # Komunikat o rozłączeniu
    messagebox.showinfo("Rozłączono", "Rozłączono z serwera.")
    sock.close()
    root.quit()

def update_user_list(user_list_str):
    """Aktualizacja listy użytkowników dla aktualnego pokoju w interfejsie graficznym."""
    users = user_list_str.split("/")
    user_list.config(state=tk.NORMAL)
    user_list.delete("1.0", tk.END)
    for user in users:
        user = user.strip()
        if len(user) > 0:
            user_list.insert(tk.END, f"{user}\n")
    user_list.config(state=tk.DISABLED)

def display_message(message):
    """Wyświetlenie wiadomości w oknie czatu z wraz z aktulanym czasem."""
    timestamp = datetime.now().strftime("[%H:%M:%S]")
    formatted_message = f"{timestamp} {message}\n"
    message_box.config(state=tk.NORMAL)
    message_box.insert(tk.END, formatted_message)
    message_box.config(state=tk.DISABLED)
    message_box.see(tk.END)  # Automatyczne przewinięcie do końca

def send_message(event=None):
    """Wysyłanie wiadomości z pola entry do serwera (obsługa Enter i przycisku)."""
    message = entry.get()
    if message:
        try:
            sock.send(message.encode('utf-8'))
            entry.delete(0, tk.END)
        except:
            messagebox.showerror("Error", "Błąd zapisu do gniazda")

def on_closing():
    """
    Obsługa zamknięcia okna aplikacji.
    """
    try:
        sock.send("/exit".encode('utf-8'))
        sock.close()
    except:
        pass
    root.destroy()

# Funkcja do przełączania pełnoekranowego trybu
fullscreen_state = False
def toggle_fullscreen():
    global fullscreen_state
    fullscreen_state = not fullscreen_state
    root.attributes("-fullscreen", fullscreen_state)

# --- Konfiguracja interfejsu graficznego (tkinter) ---
root = tk.Tk()
root.title("IRC Communicator - GrajaZurek")

root.resizable(True, True)  # Okno może być zmieniane
root.configure(bg="#7C4DFF")

# Pole czatu
message_box = scrolledtext.ScrolledText(
    root, height=20, width=70, state=tk.DISABLED,
    bg="#3C3F41", fg="#DCDCDC", insertbackground="#DCDCDC",
    font=("Monospace", 10), selectbackground="#7C4DFF"
)
message_box.grid(row=0, column=0, padx=10, pady=10, columnspan=4, sticky="nsew")

# Lista użytkowników
user_list = scrolledtext.ScrolledText(
    root, height=20, width=20, state=tk.DISABLED,
    bg="#3C3F41", fg="#DCDCDC", insertbackground="#DCDCDC",
    font=("Monospace", 10), selectbackground="#7C4DFF"
)
user_list.grid(row=0, column=4, padx=10, pady=10, sticky="nsew")

root.grid_columnconfigure(0, weight=1)
root.grid_columnconfigure(4, weight=0)
root.grid_rowconfigure(0, weight=1)

# Pole do wpisywania wiadomości
entry = tk.Entry(
    root, width=70, bg="#3C3F41", fg="#DCDCDC",
    insertbackground="#DCDCDC", font=("Monospace", 10),
    selectbackground="#7C4DFF"
)
entry.grid(row=1, column=0, padx=10, pady=10, columnspan=3, sticky="ew")
entry.bind("<Return>", send_message)

# Przycisk "Send"
send_button = tk.Button(
    root, text="Send", command=send_message,
    bg="#555555", fg="#DCDCDC", activebackground="#7C4DFF",
    activeforeground="#2B2B2B", font=("Monospace", 10), width=6
)
send_button.grid(row=1, column=3, padx=10, pady=10, sticky="w")

# Przycisk "Fullscreen"
fullscreen_button = tk.Button(
    root, text="Fullscreen", command=toggle_fullscreen,
    bg="#555555", fg="#DCDCDC", activebackground="#7C4DFF",
    activeforeground="#2B2B2B", font=("Monospace", 10), width=10
)
fullscreen_button.grid(row=1, column=4, padx=10, pady=10, sticky="e")

# --- Konfiguracja gniazda do połączenia z serwerem ---
server_address = ("127.0.0.1", 5555)  # IP i port serwera
sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

try:
    sock.connect(server_address)
except socket.error as e:
    messagebox.showerror("Error", f"Error connecting to server: {e}")
    root.quit()

# Zaraz po połączeniu pytamy serwer o listę użytkowników w aktualnym pokoju (Lobby)
try:
    sock.send("/who".encode('utf-8'))
except:
    pass

# Wątek odbierający wiadomości
recv_thread = threading.Thread(target=receive_message, args=(sock,), daemon=True)
recv_thread.start()

# Obsługa kliknięcia "X"
root.protocol("WM_DELETE_WINDOW", on_closing)

root.mainloop()
