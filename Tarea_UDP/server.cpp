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
#include <iomanip>
#include <sstream>

using namespace std;

#define BUFFER_SIZE 777

unordered_map<string, string> apodo_por_dir;
unordered_map<string, struct sockaddr_in> dir_por_apodo;
vector<string> jugadores_activos;

string simbolo_base = "o";
const int DIM_TABLERO = 3;
string estado_tablero[DIM_TABLERO * DIM_TABLERO];
int contador_jugadas = 0;
int secuencia_server = 0;

string calcularChecksum(const string& payload) {
    int suma = 0;
    for (char c : payload) {
        suma += static_cast<unsigned char>(c);
    }
    return to_string(suma % 6);
}

string crearPaquete(char tipo, int seq, string payload) {
    // 1. Preparamos la cabecera básica para calcular tamaños
    // Estructura: Tipo(1) + Seq(5) + Checksum(1) = 7 bytes de cabecera
    int header_size = 1 + 5 + 1; 
    
    // 2. Calculamos cuánto relleno necesitamos para llegar a BUFFER_SIZE
    int espacio_disponible = BUFFER_SIZE - header_size;
    
    // Creamos una copia del payload para añadirle el padding
    string payload_con_padding = payload;
    
    if ((int)payload_con_padding.size() < espacio_disponible) {
        // Rellenamos con '#' hasta completar el buffer
        payload_con_padding.append(espacio_disponible - payload_con_padding.size(), '#');
    }

    // 3. Formateamos el número de secuencia
    stringstream ss_seq;
    ss_seq << setfill('0') << setw(5) << seq;
    string seq_str = ss_seq.str();

    // 4. IMPORTANTE: Calculamos el checksum sobre el payload YA RELLENO
    string cksum = calcularChecksum(payload_con_padding);

    // 5. Retornamos el paquete final
    return tipo + seq_str + cksum + payload_con_padding;
}

string crearPaqueteControl(char tipo, int seq) {
    stringstream ss_seq;
    ss_seq << setfill('0') << setw(5) << seq;
    string seq_str = ss_seq.str();
    string paquete = tipo + seq_str + "0"; 
    if (paquete.size() < BUFFER_SIZE) {
        paquete.append(BUFFER_SIZE - paquete.size(), '#');
    }
    return paquete;
}

string obtenerContenidoLog(const string& paquete) {
    string visual = paquete;
    if (visual.size() > 30) {
        visual = visual.substr(0, 30) + "...";
    }
    return visual;
}

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

    cout << "Servidor UDP iniciado en puerto " << puerto_servidor << endl;

    while (true) {
        memset(bufer_recepcion, 0, BUFFER_SIZE);
        int bytes_recibidos = recvfrom(socket_servidor, bufer_recepcion, BUFFER_SIZE, 0, (struct sockaddr *) &dir_cliente, &longitud_dir_cliente);

        if (bytes_recibidos <= 0) continue;

        string paquete_recibido(bufer_recepcion, bytes_recibidos);
        cout << "[RECIBIDO]: " << obtenerContenidoLog(paquete_recibido) << endl;

        if (paquete_recibido.size() < 7) continue;

        char tipo_recibido = paquete_recibido[0];
        string seq_str = paquete_recibido.substr(1, 5);
        string cksum_recibido = paquete_recibido.substr(6, 1);
        int seq_num = stoi(seq_str);

        string payload_net = paquete_recibido.substr(7);

        if (tipo_recibido == 'd') {
            string cksum_calculado = calcularChecksum(payload_net);
            if (cksum_calculado != cksum_recibido) {
                string nack = crearPaqueteControl('N', seq_num);
                sendto(socket_servidor, nack.c_str(), nack.size(), 0, (struct sockaddr*)&dir_cliente, longitud_dir_cliente);
                cout << "[ENVIANDO]: " << obtenerContenidoLog(nack) << endl;
                continue;
            } else {
                string ack = crearPaqueteControl('A', seq_num);
                sendto(socket_servidor, ack.c_str(), ack.size(), 0, (struct sockaddr*)&dir_cliente, longitud_dir_cliente);
                cout << "[ENVIANDO]: " << obtenerContenidoLog(ack) << endl;
            }
        }

        if (tipo_recibido == 'a') {
            cout << "[PROTOCOLO] Paquete SEQ " << seq_num << " confirmado por el receptor" << endl;
            continue;
        } else if (tipo_recibido == 'n') {
            cout << "[PROTOCOLO] Error en paquete SEQ " << seq_num << " (NACK recibido)" << endl;
            continue;
        } else if (tipo_recibido == 'd') {
            string dir_cliente_texto = direccion_a_texto(dir_cliente);
            string apodo_cliente_actual;

            if (apodo_por_dir.count(dir_cliente_texto)) {
                apodo_cliente_actual = apodo_por_dir[dir_cliente_texto];
            } else {
                apodo_cliente_actual = "temp_" + dir_cliente_texto;
                apodo_por_dir[dir_cliente_texto] = apodo_cliente_actual;
                dir_por_apodo[apodo_cliente_actual] = dir_cliente;
            }

            if (payload_net.empty()) continue;
            char tipo_app = payload_net[0];

            if (tipo_app == 'f' || tipo_app == 'e') {
                int offset = 1;
                int longitud_destino = stoi(payload_net.substr(offset, 2));
                offset += 2;
                string apodo_destino = payload_net.substr(offset, longitud_destino);
                offset += longitud_destino;

                if (dir_por_apodo.count(apodo_destino)) {
                    struct sockaddr_in direccion_destino = dir_por_apodo[apodo_destino];
                    string longitud_origen = (apodo_cliente_actual.size() < 10) ? "0" + to_string(apodo_cliente_actual.size()) : to_string(apodo_cliente_actual.size());
                    
                    char tipo_forward = toupper(tipo_app); 
                    
                    string payload_forward = string(1, tipo_forward) + longitud_origen + apodo_cliente_actual + payload_net.substr(offset);
                    string paquete_envio = crearPaquete('D', ++secuencia_server, payload_forward);
                    sendto(socket_servidor, paquete_envio.c_str(), paquete_envio.size(), 0, (struct sockaddr*)&direccion_destino, sizeof(direccion_destino));
                    cout << "[ENVIANDO]: " << obtenerContenidoLog(paquete_envio) << endl;
                }
            } else if (tipo_app == 'n') {
                int longitud_nuevo_apodo = stoi(payload_net.substr(1, 2));
                string nuevo_apodo = payload_net.substr(3, longitud_nuevo_apodo);
                string apodo_anterior = apodo_por_dir[dir_cliente_texto];
                apodo_por_dir[dir_cliente_texto] = nuevo_apodo;
                struct sockaddr_in direccion_cliente_actual = dir_por_apodo[apodo_anterior];
                dir_por_apodo.erase(apodo_anterior);
                dir_por_apodo[nuevo_apodo] = direccion_cliente_actual;
            } else if (tipo_app == 'm') {
                int longitud_mensaje = stoi(payload_net.substr(1, 3));
                string contenido_mensaje = payload_net.substr(4, longitud_mensaje);
                int tamNick = apodo_cliente_actual.size();
                string longitud_origen = (tamNick < 10) ? "0" + to_string(tamNick) : to_string(tamNick);
                string longitud_mensaje_str = (longitud_mensaje < 10) ? "00" + to_string(longitud_mensaje) : (longitud_mensaje < 100) ? "0" + to_string(longitud_mensaje) : to_string(longitud_mensaje);
                string nuevo_payload = string("M") + longitud_origen + apodo_cliente_actual + longitud_mensaje_str + contenido_mensaje;
                string paquete_envio = crearPaquete('D', ++secuencia_server, nuevo_payload);
                for (const auto& par : dir_por_apodo) {
                    if (par.first != apodo_cliente_actual) {
                        sendto(socket_servidor, paquete_envio.c_str(), paquete_envio.size(), 0, (struct sockaddr*)&par.second, sizeof(par.second));
                        cout << "[ENVIANDO]: " << obtenerContenidoLog(paquete_envio) << endl;
                    }
                }
            } else if (tipo_app == 't') {
                int longitud_destino = stoi(payload_net.substr(1, 2));
                string apodo_destino = payload_net.substr(3, longitud_destino);
                int longitud_mensaje = stoi(payload_net.substr(3 + longitud_destino, 3));
                string contenido_mensaje = payload_net.substr(3 + longitud_destino + 3, longitud_mensaje);
                int tamNick = apodo_cliente_actual.size();
                string longitud_origen = (tamNick < 10) ? "0" + to_string(tamNick) : to_string(tamNick);
                string longitud_mensaje_str = (longitud_mensaje < 10) ? "00" + to_string(longitud_mensaje) : (longitud_mensaje < 100) ? "0" + to_string(longitud_mensaje) : to_string(longitud_mensaje);
                string nuevo_payload = string("T") + longitud_origen + apodo_cliente_actual + longitud_mensaje_str + contenido_mensaje;
                string paquete_envio = crearPaquete('D', ++secuencia_server, nuevo_payload);
                if (dir_por_apodo.count(apodo_destino)) {
                    struct sockaddr_in direccion_destino = dir_por_apodo[apodo_destino];
                    sendto(socket_servidor, paquete_envio.c_str(), paquete_envio.size(), 0, (struct sockaddr*)&direccion_destino, sizeof(direccion_destino));
                    cout << "[ENVIANDO]: " << obtenerContenidoLog(paquete_envio) << endl;
                }
            } else if (tipo_app == 'l') {
                string lista_nombres;
                for (const auto& par : dir_por_apodo) {
                    string tamNick = (par.first.size() < 10) ? "0" + to_string(par.first.size()) : to_string(par.first.size());
                    lista_nombres += tamNick + par.first;
                }
                string total_clientes_str = (dir_por_apodo.size() < 10) ? "0" + to_string(dir_por_apodo.size()) : to_string(dir_por_apodo.size());
                string nuevo_payload = "L" + total_clientes_str + lista_nombres;
                string paquete_envio = crearPaquete('D', ++secuencia_server, nuevo_payload);
                sendto(socket_servidor, paquete_envio.c_str(), paquete_envio.size(), 0, (struct sockaddr*)&dir_cliente, longitud_dir_cliente);
                cout << "[ENVIANDO]: " << obtenerContenidoLog(paquete_envio) << endl;
            } else if (tipo_app == 'p') {
                if (find(jugadores_activos.begin(), jugadores_activos.end(), dir_cliente_texto) != jugadores_activos.end()) {
                    continue;
                }
                if (jugadores_activos.size() < 2) {
                    jugadores_activos.push_back(dir_cliente_texto);
                    if (jugadores_activos.size() == 2) {
                        string nuevo_payload = "Vo";
                        string paquete_envio = crearPaquete('D', ++secuencia_server, nuevo_payload);
                        string apodo_j1 = apodo_por_dir[jugadores_activos[0]];
                        struct sockaddr_in dir_jugador1 = dir_por_apodo[apodo_j1];
                        sendto(socket_servidor, paquete_envio.c_str(), paquete_envio.size(), 0, (struct sockaddr*)&dir_jugador1, sizeof(dir_jugador1));
                        cout << "[ENVIANDO]: " << obtenerContenidoLog(paquete_envio) << endl;
                    }
                }
            } else if (tipo_app == 'w') {
                string simbolo_recibido = payload_net.substr(1, 1);
                int posicion_jugada = stoi(payload_net.substr(2));
                if (posicion_jugada >= 1 && posicion_jugada <= DIM_TABLERO * DIM_TABLERO && estado_tablero[posicion_jugada-1] == "_") {
                    estado_tablero[posicion_jugada - 1] = simbolo_recibido;
                    contador_jugadas++;
                    string tablero_str;
                    for(int i = 0; i < DIM_TABLERO * DIM_TABLERO; ++i) tablero_str += estado_tablero[i];
                    string nuevo_payload = "v" + tablero_str;
                    string paquete_envio = crearPaquete('D', ++secuencia_server, nuevo_payload);
                    string apodo_j1 = apodo_por_dir[jugadores_activos[0]];
                    string apodo_j2 = apodo_por_dir[jugadores_activos[1]];
                    struct sockaddr_in dir_jugador1 = dir_por_apodo[apodo_j1];
                    struct sockaddr_in dir_jugador2 = dir_por_apodo[apodo_j2];
                    sendto(socket_servidor, paquete_envio.c_str(), paquete_envio.size(), 0, (struct sockaddr*)&dir_jugador1, sizeof(dir_jugador1));
                    cout << "[ENVIANDO]: " << obtenerContenidoLog(paquete_envio) << endl;
                    sendto(socket_servidor, paquete_envio.c_str(), paquete_envio.size(), 0, (struct sockaddr*)&dir_jugador2, sizeof(dir_jugador2));
                    cout << "[ENVIANDO]: " << obtenerContenidoLog(paquete_envio) << endl;

                    if (verificar_ganador(simbolo_recibido)) {
                        string dir_str_ganador = (simbolo_recibido == "o") ? jugadores_activos[0] : jugadores_activos[1];
                        string dir_str_perdedor = (simbolo_recibido == "o") ? jugadores_activos[1] : jugadores_activos[0];
                        string apodo_ganador = apodo_por_dir[dir_str_ganador];
                        string apodo_perdedor = apodo_por_dir[dir_str_perdedor];
                        struct sockaddr_in dir_ganador = dir_por_apodo[apodo_ganador];
                        struct sockaddr_in dir_perdedor = dir_por_apodo[apodo_perdedor];
                        string pay_win = "Owin";
                        string pkt_win = crearPaquete('D', ++secuencia_server, pay_win);
                        string pay_los = "Olos";
                        string pkt_los = crearPaquete('D', ++secuencia_server, pay_los);
                        sendto(socket_servidor, pkt_win.c_str(), pkt_win.size(), 0, (struct sockaddr*)&dir_ganador, sizeof(dir_ganador));
                        cout << "[ENVIANDO]: " << obtenerContenidoLog(pkt_win) << endl;
                        sendto(socket_servidor, pkt_los.c_str(), pkt_los.size(), 0, (struct sockaddr*)&dir_perdedor, sizeof(dir_perdedor));
                        cout << "[ENVIANDO]: " << obtenerContenidoLog(pkt_los) << endl;
                        preparar_nuevo_juego();
                    } else if (verificar_empate()) {
                        string pay_emp = "Oemp";
                        string pkt_emp = crearPaquete('D', ++secuencia_server, pay_emp);
                        sendto(socket_servidor, pkt_emp.c_str(), pkt_emp.size(), 0, (struct sockaddr*)&dir_jugador1, sizeof(dir_jugador1));
                        cout << "[ENVIANDO]: " << obtenerContenidoLog(pkt_emp) << endl;
                        sendto(socket_servidor, pkt_emp.c_str(), pkt_emp.size(), 0, (struct sockaddr*)&dir_jugador2, sizeof(dir_jugador2));
                        cout << "[ENVIANDO]: " << obtenerContenidoLog(pkt_emp) << endl;
                        preparar_nuevo_juego();
                    } else {
                        if (simbolo_recibido == "o") {
                            string pay_next = "Vx";
                            string pkt_next = crearPaquete('D', ++secuencia_server, pay_next);
                            sendto(socket_servidor, pkt_next.c_str(), pkt_next.size(), 0, (struct sockaddr*)&dir_jugador2, sizeof(dir_jugador2));
                            cout << "[ENVIANDO]: " << obtenerContenidoLog(pkt_next) << endl;
                        } else {
                            string pay_next = "Vo";
                            string pkt_next = crearPaquete('D', ++secuencia_server, pay_next);
                            sendto(socket_servidor, pkt_next.c_str(), pkt_next.size(), 0, (struct sockaddr*)&dir_jugador1, sizeof(dir_jugador1));
                            cout << "[ENVIANDO]: " << obtenerContenidoLog(pkt_next) << endl;
                        }
                    }
                }
            } else if (tipo_app == 'x') {
                cout << "\n" << apodo_cliente_actual << " ha cerrado conexión." << endl;
                dir_por_apodo.erase(apodo_cliente_actual);
                apodo_por_dir.erase(dir_cliente_texto);
                auto iterador_jugador = find(jugadores_activos.begin(), jugadores_activos.end(), dir_cliente_texto);
                if (iterador_jugador != jugadores_activos.end()) {
                    preparar_nuevo_juego();
                }
            }
        }
    }

    close(socket_servidor);
    return 0;
}