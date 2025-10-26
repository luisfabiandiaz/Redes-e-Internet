#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <algorithm>
#include <unordered_map>
#include <fstream>
#include <netdb.h> 
#include <errno.h> 
#include <map> 

using namespace std;

#define MAXLINE 1024

struct ClientInfo {
    string nickname;
    struct sockaddr_in addr;
};

unordered_map<string, ClientInfo> clients_by_addr_str;
unordered_map<string, ClientInfo> clients_by_nick;


vector<string> clientesJugando;
string simbolo = "o";
const int tamTablero = 3;
string tablero[tamTablero * tamTablero];
int movimientos = 0;


void reiniciarJuego();
bool revisarVictoria(const string& simbolo_jugador);
bool revisarEmpate();

string addr_to_string(const struct sockaddr_in& addr) {
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(addr.sin_addr), ip, INET_ADDRSTRLEN);
    return string(ip) + ":" + to_string(ntohs(addr.sin_port));
}

void reiniciarJuego() {
    for (int i = 0; i < tamTablero * tamTablero; i++) {
        tablero[i] = "_";
    }
    movimientos = 0;
    clientesJugando.clear();
    cout << "\nJuego reiniciado. Esperando nuevos jugadores." << endl;
}

bool revisarVictoria(const string& simbolo_jugador) {
    // filas
    for (int i = 0; i < tamTablero; ++i) {
        bool fila_completa = true;
        for (int j = 0; j < tamTablero; ++j) {
            if (tablero[i * tamTablero + j] != simbolo_jugador) {
                fila_completa = false;
                break;
            }
        }
        if (fila_completa) return true;
    }
    // columnas
    for (int i = 0; i < tamTablero; ++i) {
        bool col_completa = true;
        for (int j = 0; j < tamTablero; ++j) {
            if (tablero[j * tamTablero + i] != simbolo_jugador) {
                col_completa = false;
                break;
            }
        }
        if (col_completa) return true;
    }
    // diagonal principal
    bool diag1_completa = true;
    for (int i = 0; i < tamTablero; ++i) {
        if (tablero[i * tamTablero + i] != simbolo_jugador) {
            diag1_completa = false;
            break;
        }
    }
    if (diag1_completa) return true;
    // diagonal secundaria
    bool diag2_completa = true;
    for (int i = 0; i < tamTablero; ++i) {
        if (tablero[i * tamTablero + (tamTablero - 1 - i)] != simbolo_jugador) {
            diag2_completa = false;
            break;
        }
    }
    if (diag2_completa) return true;
    return false;
}

bool revisarEmpate() {
    return movimientos == tamTablero * tamTablero;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        cerr << "Uso: " << argv[0] << " <puerto>\n";
        return 1;
    }

    int puerto = atoi(argv[1]);
    reiniciarJuego();

    struct sockaddr_in servaddr, cliaddr;
    int servidor_sock;
    
    if ((servidor_sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Fallo al crear socket");
        exit(EXIT_FAILURE);
    }
    
    memset(&servaddr, 0, sizeof(servaddr));
    memset(&cliaddr, 0, sizeof(cliaddr));

    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(puerto);
    servaddr.sin_addr.s_addr = INADDR_ANY;
    
    if (bind(servidor_sock, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("error bind failed");
        close(servidor_sock);
        exit(EXIT_FAILURE);
    }

    cout << "Servidor UDP escuchando en puerto " << puerto << "...\n";

    char buffer[MAXLINE];
    socklen_t len_cliaddr = sizeof(cliaddr);


    while (true) {
        memset(buffer, 0, MAXLINE);
        int n = recvfrom(servidor_sock, buffer, MAXLINE, 0, (struct sockaddr *) &cliaddr, &len_cliaddr);

        if (n <= 0) {
            cerr << "Error en recvfrom" << endl;
            continue;
        }
        
        // <<< NOTA: Esta lógica es correcta, 'datagram' tendrá los '#' al final >>>
        string datagram(buffer, n);
        string client_addr_str = addr_to_string(cliaddr);
        string nickCliente;

        if (clients_by_addr_str.count(client_addr_str)) {
            nickCliente = clients_by_addr_str[client_addr_str].nickname;
        } else {
            nickCliente = "temp_" + client_addr_str;
            ClientInfo newClient;
            newClient.nickname = nickCliente;
            newClient.addr = cliaddr;
            clients_by_addr_str[client_addr_str] = newClient;
            clients_by_nick[nickCliente] = newClient;
            cout << "Nuevo cliente conectado: " << client_addr_str << " (asignado " << nickCliente << ")" << endl;
        }
        
        // <<< NOTA: Esta lógica de parseo ignora el relleno '#' gracias a los 'substr' con tamaño >>>
        cout << "\n[SERVER RECV]: " << datagram.substr(0, 70) 
             << (datagram.size() > 70 ? "..." : "") 
             << " (from " << nickCliente << ", size=" << n << ")" << endl;

        if (isdigit(datagram[0])) {
            char tipo_paquete = datagram[5]; // Debería ser 'f'

            if (tipo_paquete != 'f') {
                cerr << "Error: Paquete con seq pero no es tipo 'f'" << endl;
                continue;
            }

            int offset = 6; 
            int tamDest = stoi(datagram.substr(offset, 2));
            offset += 2;
            string dest = datagram.substr(offset, tamDest);

            if (clients_by_nick.count(dest)) {
                struct sockaddr_in dest_addr = clients_by_nick[dest].addr;
                
              
                cout << "[SERVER SEND]: Reenviando paquete (seq " << datagram.substr(0,2) << ") a " << dest << " (size=" << n << ")" << endl;
           
                // <<< NOTA: ESTE sendto() ES CORRECTO, datagram.size() YA ES MAXLINE >>>
                sendto(servidor_sock, datagram.c_str(), datagram.size(), 0, 
                       (struct sockaddr*)&dest_addr, sizeof(dest_addr));
            } else {
                 cout << "Error: Destinatario de archivo '" << dest << "' no encontrado." << endl;
            }
            
            continue; 
        }

        char tipo = datagram[0];

        if (tipo == 'n') {
        
            int tam = stoi(datagram.substr(1, 2));
            string newNick = datagram.substr(3, tam);
            string oldNick = clients_by_addr_str[client_addr_str].nickname;
            clients_by_nick.erase(oldNick);
            clients_by_addr_str[client_addr_str].nickname = newNick;
            clients_by_nick[newNick] = clients_by_addr_str[client_addr_str];
            cout << "\n" << oldNick << " ahora es " << newNick << endl;

        } else if (tipo == 'm') {
           
            int tamMsg = stoi(datagram.substr(1, 3));
            string msg = datagram.substr(4, tamMsg);
            int tamNick = nickCliente.size();
            string tamañoSource = (tamNick < 10) ? "0" + to_string(tamNick) : to_string(tamNick);
            string tamañoMsg = (tamMsg < 10) ? "00" + to_string(tamMsg) : (tamMsg < 100) ? "0" + to_string(tamMsg) : to_string(tamMsg);
            string enviar = 'M' + tamañoSource + nickCliente + tamañoMsg + msg;
            
            // <<< CAMBIO 1: Rellenar paquete de chat broadcast >>>
            enviar.append(MAXLINE - enviar.size(), '#');

            for (const auto& pair : clients_by_addr_str) {
                if (pair.first != client_addr_str) {
                    cout << "[SERVER SEND]: " << enviar.substr(0, 70) << "... (to " << pair.second.nickname << ")" << endl;
                    sendto(servidor_sock, enviar.c_str(), enviar.size(), 0,
                           (struct sockaddr*)&pair.second.addr, sizeof(pair.second.addr));
                }
            }
        } else if (tipo == 't') {
            int tamDest = stoi(datagram.substr(1, 2));
            string destination = datagram.substr(3, tamDest);
            int tamMsg = stoi(datagram.substr(3 + tamDest, 3));
            string msg = datagram.substr(3 + tamDest + 3, tamMsg);
            int tamNick = nickCliente.size();
            string tamañoSource = (tamNick < 10) ? "0" + to_string(tamNick) : to_string(tamNick);
            string tamañoMsg = (tamMsg < 10) ? "00" + to_string(tamMsg) : (tamMsg < 100) ? "0" + to_string(tamMsg) : to_string(tamMsg);
            string enviar = 'T' + tamañoSource + nickCliente + tamañoMsg + msg;
            
            // <<< CAMBIO 2: Rellenar paquete de chat privado >>>
            enviar.append(MAXLINE - enviar.size(), '#');
            
            if (clients_by_nick.count(destination)) {
                struct sockaddr_in dest_addr = clients_by_nick[destination].addr;
                cout << "[SERVER SEND]: " << enviar.substr(0, 70) << "... (to " << destination << ")" << endl;
                sendto(servidor_sock, enviar.c_str(), enviar.size(), 0,
                       (struct sockaddr*)&dest_addr, sizeof(dest_addr));
            }
        } else if (tipo == 'l') {
            string lista_clientes_str;
            for (const auto& pair : clients_by_nick) {
                string tamNick = (pair.first.size() < 10) ? "0" + to_string(pair.first.size()) : to_string(pair.first.size());
                lista_clientes_str += tamNick + pair.first;
            }
            string cantClientes = (clients_by_nick.size() < 10) ? "0" + to_string(clients_by_nick.size()) : to_string(clients_by_nick.size());
            string enviar = "L" + cantClientes + lista_clientes_str;
            
            // <<< CAMBIO 3: Rellenar paquete de lista de clientes >>>
            enviar.append(MAXLINE - enviar.size(), '#');
            
            cout << "[SERVER SEND]: " << enviar.substr(0, 70) << "... (to " << nickCliente << ")" << endl;
            sendto(servidor_sock, enviar.c_str(), enviar.size(), 0, (struct sockaddr*)&cliaddr, len_cliaddr);

        } else if (tipo == 'p') {
          
            if (find(clientesJugando.begin(), clientesJugando.end(), client_addr_str) != clientesJugando.end()) {
                continue;
            }
            if (clientesJugando.size() < 2) {
                clientesJugando.push_back(client_addr_str);
                cout << "\n" << nickCliente << " se unió a la partida." << endl;
                if (clientesJugando.size() == 2) {
                    cout << "La partida comienza." << endl;
                    
                    // <<< CAMBIO 4: Rellenar paquete de inicio de juego 'Vo' >>>
                    string enviar = "Vo";
                    enviar.append(MAXLINE - enviar.size(), '#');
                    
                    struct sockaddr_in player1_addr = clients_by_addr_str[clientesJugando[0]].addr;
                    cout << "[SERVER SEND]: " << enviar.substr(0, 70) << "... (to " << clients_by_addr_str[clientesJugando[0]].nickname << ")" << endl;
                    sendto(servidor_sock, enviar.c_str(), enviar.size(), 0,
                           (struct sockaddr*)&player1_addr, sizeof(player1_addr));
                }
            }
        } else if (tipo == 'w') {
          
            string simbolo_jugador = datagram.substr(1, 1);
            int pos = stoi(datagram.substr(2));
            
            if (pos >= 1 && pos <= tamTablero * tamTablero && tablero[pos-1] == "_") {
                tablero[pos - 1] = simbolo_jugador;
                movimientos++;
                
                // <<< CAMBIO 5: Rellenar paquete de estado de tablero 'v...' >>>
                string enviar_tablero = "v";
                for(int i = 0; i < tamTablero*tamTablero; ++i) enviar_tablero += tablero[i];
                enviar_tablero.append(MAXLINE - enviar_tablero.size(), '#');

                struct sockaddr_in p1_addr = clients_by_addr_str[clientesJugando[0]].addr;
                struct sockaddr_in p2_addr = clients_by_addr_str[clientesJugando[1]].addr;

                cout << "[SERVER SEND]: " << enviar_tablero.substr(0, 70) << "... (to player 1)" << endl;
                sendto(servidor_sock, enviar_tablero.c_str(), enviar_tablero.size(), 0, (struct sockaddr*)&p1_addr, sizeof(p1_addr));
                cout << "[SERVER SEND]: " << enviar_tablero.substr(0, 70) << "... (to player 2)" << endl;
                sendto(servidor_sock, enviar_tablero.c_str(), enviar_tablero.size(), 0, (struct sockaddr*)&p2_addr, sizeof(p2_addr));
                
                if (revisarVictoria(simbolo_jugador)) {
                    string ganador_addr_str = (simbolo_jugador == "o") ? clientesJugando[0] : clientesJugando[1];
                    string perdedor_addr_str = (simbolo_jugador == "o") ? clientesJugando[1] : clientesJugando[0];
                    struct sockaddr_in ganador_addr = clients_by_addr_str[ganador_addr_str].addr;
                    struct sockaddr_in perdedor_addr = clients_by_addr_str[perdedor_addr_str].addr;
                    
                    // <<< CAMBIO 6: Rellenar paquetes de fin de juego (Win/Los) >>>
                    string msg_win = "Owin";
                    msg_win.append(MAXLINE - msg_win.size(), '#');
                    string msg_los = "Olos";
                    msg_los.append(MAXLINE - msg_los.size(), '#');
                    
                    cout << "[SERVER SEND]: Owin... (to winner)" << endl;
                    sendto(servidor_sock, msg_win.c_str(), msg_win.size(), 0, (struct sockaddr*)&ganador_addr, sizeof(ganador_addr)); 
                    cout << "[SERVER SEND]: Olos... (to loser)" << endl;
                    sendto(servidor_sock, msg_los.c_str(), msg_los.size(), 0, (struct sockaddr*)&perdedor_addr, sizeof(perdedor_addr)); 
                    reiniciarJuego();
                } else if (revisarEmpate()) {
                    
                    // <<< CAMBIO 7: Rellenar paquete de fin de juego (Empate) >>>
                    string msg_emp = "Oemp";
                    msg_emp.append(MAXLINE - msg_emp.size(), '#');

                    cout << "[SERVER SEND]: Oemp... (to players)" << endl;
                    sendto(servidor_sock, msg_emp.c_str(), msg_emp.size(), 0, (struct sockaddr*)&p1_addr, sizeof(p1_addr)); 
                    sendto(servidor_sock, msg_emp.c_str(), msg_emp.size(), 0, (struct sockaddr*)&p2_addr, sizeof(p2_addr));
                    reiniciarJuego();
                } else {
                    
                    // <<< CAMBIO 8: Rellenar paquetes de cambio de turno (Vx/Vo) >>>
                    if (simbolo_jugador == "o") {
                        string msg_turn_x = "Vx";
                        msg_turn_x.append(MAXLINE - msg_turn_x.size(), '#');
                        cout << "[SERVER SEND]: Vx... (to player 2)" << endl;
                        sendto(servidor_sock, msg_turn_x.c_str(), msg_turn_x.size(), 0, (struct sockaddr*)&p2_addr, sizeof(p2_addr));
                    } else {
                        string msg_turn_o = "Vo";
                        msg_turn_o.append(MAXLINE - msg_turn_o.size(), '#');
                        cout << "[SERVER SEND]: Vo... (to player 1)" << endl;
                        sendto(servidor_sock, msg_turn_o.c_str(), msg_turn_o.size(), 0, (struct sockaddr*)&p1_addr, sizeof(p1_addr));
                    }
                }
            }
        } 
        
        else if (tipo == 'f') {
        
            cerr << "Advertencia: Se recibió un paquete 'f' simple (no fragmentado)." << endl;
        } 

        else if (tipo == 'x') {
         
            cout << "\n" << nickCliente << " se desconectó." << endl;
            clients_by_nick.erase(nickCliente);
            clients_by_addr_str.erase(client_addr_str);
            auto it_jugador = find(clientesJugando.begin(), clientesJugando.end(), client_addr_str);
            if (it_jugador != clientesJugando.end()) {
                 cout << "\nUn jugador abandonó la partida. Reiniciando el juego." << endl;
                 reiniciarJuego();
            }
        }
    }

    close(servidor_sock);
    return 0;
}