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
#include <limits>
#include <sstream>
#include <fstream>
#include <netdb.h>
#include <errno.h>
#include <map>
#include <cctype>
#include <unordered_map>
#include <mutex>
#include <vector>
#include <iomanip>

using namespace std;

#define BUFFER_SIZE 777

string mi_simbolo_juego = "_";
const int DIM_TABLERO = 3;
int secuencia_global = 0;

struct sockaddr_in dir_servidor;
socklen_t longitud_dir_servidor = sizeof(dir_servidor);

struct BandejaReensamblaje {
    map<int, string> fragmentos;
    int tamano_total_esperado = 0;
    string nombre_archivo;
    string apodo_remitente;
    int paquetes_recibidos_count = 0;
};

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

void mostrarMenu() {
    cout << endl;
    cout << "1. Cambiar apodo" << endl;
    cout << "2. Enviar mensaje privado" << endl;
    cout << "3. Enviar mensaje broadcast" << endl;
    cout << "4. Ver lista de usuarios" << endl;
    cout << "5. Enviar un archivo" << endl;
    cout << "6. Unirse a partida" << endl;
    cout << "7. Realizar movimiento" << endl;
    cout << "8. Salir" << endl;
    cout << "Seleccione una opción: ";
}

void recibirMensajes(int socket_cliente) {
    char bufer_recepcion[BUFFER_SIZE];
    static unordered_map <string, BandejaReensamblaje> bandejas_archivos;

    while (true) {
        memset(bufer_recepcion, 0, sizeof(bufer_recepcion));
        int bytes_recibidos = recvfrom(socket_cliente, bufer_recepcion, BUFFER_SIZE, 0, (struct sockaddr *) &dir_servidor, &longitud_dir_servidor);

        if (bytes_recibidos <= 0) {
            cout << "\nError al recibir datos del servidor." << endl;
            close(socket_cliente);
            return;
        }

        string paquete_recibido(bufer_recepcion, bytes_recibidos);
        cout << "[RECIBIDO]: " << obtenerContenidoLog(paquete_recibido) << endl;

        if (paquete_recibido.size() < 7) continue;

        char tipo_recibido = paquete_recibido[0];
        string seq_str = paquete_recibido.substr(1, 5);
        string cksum_recibido = paquete_recibido.substr(6, 1);
        int seq_num = stoi(seq_str);

        string payload_net = paquete_recibido.substr(7);

        if (tipo_recibido == 'D') {
            string cksum_calculado = calcularChecksum(payload_net);
            if (cksum_calculado != cksum_recibido) {
                string nack = crearPaqueteControl('n', seq_num);
                sendto(socket_cliente, nack.c_str(), nack.size(), 0, (struct sockaddr*)&dir_servidor, longitud_dir_servidor);
                cout << "[ENVIANDO]: " << obtenerContenidoLog(nack) << endl;
                continue;
            } else {
                string ack = crearPaqueteControl('a', seq_num);
                sendto(socket_cliente, ack.c_str(), ack.size(), 0, (struct sockaddr*)&dir_servidor, longitud_dir_servidor);
                cout << "[ENVIANDO]: " << obtenerContenidoLog(ack) << endl;
            }
        }

        if (tipo_recibido == 'A') {
            cout << "[PROTOCOLO] Paquete SEQ " << seq_num << " confirmado por el receptor" << endl;
            continue;
        } else if (tipo_recibido == 'N') {
            cout << "[PROTOCOLO] Error en paquete SEQ " << seq_num << " (NACK recibido)" << endl;
            continue;
        } else if (tipo_recibido == 'D') {
            if (payload_net.empty()) continue;
            char tipo_app = payload_net[0];

            if (tipo_app == 'F' || tipo_app == 'E') {
                bool es_ultimo_paquete = (tipo_app == 'E');
                int offset = 1;
                int longitud_apodo = stoi(payload_net.substr(offset, 2));
                offset += 2;
                string apodo_remitente = payload_net.substr(offset, longitud_apodo);
                offset += longitud_apodo;
                int longitud_nombre_archivo = stoi(payload_net.substr(offset, 2));
                offset += 2;
                string nombre_archivo = payload_net.substr(offset, longitud_nombre_archivo);
                offset += longitud_nombre_archivo;
                int tamano_archivo = stoi(payload_net.substr(offset, 10));
                offset += 10;
                string datos_fragmento = payload_net.substr(offset);
                string clave_transferencia = apodo_remitente + ":" + nombre_archivo;

                if (bandejas_archivos.count(clave_transferencia) == 0) {
                    BandejaReensamblaje nueva_bandeja;
                    nueva_bandeja.tamano_total_esperado = tamano_archivo;
                    nueva_bandeja.nombre_archivo = nombre_archivo;
                    nueva_bandeja.apodo_remitente = apodo_remitente;
                    bandejas_archivos[clave_transferencia] = nueva_bandeja;
                    cout << "\n[UDP] Recibiendo archivo '" << nombre_archivo << "' de " << apodo_remitente << "..." << endl;
                }

                BandejaReensamblaje& bandeja_actual = bandejas_archivos[clave_transferencia];
                bandeja_actual.fragmentos[seq_num] = datos_fragmento;
                bandeja_actual.paquetes_recibidos_count++;

                if (es_ultimo_paquete) {
                    string datos_archivo_completos;
                    for (const auto& par : bandeja_actual.fragmentos) {
                        datos_archivo_completos.append(par.second);
                    }
                    if (datos_archivo_completos.size() > bandeja_actual.tamano_total_esperado) {
                        datos_archivo_completos.resize(bandeja_actual.tamano_total_esperado);
                    }

                    ofstream out("recibido_" + bandeja_actual.nombre_archivo, ios::binary);
                    out.write(datos_archivo_completos.c_str(), datos_archivo_completos.size());
                    out.close();
                    cout << "Archivo guardado como 'recibido_" << bandeja_actual.nombre_archivo << "' ("
                        << datos_archivo_completos.size() << " bytes). Paquetes totales: " << bandeja_actual.paquetes_recibidos_count << "\n";
                    bandejas_archivos.erase(clave_transferencia);
                }
            } else if (tipo_app == 'M') {
                int longitud_apodo = stoi(payload_net.substr(1, 2));
                string apodo_remitente = payload_net.substr(3, longitud_apodo);
                int longitud_mensaje = stoi(payload_net.substr(3 + longitud_apodo, 3));
                string contenido_mensaje = payload_net.substr(3 + longitud_apodo + 3, longitud_mensaje);
                cout << "\n[" << apodo_remitente << " (global)]: " << contenido_mensaje << endl;
            } else if (tipo_app == 'T') {
                int longitud_apodo = stoi(payload_net.substr(1, 2));
                string apodo_remitente = payload_net.substr(3, longitud_apodo);
                int longitud_mensaje = stoi(payload_net.substr(3 + longitud_apodo, 3));
                string contenido_mensaje = payload_net.substr(3 + longitud_apodo + 3, longitud_mensaje);
                cout << "\n[PRIVADO de " << apodo_remitente << "]: " << contenido_mensaje << endl;
            } else if (tipo_app == 'L') {
                int total_usuarios = stoi(payload_net.substr(1, 2));
                cout << "\n--- Lista de Usuarios (" << total_usuarios << ") ---" << endl;
                int offset = 3;
                for (int i = 0; i < total_usuarios; ++i) {
                    int longitud_apodo = stoi(payload_net.substr(offset, 2));
                    offset += 2;
                    string apodo = payload_net.substr(offset, longitud_apodo);
                    offset += longitud_apodo;
                    cout << "-> " << apodo << endl;
                }
                cout << "-----------------------------------" << endl;
            } else if (tipo_app == 'V') {
                mi_simbolo_juego = payload_net.substr(1, 1);
                cout << "\n¡Te toca! Tu símbolo es '" << mi_simbolo_juego << "'" << endl;
            } else if (tipo_app == 'v') {
                string tablero_texto = payload_net.substr(1, DIM_TABLERO * DIM_TABLERO);
                cout << "\n--- Tablero Actual ---" << endl;
                for (int i = 0; i < DIM_TABLERO * DIM_TABLERO; i++) {
                    cout << " " << tablero_texto[i] << " ";
                    if ((i + 1) % DIM_TABLERO == 0) {
                        cout << endl;
                        if (i < DIM_TABLERO * DIM_TABLERO - 1){
                            for(int k=0; k<DIM_TABLERO; ++k) cout << "---";
                            cout << endl;
                        }
                    } else {
                        cout << "|";
                    }
                }
                cout << "----------------------" << endl;
            } else if (tipo_app == 'O') {
                mi_simbolo_juego = "_";
                string resultado = payload_net.substr(1, 3);
                if(resultado == "win"){
                    cout << "\n¡GANASTE LA PARTIDA!" << endl;
                }
                else if (resultado == "los") {
                    cout << "\nPerdiste. ¡Qué lástima!" << endl;
                }
                else if (resultado == "emp") {
                    cout << "\nEl juego terminó en empate." << endl;
                }
            }
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        cerr << "Modo de uso: " << argv[0] << " <puerto_servidor>\n";
        return 1;
    }
    char* ip_servidor = "127.0.0.1";
    int puerto_servidor = atoi(argv[1]);
    struct hostent *host_info = (struct hostent *)gethostbyname(ip_servidor);
    int socket_cliente = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_cliente < 0) {
        perror("Fallo al crear el socket");
        exit(EXIT_FAILURE);
    }
    memset(&dir_servidor, 0, sizeof(dir_servidor));
    dir_servidor.sin_family = AF_INET;
    dir_servidor.sin_port = htons(puerto_servidor);
    dir_servidor.sin_addr = *((struct in_addr *)host_info->h_addr);
    thread hilo_receptor(recibirMensajes, socket_cliente);
    hilo_receptor.detach();

    cout << "Ingresa tu alias: ";
    string apodo_inicial;
    cin >> apodo_inicial;

    string longitud_apodo_str = (apodo_inicial.size() < 10) ? "0" + to_string(apodo_inicial.size()) : to_string(apodo_inicial.size());
    string payload_inicio = string("n") + longitud_apodo_str + apodo_inicial;
    string paquete_inicio = crearPaquete('d', ++secuencia_global, payload_inicio);

    sendto(socket_cliente, paquete_inicio.c_str(), paquete_inicio.size(), 0, (const struct sockaddr *) &dir_servidor, sizeof(dir_servidor));
    cout << "[ENVIANDO]: " << obtenerContenidoLog(paquete_inicio) << endl;

    int opcion_menu;
    while (true) {
        mostrarMenu();
        cin >> opcion_menu;

        if (cin.fail()) {
            cout << "Opción incorrecta. Ingresa un número." << endl;
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            continue;
        }

        string contenido, longitud_str, payload_a_enviar, apodo_destino, longitud_destino_str;
        char tipo_paquete_menu;

        if (opcion_menu == 1) {
            tipo_paquete_menu = 'n';
            cout << "Ingresa tu nuevo alias: ";
            cin >> contenido;
            int tamNick = contenido.size();
            longitud_str = (tamNick < 10) ? "0" + to_string(tamNick) : to_string(tamNick);
            payload_a_enviar = string(1, tipo_paquete_menu) + longitud_str + contenido;
        } else if (opcion_menu == 2) {
            tipo_paquete_menu = 't';
            cout << "Destinatario: ";
            cin >> apodo_destino;
            cout << "Mensaje: ";
            cin.ignore();
            getline(cin, contenido);
            int longitud_destino = apodo_destino.size();
            int longitud_mensaje = contenido.size();
            longitud_destino_str = (longitud_destino < 10) ? "0" + to_string(longitud_destino) : to_string(longitud_destino);
            longitud_str = (longitud_mensaje < 10) ? "00" + to_string(longitud_mensaje) : (longitud_mensaje < 100) ? "0" + to_string(longitud_mensaje) : to_string(longitud_mensaje);
            payload_a_enviar = string(1, tipo_paquete_menu) + longitud_destino_str + apodo_destino + longitud_str + contenido;
        } else if (opcion_menu == 3) {
            tipo_paquete_menu = 'm';
            cout << "Mensaje para todos: ";
            cin.ignore();
            getline(cin, contenido);
            int longitud_mensaje = contenido.size();
            longitud_str = (longitud_mensaje < 10) ? "00" + to_string(longitud_mensaje) : (longitud_mensaje < 100) ? "0" + to_string(longitud_mensaje) : to_string(longitud_mensaje);
            payload_a_enviar = string(1, tipo_paquete_menu) + longitud_str + contenido;
        } else if (opcion_menu == 4) {
            tipo_paquete_menu = 'l';
            payload_a_enviar = string(1, tipo_paquete_menu);
        } else if(opcion_menu == 5){
            cout << "Destinatario: ";
            cin >> apodo_destino;
            cout << "Nombre del archivo (local): ";
            string nombre_archivo_local;
            cin >> nombre_archivo_local;

            ifstream archivo_entrada(nombre_archivo_local, ios::binary);
            if (!archivo_entrada) {
                cout << "Error: No se pudo abrir el archivo " << nombre_archivo_local << endl;
                continue;
            }

            archivo_entrada.seekg(0, ios::end);
            int tamano_archivo = archivo_entrada.tellg();
            archivo_entrada.seekg(0, ios::beg);
            string datos_archivo(tamano_archivo, '\0');
            archivo_entrada.read(&datos_archivo[0], tamano_archivo);
            archivo_entrada.close();

            string longitud_destino_str = (apodo_destino.size() < 10) ? "0"+to_string(apodo_destino.size()) : to_string(apodo_destino.size());
            string longitud_nombre_archivo_str = (nombre_archivo_local.size() < 10) ? "0"+to_string(nombre_archivo_local.size()) : to_string(nombre_archivo_local.size());
            string tamano_archivo_str = string(10 - to_string(tamano_archivo).size(), '0') + to_string(tamano_archivo);

            string encabezado_payload = longitud_destino_str + apodo_destino + longitud_nombre_archivo_str + nombre_archivo_local + tamano_archivo_str;

            int overhead_encapsulamiento = 1 + 5 + 1;
            int max_payload_size = BUFFER_SIZE - overhead_encapsulamiento;
            int overhead_payload = 1 + encabezado_payload.size();
            int max_chunk_data = max_payload_size - overhead_payload;

            int bytes_enviados = 0;
            int paquetes_enviados_count = 0;
            cout << "Enviando " << tamano_archivo << " bytes..." << endl;

            while (bytes_enviados < tamano_archivo) {
                int tamano_restante = tamano_archivo - bytes_enviados;
                int chunk_actual = min(max_chunk_data, tamano_restante);

                string fragmento_datos = datos_archivo.substr(bytes_enviados, chunk_actual);
                bool es_ultimo = (bytes_enviados + chunk_actual == tamano_archivo);
                char tipo_fragmento = (es_ultimo) ? 'e' : 'f';

                string payload_archivo = string(1, tipo_fragmento) + encabezado_payload + fragmento_datos;
                string paquete_fragmento = crearPaquete('d', ++secuencia_global, payload_archivo);

                sendto(socket_cliente, paquete_fragmento.c_str(), paquete_fragmento.size(), 0, (const struct sockaddr *) &dir_servidor, sizeof(dir_servidor));
                cout << "[ENVIANDO]: " << obtenerContenidoLog(paquete_fragmento) << endl;

                bytes_enviados += chunk_actual;
                paquetes_enviados_count++;
                usleep(2000);
            }
            cout << "Envio finalizado. Paquetes enviados: " << paquetes_enviados_count << endl;
            continue;
        } else if (opcion_menu == 6) {
            tipo_paquete_menu = 'p';
            payload_a_enviar = string(1, tipo_paquete_menu);
        } else if (opcion_menu == 7) {
            if (mi_simbolo_juego == "_") {
                cout << "No puedes jugar ahora." << endl;
                continue;
            }
            cout << "Ingresa tu movimiento (posición 1-" << DIM_TABLERO * DIM_TABLERO << "): ";
            int posicion_jugada;
            cin >> posicion_jugada;
            payload_a_enviar = string("w") + mi_simbolo_juego + to_string(posicion_jugada);
        } else if (opcion_menu == 8) {
            tipo_paquete_menu = 'x';
            payload_a_enviar = string(1, tipo_paquete_menu);
            string paquete_final = crearPaquete('d', ++secuencia_global, payload_a_enviar);
            sendto(socket_cliente, paquete_final.c_str(), paquete_final.size(), 0, (const struct sockaddr *) &dir_servidor, sizeof(dir_servidor));
            cout << "[ENVIANDO]: " << obtenerContenidoLog(paquete_final) << endl;
            break;
        } else {
            cout << "Opción no reconocida." << endl;
            continue;
        }

        string paquete_a_enviar = crearPaquete('d', ++secuencia_global, payload_a_enviar);
        sendto(socket_cliente, paquete_a_enviar.c_str(), paquete_a_enviar.size(), 0, (const struct sockaddr *) &dir_servidor, sizeof(dir_servidor));
        cout << "[ENVIANDO]: " << obtenerContenidoLog(paquete_a_enviar) << endl;
    }

    close(socket_cliente);
    cout << "Conexión cerrada." << endl;
    return 0;
}