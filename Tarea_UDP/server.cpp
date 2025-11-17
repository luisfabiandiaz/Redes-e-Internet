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

#define BUFFER_SIZE 777
unordered_map<string, string> apodo_por_dir; 

unordered_map<string, struct sockaddr_in> dir_por_apodo;


vector<string> jugadores_activos; 
string simbolo_base = "o";
const int DIM_TABLERO = 3;
string estado_tablero[DIM_TABLERO * DIM_TABLERO];
int contador_jugadas = 0;


void preparar_nuevo_juego();
bool verificar_ganador(const string& simbolo_actual);
bool verificar_empate();

string direccion_a_texto(const struct sockaddr_in& addr) {
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(addr.sin_addr), ip, INET_ADDRSTRLEN);
    return string(ip) + ":" + to_string(ntohs(addr.sin_port));
}

void preparar_nuevo_juego() {
    for (int i = 0; i < DIM_TABLERO * DIM_TABLERO; i++) {
        estado_tablero[i] = "_";
    }
    contador_jugadas = 0;
    jugadores_activos.clear();
}

bool verificar_ganador(const string& simbolo_actual) {
    for (int i = 0; i < DIM_TABLERO; ++i) {
        bool fila_completa = true;
        for (int j = 0; j < DIM_TABLERO; ++j) {
            if (estado_tablero[i * DIM_TABLERO + j] != simbolo_actual) {
                fila_completa = false;
                break;
            }
        }
        if (fila_completa) return true;
    }
    for (int i = 0; i < DIM_TABLERO; ++i) {
        bool col_completa = true;
        for (int j = 0; j < DIM_TABLERO; ++j) {
            if (estado_tablero[j * DIM_TABLERO + i] != simbolo_actual) {
                col_completa = false;
                break;
            }
        }
        if (col_completa) return true;
    }
    bool diag1_completa = true;
    for (int i = 0; i < DIM_TABLERO; ++i) {
        if (estado_tablero[i * DIM_TABLERO + i] != simbolo_actual) {
            diag1_completa = false;
            break;
        }
    }
    if (diag1_completa) return true;
    bool diag2_completa = true;
    for (int i = 0; i < DIM_TABLERO; ++i) {
        if (estado_tablero[i * DIM_TABLERO + (DIM_TABLERO - 1 - i)] != simbolo_actual) {
            diag2_completa = false;
            break;
        }
    }
    if (diag2_completa) return true;
    return false;
}

bool verificar_empate() {
    return contador_jugadas == DIM_TABLERO * DIM_TABLERO;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        cerr << "Modo de uso: " << argv[0] << " <puerto_servidor>\n";
        return 1;
    }

    int puerto_servidor = atoi(argv[1]);
    preparar_nuevo_juego();

    struct sockaddr_in dir_servidor, dir_cliente;
    int socket_servidor;
    
    socket_servidor = socket(AF_INET, SOCK_DGRAM, 0);
    
    memset(&dir_servidor, 0, sizeof(dir_servidor));
    memset(&dir_cliente, 0, sizeof(dir_cliente));

    dir_servidor.sin_family = AF_INET;
    dir_servidor.sin_port = htons(puerto_servidor);
    dir_servidor.sin_addr.s_addr = INADDR_ANY;
    
    bind(socket_servidor, (const struct sockaddr *)&dir_servidor, sizeof(dir_servidor));

    char bufer_recepcion[BUFFER_SIZE];
    socklen_t longitud_dir_cliente = sizeof(dir_cliente);

    while (true) {
        memset(bufer_recepcion, 0, BUFFER_SIZE);
        int bytes_recibidos = recvfrom(socket_servidor, bufer_recepcion, BUFFER_SIZE, 0, (struct sockaddr *) &dir_cliente, &longitud_dir_cliente);

        if (bytes_recibidos <= 0) {
            cerr << "Fallo al recibir datos (recvfrom)" << endl;
            continue;
        }
        string paquete_recibido(bufer_recepcion, bytes_recibidos);
        string dir_cliente_texto = direccion_a_texto(dir_cliente);
        string apodo_cliente_actual;

        if (apodo_por_dir.count(dir_cliente_texto)) {
            apodo_cliente_actual = apodo_por_dir[dir_cliente_texto];
        } 
        else {
            apodo_cliente_actual = "temp_" + dir_cliente_texto;
            apodo_por_dir[dir_cliente_texto] = apodo_cliente_actual;
            dir_por_apodo[apodo_cliente_actual] = dir_cliente;
        }
        
        
        cout << "\nserv recv: " << paquete_recibido.substr(0, 70) << "..." << endl;

        if (isdigit(paquete_recibido[0])) {
            
            char tipo_fragmento = paquete_recibido[5];

            if (tipo_fragmento != 'f' && tipo_fragmento != 'e') { 
                cerr << "Error: Paquete fragmentado no es tipo 'f' o 'e'" << endl;
                continue;
            }

            int offset = 6; 
            int longitud_destino = stoi(paquete_recibido.substr(offset, 2));
            offset += 2;
            string apodo_destino = paquete_recibido.substr(offset, longitud_destino);
            offset += longitud_destino;
            
            if (dir_por_apodo.count(apodo_destino)) {
                struct sockaddr_in direccion_destino = dir_por_apodo[apodo_destino];
                
                string longitud_origen = (apodo_cliente_actual.size() < 10) ? "0" + to_string(apodo_cliente_actual.size()) : to_string(apodo_cliente_actual.size());
                
                string tipo_para_receptor = (tipo_fragmento == 'e') ? "E" : "F";

                string paquete_a_enviar = paquete_recibido.substr(0,5) + tipo_para_receptor + longitud_origen + apodo_cliente_actual + paquete_recibido.substr(offset, BUFFER_SIZE - offset);
                
                cout << "serv envio: " << paquete_a_enviar.substr(0,70) << "..." << endl;
                
                sendto(socket_servidor, paquete_a_enviar.c_str(), paquete_a_enviar.size(), 0, 
                       (struct sockaddr*)&direccion_destino, sizeof(direccion_destino));
            } else {
                 cout << "Error: No se encontró al destinatario del archivo '" << apodo_destino << "'." << endl;
            }
            continue; 
        }

        char tipo_paquete = paquete_recibido[0];

        if (tipo_paquete == 'A') {
            int offset = 6;
            int longitud_destino = stoi(paquete_recibido.substr(offset, 2));
            offset += 2;
            string apodo_destino = paquete_recibido.substr(offset, longitud_destino);

            if (dir_por_apodo.count(apodo_destino)) {
                struct sockaddr_in direccion_destino = dir_por_apodo[apodo_destino];
                
                string longitud_origen = (apodo_cliente_actual.size() < 10) ? "0" + to_string(apodo_cliente_actual.size()) : to_string(apodo_cliente_actual.size());
                
                string paquete_a_enviar = paquete_recibido.substr(0, 6) + longitud_origen + apodo_cliente_actual;
                paquete_a_enviar.append(BUFFER_SIZE - paquete_a_enviar.size(), '#');

                cout << "serv envio (ACK): " << paquete_a_enviar.substr(0,70) << "..." << endl;
                
                sendto(socket_servidor, paquete_a_enviar.c_str(), paquete_a_enviar.size(), 0, 
                       (struct sockaddr*)&direccion_destino, sizeof(direccion_destino));
            } else {
                 cout << "Error: No se encontró al destinatario del ACK '" << apodo_destino << "'." << endl;
            }
            continue; 
        }

        if (tipo_paquete == 'A') {
            int offset = 6; 
            int longitud_destino = stoi(paquete_recibido.substr(offset, 2));
            offset += 2;
            string apodo_destino = paquete_recibido.substr(offset, longitud_destino);

            if (dir_por_apodo.count(apodo_destino)) {
                struct sockaddr_in direccion_destino = dir_por_apodo[apodo_destino];
                
                string longitud_origen = (apodo_cliente_actual.size() < 10) ? "0" + to_string(apodo_cliente_actual.size()) : to_string(apodo_cliente_actual.size());
                
                string paquete_a_enviar = paquete_recibido.substr(0, 6) + longitud_origen + apodo_cliente_actual;
                paquete_a_enviar.append(BUFFER_SIZE - paquete_a_enviar.size(), '#');

                cout << "serv envio (ACK): " << paquete_a_enviar.substr(0,70) << "..." << endl;
                
                sendto(socket_servidor, paquete_a_enviar.c_str(), paquete_a_enviar.size(), 0, 
                       (struct sockaddr*)&direccion_destino, sizeof(direccion_destino));
            } else {
                 cout << "Error: No se encontró al destinatario del ACK '" << apodo_destino << "'." << endl;
            }
            continue; 
        }
        else if (tipo_paquete == 'n') { 
            int longitud_nuevo_apodo = stoi(paquete_recibido.substr(1, 2));
            string nuevo_apodo = paquete_recibido.substr(3, longitud_nuevo_apodo);
            string apodo_anterior = apodo_por_dir[dir_cliente_texto];
            apodo_por_dir[dir_cliente_texto] = nuevo_apodo;
            struct sockaddr_in direccion_cliente_actual = dir_por_apodo[apodo_anterior];
            dir_por_apodo.erase(apodo_anterior);
            dir_por_apodo[nuevo_apodo] = direccion_cliente_actual;
        } else if (tipo_paquete == 'm') {
            int longitud_mensaje = stoi(paquete_recibido.substr(1, 3));
            string contenido_mensaje = paquete_recibido.substr(4, longitud_mensaje);
            int tamNick = apodo_cliente_actual.size();
            string longitud_origen = (tamNick < 10) ? "0" + to_string(tamNick) : to_string(tamNick);
            string longitud_mensaje_str = (longitud_mensaje < 10) ? "00" + to_string(longitud_mensaje) : (longitud_mensaje < 100) ? "0" + to_string(longitud_mensaje) : to_string(longitud_mensaje);
            string paquete_a_enviar = 'M' + longitud_origen + apodo_cliente_actual + longitud_mensaje_str + contenido_mensaje;
            paquete_a_enviar.append(BUFFER_SIZE - paquete_a_enviar.size(), '#');
            for (const auto& par : dir_por_apodo) {
                if (par.first != apodo_cliente_actual) { 
                    cout << "serv envio: " << paquete_a_enviar.substr(0, 40) << "... (a " << par.first << ")" << endl;
                    sendto(socket_servidor, paquete_a_enviar.c_str(), paquete_a_enviar.size(), 0,
                           (struct sockaddr*)&par.second, sizeof(par.second));
                }
            }
        } else if (tipo_paquete == 't') { 
            int longitud_destino = stoi(paquete_recibido.substr(1, 2));
            string apodo_destino = paquete_recibido.substr(3, longitud_destino);
            int longitud_mensaje = stoi(paquete_recibido.substr(3 + longitud_destino, 3));
            string contenido_mensaje = paquete_recibido.substr(3 + longitud_destino + 3, longitud_mensaje);
            int tamNick = apodo_cliente_actual.size();
            string longitud_origen = (tamNick < 10) ? "0" + to_string(tamNick) : to_string(tamNick);
            string longitud_mensaje_str = (longitud_mensaje < 10) ? "00" + to_string(longitud_mensaje) : (longitud_mensaje < 100) ? "0" + to_string(longitud_mensaje) : to_string(longitud_mensaje);
            string paquete_a_enviar = 'T' + longitud_origen + apodo_cliente_actual + longitud_mensaje_str + contenido_mensaje;
            paquete_a_enviar.append(BUFFER_SIZE - paquete_a_enviar.size(), '#'); 
            if (dir_por_apodo.count(apodo_destino)) {
                struct sockaddr_in direccion_destino = dir_por_apodo[apodo_destino];
                cout << "serv envio: " << paquete_a_enviar.substr(0, 40) << "... (a " << apodo_destino << ")" << endl;
                sendto(socket_servidor, paquete_a_enviar.c_str(), paquete_a_enviar.size(), 0,
                       (struct sockaddr*)&direccion_destino, sizeof(direccion_destino));
            }
        } else if (tipo_paquete == 'l') {
            string lista_nombres;
            for (const auto& par : dir_por_apodo) {
                string tamNick = (par.first.size() < 10) ? "0" + to_string(par.first.size()) : to_string(par.first.size());
                lista_nombres += tamNick + par.first;
            }
            string total_clientes_str = (dir_por_apodo.size() < 10) ? "0" + to_string(dir_por_apodo.size()) : to_string(dir_por_apodo.size());
            string paquete_a_enviar = "L" + total_clientes_str + lista_nombres;
            paquete_a_enviar.append(BUFFER_SIZE - paquete_a_enviar.size(), '#');
            cout << "serv envio: " << paquete_a_enviar.substr(0, 40) << "... (a " << apodo_cliente_actual << ")" << endl;
            sendto(socket_servidor, paquete_a_enviar.c_str(), paquete_a_enviar.size(), 0, (struct sockaddr*)&dir_cliente, longitud_dir_cliente);
        } else if (tipo_paquete == 'p') { 
            if (find(jugadores_activos.begin(), jugadores_activos.end(), dir_cliente_texto) != jugadores_activos.end()) {
                continue;
            }
            if (jugadores_activos.size() < 2) {
                jugadores_activos.push_back(dir_cliente_texto); 
                cout << "\n" << apodo_cliente_actual << " ha entrado al juego." << endl;
                if (jugadores_activos.size() == 2) {
                    cout << "Iniciando el juego." << endl;
                    string paquete_a_enviar = "Vo";
                    paquete_a_enviar.append(BUFFER_SIZE - paquete_a_enviar.size(), '#');
                    string apodo_j1 = apodo_por_dir[jugadores_activos[0]];
                    struct sockaddr_in dir_jugador1 = dir_por_apodo[apodo_j1];
                    cout << "serv envio: " << paquete_a_enviar.substr(0, 40) << "... (a " << apodo_j1 << ")" << endl;
                    sendto(socket_servidor, paquete_a_enviar.c_str(), paquete_a_enviar.size(), 0,
                           (struct sockaddr*)&dir_jugador1, sizeof(dir_jugador1));
                }
            }
        } else if (tipo_paquete == 'w') { 
            string simbolo_recibido = paquete_recibido.substr(1, 1);
            int posicion_jugada = stoi(paquete_recibido.substr(2));
            if (posicion_jugada >= 1 && posicion_jugada <= DIM_TABLERO * DIM_TABLERO && estado_tablero[posicion_jugada-1] == "_") {
                estado_tablero[posicion_jugada - 1] = simbolo_recibido;
                contador_jugadas++;
                string paquete_tablero = "v"; 
                for(int i = 0; i < DIM_TABLERO * DIM_TABLERO; ++i) paquete_tablero += estado_tablero[i];
                paquete_tablero.append(BUFFER_SIZE - paquete_tablero.size(), '#');
                string apodo_j1 = apodo_por_dir[jugadores_activos[0]];
                string apodo_j2 = apodo_por_dir[jugadores_activos[1]];
                struct sockaddr_in dir_jugador1 = dir_por_apodo[apodo_j1];
                struct sockaddr_in dir_jugador2 = dir_por_apodo[apodo_j2];
                cout << "serv envio: " << paquete_tablero.substr(0, 40) << "... (a jugador 1)" << endl;
                sendto(socket_servidor, paquete_tablero.c_str(), paquete_tablero.size(), 0, (struct sockaddr*)&dir_jugador1, sizeof(dir_jugador1));
                cout << "serv envio: " << paquete_tablero.substr(0, 40) << "... (a jugador 2)" << endl;
                sendto(socket_servidor, paquete_tablero.c_str(), paquete_tablero.size(), 0, (struct sockaddr*)&dir_jugador2, sizeof(dir_jugador2));
                if (verificar_ganador(simbolo_recibido)) {
                    string dir_str_ganador = (simbolo_recibido == "o") ? jugadores_activos[0] : jugadores_activos[1];
                    string dir_str_perdedor = (simbolo_recibido == "o") ? jugadores_activos[1] : jugadores_activos[0];
                    string apodo_ganador = apodo_por_dir[dir_str_ganador];
                    string apodo_perdedor = apodo_por_dir[dir_str_perdedor];
                    struct sockaddr_in dir_ganador = dir_por_apodo[apodo_ganador];
                    struct sockaddr_in dir_perdedor = dir_por_apodo[apodo_perdedor];
                    string paquete_victoria = "Owin";
                    paquete_victoria.append(BUFFER_SIZE - paquete_victoria.size(), '#');
                    string paquete_derrota = "Olos";
                    paquete_derrota.append(BUFFER_SIZE - paquete_derrota.size(), '#');
                    cout << "serv envio: Owin... (a ganador)" << endl;
                    sendto(socket_servidor, paquete_victoria.c_str(), paquete_victoria.size(), 0, (struct sockaddr*)&dir_ganador, sizeof(dir_ganador)); 
                    cout << "serv envio: Olos... (a perdedor)" << endl;
                    sendto(socket_servidor, paquete_derrota.c_str(), paquete_derrota.size(), 0, (struct sockaddr*)&dir_perdedor, sizeof(dir_perdedor)); 
                    preparar_nuevo_juego();
                } else if (verificar_empate()) {
                    string paquete_empate = "Oemp";
                    paquete_empate.append(BUFFER_SIZE - paquete_empate.size(), '#');
                    cout << "serv envio: Oemp... (a jugadores)" << endl;
                    sendto(socket_servidor, paquete_empate.c_str(), paquete_empate.size(), 0, (struct sockaddr*)&dir_jugador1, sizeof(dir_jugador1)); 
                    sendto(socket_servidor, paquete_empate.c_str(), paquete_empate.size(), 0, (struct sockaddr*)&dir_jugador2, sizeof(dir_jugador2));
                    preparar_nuevo_juego();
                } else {
                    if (simbolo_recibido == "o") {
                        string paquete_turno_x = "Vx";
                        paquete_turno_x.append(BUFFER_SIZE - paquete_turno_x.size(), '#');
                        cout << "serv envio: " << paquete_turno_x.substr(0,40) << endl;
                        sendto(socket_servidor, paquete_turno_x.c_str(), paquete_turno_x.size(), 0, (struct sockaddr*)&dir_jugador2, sizeof(dir_jugador2));
                    } else {
                        string paquete_turno_o = "Vo";
                        paquete_turno_o.append(BUFFER_SIZE - paquete_turno_o.size(), '#');
                        cout << "serv envio: " << paquete_turno_o.substr(0,40) << endl;
                        sendto(socket_servidor, paquete_turno_o.c_str(), paquete_turno_o.size(), 0, (struct sockaddr*)&dir_jugador1, sizeof(dir_jugador1));
                    }
                }
            }
        } 
        else if (tipo_paquete == 'f' || tipo_paquete == 'e') { 
            cerr << "Aviso: Recibido paquete 'f'/'e' no fragmentado. Ignorando." << endl;
        } 
        else if (tipo_paquete == 'x') {
            cout << "\n" << apodo_cliente_actual << " ha cerrado conexión." << endl;
            dir_por_apodo.erase(apodo_cliente_actual);
            apodo_por_dir.erase(dir_cliente_texto);
            auto iterador_jugador = find(jugadores_activos.begin(), jugadores_activos.end(), dir_cliente_texto);
            if (iterador_jugador != jugadores_activos.end()) {
                 cout << "\nUn jugador se fue. Reiniciando la partida." << endl;
                 preparar_nuevo_juego();
            }
        }

    } 

    close(socket_servidor);
    return 0;
}