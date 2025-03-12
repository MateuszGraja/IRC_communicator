#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <ctype.h>
#include <arpa/inet.h>
#include <signal.h>
#include <errno.h>

#define MAX_CLIENTS 30
#define BUFFER_LEN 512
#define ROOM_NAME_LEN 50
#define USERNAME_LEN 32
#define DEFAULT_PORT 5555

typedef struct Client {
    int socket_fd;
    char username[USERNAME_LEN];
    char current_room[ROOM_NAME_LEN];
    int active;
    char creator_room[ROOM_NAME_LEN];
} Client;

typedef struct Room {
    char name[ROOM_NAME_LEN];
    int hidden;
    char creator[USERNAME_LEN];
    struct Room *next;
} Room;

// Global variables
Client clients[MAX_CLIENTS];
Room *room_list = NULL;
pthread_mutex_t clients_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t rooms_lock = PTHREAD_MUTEX_INITIALIZER;
int server_socket;

// ---------------- Funkcje Pomocnicze ---------------- //

// Funkcja obsługi sygnałów (np. Ctrl+C)
void handle_signal(int sig) {
    printf("\nZatrzymywanie serwera...\n");

    // Zamknięcie wszystkich gniazd klientów
    pthread_mutex_lock(&clients_lock);
    for(int i = 0; i < MAX_CLIENTS; i++) {
        if(clients[i].active) {
            close(clients[i].socket_fd);
        }
    }
    pthread_mutex_unlock(&clients_lock);

    // Zamknięcie gniazda serwera
    close(server_socket);

    // Zwolnienie pamięci przydzielonej dla pokojów
    pthread_mutex_lock(&rooms_lock);
    Room *temp = room_list;
    while(temp != NULL) {
        Room *to_free = temp;
        temp = temp->next;
        free(to_free);
    }
    pthread_mutex_unlock(&rooms_lock);

    printf("Serwer zatrzymany.\n");
    exit(EXIT_SUCCESS);
}

// Funkcja sprawdzająca, czy podany nick jest już używany przez kogoś na serwerze
int is_username_taken(const char *name) {
    for(int i = 0; i < MAX_CLIENTS; i++) {
        if(clients[i].active && strcmp(clients[i].username, name) == 0) {
            return 1;
        }
    }
    return 0;
}

// ---------------- Zarządzanie Pokojami ---------------- //

// Funkcja do dodawania nowego pokoju
void add_room(const char *room_name, int hidden, const char *creator) {
    Room *new_room = (Room*)malloc(sizeof(Room));
    strncpy(new_room->name, room_name, ROOM_NAME_LEN);
    new_room->hidden = hidden;
    strncpy(new_room->creator, creator, USERNAME_LEN);
    new_room->next = NULL;

    pthread_mutex_lock(&rooms_lock);
    if(room_list == NULL) {
        room_list = new_room;
    } else {
        Room *ptr = room_list;
        while(ptr->next != NULL) {
            ptr = ptr->next;
        }
        ptr->next = new_room;
    }
    pthread_mutex_unlock(&rooms_lock);

    printf("Pokój '%s' został utworzony przez '%s'%s.\n", room_name, creator, hidden ? " (ukryty)" : "");
}

// Funkcja do sprawdzania aktualnych użytkowników w pokoju
void who_in_room(int client_index) {
    char response[BUFFER_LEN];
    snprintf(response, sizeof(response), "USERLIST/"); 

    pthread_mutex_lock(&clients_lock);
    const char *currentRoom = clients[client_index].current_room;
    for(int i = 0; i < MAX_CLIENTS; i++) {
        if(clients[i].active && strcmp(clients[i].current_room, currentRoom) == 0) {
            // Dopisujemy nick do odpowiedzi
            strncat(response, clients[i].username, sizeof(response) - strlen(response) - 1);
            strncat(response, "/", sizeof(response) - strlen(response) - 1);
        }
    }
    pthread_mutex_unlock(&clients_lock);

    // Wysyłamy wynik do konkretnego klienta
    send(clients[client_index].socket_fd, response, strlen(response), 0);
}

// Funkcja do sprawdzania istnienia pokoju
int is_room_exist(const char *room_name) {
    int exists = 0;
    pthread_mutex_lock(&rooms_lock);
    Room *ptr = room_list;
    while(ptr != NULL) {
        if(strcmp(ptr->name, room_name) == 0) {
            exists = 1;
            break;
        }
        ptr = ptr->next;
    }
    pthread_mutex_unlock(&rooms_lock);
    return exists;
}

// Funkcja do usuwania pokoju
int delete_room(const char *room_name, const char *requester) {
    pthread_mutex_lock(&rooms_lock);
    Room *ptr = room_list;
    Room *prev = NULL;
    while(ptr != NULL) {
        if(strcmp(ptr->name, room_name) == 0) {
            if(strcmp(ptr->creator, requester) != 0) {
                pthread_mutex_unlock(&rooms_lock);
                return -1; // Tylko twórca może usunąć pokój
            }
            // Usunięcie pokoju z listy
            if(prev == NULL) {
                room_list = ptr->next;
            } else {
                prev->next = ptr->next;
            }
            free(ptr);
            pthread_mutex_unlock(&rooms_lock);
            printf("Pokój '%s' został usunięty przez '%s'.\n", room_name, requester);
            return 0;
        }
        prev = ptr;
        ptr = ptr->next;
    }
    pthread_mutex_unlock(&rooms_lock);
    return -2; // Pokój nie istnieje
}

// ---------------- Zarządzanie Klientami ---------------- //

// Dodawanie unikalnych nazw urzytkowników w stylu "Anon1, Anon2..."
void assign_unique_anon_nick(int client_index) {
    char base_nick[] = "Anon";
    char tentative_name[USERNAME_LEN];
    int counter = 1;

    // Próba aż do znalezienia wolnego nicku
    while(1) {
        snprintf(tentative_name, sizeof(tentative_name), "%s%d", base_nick, counter++);
        if(!is_username_taken(tentative_name)) {
            break;
        }
    }

    // Ustawienie wybranego nicku
    strncpy(clients[client_index].username, tentative_name, USERNAME_LEN);
    clients[client_index].username[USERNAME_LEN - 1] = '\0';
    printf("Nadano unikalny domyślny nick: %s\n", clients[client_index].username);
}

// Funkcja do dodawania klienta do listy
int add_client(int client_socket) {
    pthread_mutex_lock(&clients_lock);
    for(int i = 0; i < MAX_CLIENTS; i++) {
        if(!clients[i].active) {
            clients[i].socket_fd = client_socket;

            // Przypisanie domyślnego pokoju
            strncpy(clients[i].current_room, "Lobby", ROOM_NAME_LEN);
            clients[i].active = 1;
            clients[i].creator_room[0] = '\0';

            // Nadanie unikalnego nicku
            assign_unique_anon_nick(i);

            pthread_mutex_unlock(&clients_lock);
            return i;
        }
    }
    pthread_mutex_unlock(&clients_lock);
    return -1;
}

// Funkcja do usuwania klienta z listy
void remove_client(int index) {
    pthread_mutex_lock(&clients_lock);
    close(clients[index].socket_fd);
    clients[index].active = 0;
    pthread_mutex_unlock(&clients_lock);
}

// Funkcja do zmiany nicku
int change_username(int index, const char *new_name) {
    // Sprawdzenie unikalności nicku
    if(is_username_taken(new_name)) {
        return -1; // Nick zajęty
    }
    if(strcmp(new_name, "System") == 0) {
        return -2; //Zablokowanie możliwości ustawienia nicku jako System!!
    }

    pthread_mutex_lock(&clients_lock);
    strncpy(clients[index].username, new_name, USERNAME_LEN);
    clients[index].username[USERNAME_LEN - 1] = '\0';
    pthread_mutex_unlock(&clients_lock);
    return 0;
}

// ---------------- Funkcje Komunikacyjne ---------------- //

// Funkcja do wysyłania wiadomości do wszystkich w pokoju
void send_message_to_room(const char *message, const char *room_name) {
    pthread_mutex_lock(&clients_lock);
    for(int i = 0; i < MAX_CLIENTS; i++) {
        if(clients[i].active && strcmp(clients[i].current_room, room_name) == 0) {
            send(clients[i].socket_fd, message, strlen(message), 0);
        }
    }
    pthread_mutex_unlock(&clients_lock);
}

// Funkcja do dołączania do pokoju
void join_room(int index, const char *room_name) {
    char notification[BUFFER_LEN];
    // Powiadomienie o opuszczeniu poprzedniego pokoju
    snprintf(notification, sizeof(notification), "%s opuścił pokój %s.\n",
             clients[index].username, clients[index].current_room);
    send_message_to_room(notification, clients[index].current_room);

    // Dołączenie do nowego pokoju
    strncpy(clients[index].current_room, room_name, ROOM_NAME_LEN);
    clients[index].current_room[ROOM_NAME_LEN - 1] = '\0';
    snprintf(notification, sizeof(notification), "%s dołączył do pokoju %s.\n",
             clients[index].username, room_name);
    send_message_to_room(notification, room_name);

    printf("Użytkownik '%s' dołączył do pokoju '%s'.\n",
           clients[index].username, room_name);
}

// Funkcja do opuszczania aktualnego pokoju (przeniesienie do Lobby)
void leave_room(int index) {
    join_room(index, "Lobby");
}

// Funkcja do listowania pokojów wraz z liczbą użytkowników
void list_rooms(int client_socket) {
    char list[BUFFER_LEN] = "Dostępne pokoje:\n";
    pthread_mutex_lock(&rooms_lock);
    Room *ptr = room_list;
    while(ptr != NULL) {
        if(!ptr->hidden) {
            // Liczenie użytkowników w pokoju
            int count = 0;
            pthread_mutex_lock(&clients_lock);
            for(int i = 0; i < MAX_CLIENTS; i++) {
                if(clients[i].active && strcmp(clients[i].current_room, ptr->name) == 0) {
                    count++;
                }
            }
            pthread_mutex_unlock(&clients_lock);
            char room_info[ROOM_NAME_LEN + 30];
            snprintf(room_info, sizeof(room_info), "%s (%d użytkowników)\n", ptr->name, count);
            strcat(list, room_info);
        }
        ptr = ptr->next;
    }
    pthread_mutex_unlock(&rooms_lock);
    send(client_socket, list, strlen(list), 0);
}

// ---------------- Wątek obsługujący klienta ---------------- //

void *client_thread(void *arg) {
    int idx = *(int*)arg;
    free(arg);
    char buffer[BUFFER_LEN];
    int bytes;

    // Powitanie klienta
    send(clients[idx].socket_fd, "Witaj na serwerze IRC!\n", 23, 0);
    printf("Użytkownik '%s' połączony i dołączony do pokoju '%s'.\n",
           clients[idx].username, clients[idx].current_room);

    // Powiadomienie innych w pokoju
    char join_msg[BUFFER_LEN];
    snprintf(join_msg, sizeof(join_msg), "%s dołączył do pokoju %s.\n",
             clients[idx].username, clients[idx].current_room);
    send_message_to_room(join_msg, clients[idx].current_room);

    while((bytes = recv(clients[idx].socket_fd, buffer, sizeof(buffer)-1, 0)) > 0) {
        buffer[bytes] = '\0';
        // Usunięcie znaku nowej linii, jeśli występuje
        if(buffer[bytes-1] == '\n') buffer[bytes-1] = '\0';

        if(buffer[0] == '/') {
            // Obsługa komend
            char *command = strtok(buffer, " ");
            if(strcmp(command, "/nick") == 0) {
                char *new_nick = strtok(NULL, " ");
                if(new_nick) {
                    if(change_username(idx, new_nick) == 0) {
                        char success_msg[BUFFER_LEN];
                        snprintf(success_msg, sizeof(success_msg),
                                 "Twój nick został zmieniony na %s.\n", new_nick);
                        send(clients[idx].socket_fd, success_msg, strlen(success_msg), 0);

                        printf("Użytkownik zmienił nick na '%s'.\n", new_nick);
                        // Powiadomienie w pokoju
                        char notify[BUFFER_LEN];
                        snprintf(notify, sizeof(notify), "Użytkownik zmienił nick na %s.\n", new_nick);
                        send_message_to_room(notify, clients[idx].current_room);
                    } else {
                        char error_msg[] = "Nick jest już zajęty.\n";
                        send(clients[idx].socket_fd, error_msg, strlen(error_msg), 0);
                    }
                }
            }
            else if(strcmp(command, "/join") == 0) {
                char *room = strtok(NULL, " ");
                if(room && is_room_exist(room)) {
                    join_room(idx, room);
                } else {
                    char error_msg[] = "Pokój nie istnieje.\n";
                    send(clients[idx].socket_fd, error_msg, strlen(error_msg), 0);
                }
            }
            else if(strcmp(command, "/leave") == 0) {
                leave_room(idx);
            }
            else if(strcmp(command, "/exit") == 0) {
                break;
            }
            else if(strcmp(command, "/who") == 0) {
                who_in_room(idx);
            }
            else if(strcmp(command, "/list") == 0) {
                list_rooms(clients[idx].socket_fd);
            }
            else if(strcmp(command, "/create") == 0 || strcmp(command, "/create_secret") == 0) {
                char *room_name = strtok(NULL, " ");
                if(room_name) {
                    int hidden = (strcmp(command, "/create_secret") == 0) ? 1 : 0;
                    if(!is_room_exist(room_name)) {
                        add_room(room_name, hidden, clients[idx].username);
                        join_room(idx, room_name);
                        // Oznaczenie pokoju jako stworzony przez tego użytkownika
                        strncpy(clients[idx].creator_room, room_name, ROOM_NAME_LEN);
                        char create_msg[BUFFER_LEN];
                        snprintf(create_msg, sizeof(create_msg), "Pokój '%s' został utworzony.\n", room_name);
                        send(clients[idx].socket_fd, create_msg, strlen(create_msg), 0);
                    } else {
                        char error_msg[] = "Pokój o takiej nazwie już istnieje.\n";
                        send(clients[idx].socket_fd, error_msg, strlen(error_msg), 0);
                    }
                }
            }
            else if(strcmp(command, "/delete") == 0) {
                char *room_to_delete = strtok(NULL, " ");
                if(room_to_delete) {
                    // Sprawdzenie, czy użytkownik jest twórcą pokoju
                    pthread_mutex_lock(&rooms_lock);
                    Room *ptr = room_list;
                    int is_creator = 0;
                    while(ptr != NULL) {
                        if(strcmp(ptr->name, room_to_delete) == 0) {
                            if(strcmp(ptr->creator, clients[idx].username) == 0) {
                                is_creator = 1;
                            }
                            break;
                        }
                        ptr = ptr->next;
                    }
                    pthread_mutex_unlock(&rooms_lock);

                    if(is_creator) {
                        int del_status = delete_room(room_to_delete, clients[idx].username);
                        if(del_status == 0) {
                            // Przeniesienie użytkowników do Lobby
                            // Zbieranie informacji o przeniesionych użytkownikach
                            pthread_mutex_lock(&clients_lock);
                            char notify_lobby_messages[MAX_CLIENTS][BUFFER_LEN];
                            int notify_count = 0;
                            for(int i = 0; i < MAX_CLIENTS; i++) {
                                if(clients[i].active && strcmp(clients[i].current_room, room_to_delete) == 0) {
                                    strncpy(clients[i].current_room, "Lobby", ROOM_NAME_LEN);
                                    clients[i].current_room[ROOM_NAME_LEN - 1] = '\0';
                                    char move_msg[BUFFER_LEN];
                                    snprintf(move_msg, sizeof(move_msg), "Zostałeś przeniesiony do pokoju Lobby.\n");
                                    send(clients[i].socket_fd, move_msg, strlen(move_msg), 0);
                                    // Zbierz powiadomienia do wysłania do Lobby
                                    snprintf(notify_lobby_messages[notify_count], sizeof(notify_lobby_messages[notify_count]),
                                             "%s został przeniesiony do Lobby.\n", clients[i].username);
                                    notify_count++;
                                }
                            }
                            pthread_mutex_unlock(&clients_lock);

                            // Wyślij powiadomienia do Lobby
                            for(int i = 0; i < notify_count; i++) {
                                send_message_to_room(notify_lobby_messages[i], "Lobby");
                            }

                            char success_msg[] = "Pokój został pomyślnie usunięty.\n";
                            send(clients[idx].socket_fd, success_msg, strlen(success_msg), 0);
                        }
                    }
                    else {
                        char error_msg[] = "Nie masz uprawnień do usunięcia tego pokoju.\n";
                        send(clients[idx].socket_fd, error_msg, strlen(error_msg), 0);
                    }
                }
            }
        }
        else {
            // Przetwarzanie wiadomości (zwykły tekst)
            char message[4096];
            snprintf(message, sizeof(message), "%s: %s\n", clients[idx].username, buffer);
            send_message_to_room(message, clients[idx].current_room);
            printf("Wiadomość od '%s' w pokoju '%s': %s\n",
                   clients[idx].username, clients[idx].current_room, buffer);
        }
    }

    // Obsługa rozłączenia klienta
    snprintf(buffer, sizeof(buffer), "%s opuścił pokój %s.\n",
             clients[idx].username, clients[idx].current_room);
    send_message_to_room(buffer, clients[idx].current_room);
    printf("Użytkownik '%s' rozłączony.\n", clients[idx].username);
    remove_client(idx);
    close(clients[idx].socket_fd);
    pthread_exit(NULL);
}

// ---------------- Funkcja główna ---------------- //

int main(int argc, char *argv[]) {
    int port = DEFAULT_PORT;
    if(argc == 2) {
        port = atoi(argv[1]);
    }

    // Obsługa sygnałów
    signal(SIGINT, handle_signal);

    // Inicjalizacja gniazda serwera
    if((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Błąd tworzenia gniazda");
        exit(EXIT_FAILURE);
    }

    // Ustawienie opcji SO_REUSEADDR
    int opt = 1;
    if(setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("Błąd ustawiania opcji SO_REUSEADDR");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    // Powiązanie gniazda
    if(bind(server_socket, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Błąd bind");
        exit(EXIT_FAILURE);
    }

    // Nasłuchiwanie na gnieździe
    if(listen(server_socket, 10) < 0) {
        perror("Błąd listen");
        exit(EXIT_FAILURE);
    }

    printf("Serwer IRC działa na porcie %d.\n", port);

    // Dodanie domyślnego pokoju Lobby
    add_room("Lobby", 0, "System");

    while(1) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int new_socket = accept(server_socket, (struct sockaddr *)&client_addr, &addr_len);
        if(new_socket < 0) {
            perror("Błąd accept");
            continue;
        }

        // Dodanie klienta do listy
        int client_index = add_client(new_socket);
        if(client_index == -1) {
            char *msg = "Serwer jest pełny. Spróbuj później.\n";
            send(new_socket, msg, strlen(msg), 0);
            close(new_socket);
            continue;
        }

        // Tworzenie wątku dla klienta
        pthread_t tid;
        int *pclient = malloc(sizeof(int));
        if(pclient == NULL) {
            perror("Błąd alokacji pamięci");
            remove_client(client_index);
            continue;
        }
        *pclient = client_index;
        if(pthread_create(&tid, NULL, client_thread, pclient) != 0) {
            perror("Błąd tworzenia wątku");
            remove_client(client_index);
            free(pclient);
            continue;
        }
        pthread_detach(tid);
    }

    return 0;
}