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
using namespace std;

#define BUFFER_SIZE 1024

string mi_simbolo_juego = "_";
const int DIM_TABLERO = 3;

struct sockaddr_in	 dir_servidor;
socklen_t longitud_dir_servidor = sizeof(dir_servidor);

struct BandejaReensamblaje {
    map<int, string> fragmentos;
    int tamano_total_esperado = 0;
    int datos_recibidos_actual = 0;
    string nombre_archivo;
    string apodo_remitente;
    bool ultimo_fragmento_recibido = false;
};

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
            cout << "\nError al recibir datos del servidor. Desconectando." << endl;
            close(socket_cliente);
            return;
        }

        string paquete_recibido(bufer_recepcion, bytes_recibidos); 
        cout.flush();

        cout << "\nclient recv:  " << paquete_recibido.substr(0, 70) << endl;

        if (bytes_recibidos > 0 && isdigit(paquete_recibido[0])) {
            string seq_texto = paquete_recibido.substr(0, 5);
            int num_secuencia = stoi(seq_texto);
            
            char tipo_fragmento = paquete_recibido[5];
            if (tipo_fragmento != 'F') { 
                 cout<< "Error: Paquete con secuencia pero no es tipo 'F'" << endl;
                 continue;
            }

            int offset = 6; 
            int longitud_apodo = stoi(paquete_recibido.substr(offset, 2));
            offset += 2;
            string apodo_remitente = paquete_recibido.substr(offset, longitud_apodo);
            offset += longitud_apodo;

            int longitud_nombre_archivo = stoi(paquete_recibido.substr(offset, 2));
            offset += 2;
            string nombre_archivo = paquete_recibido.substr(offset, longitud_nombre_archivo);
            offset += longitud_nombre_archivo;

            int tamano_archivo = stoi(paquete_recibido.substr(offset, 10));
            offset += 10;

            string datos_fragmento = paquete_recibido.substr(offset);

            string clave_transferencia = apodo_remitente + ":" + nombre_archivo;

            if (bandejas_archivos.count(clave_transferencia) == 0) {
                BandejaReensamblaje nueva_bandeja;
                nueva_bandeja.tamano_total_esperado = tamano_archivo;
                nueva_bandeja.nombre_archivo = nombre_archivo;
                nueva_bandeja.apodo_remitente = apodo_remitente;
                bandejas_archivos[clave_transferencia] = nueva_bandeja;
                cout << "Comenzando a recibir el archivo '" << nombre_archivo << "' de " << apodo_remitente << endl;
            }

            BandejaReensamblaje& bandeja_actual = bandejas_archivos[clave_transferencia];

            if (bandeja_actual.fragmentos.count(num_secuencia)) {
                cout << "Aviso: Fragmento duplicado (seq " << num_secuencia << "). Omitiendo." << endl;
                continue;
            }

            size_t pos_relleno = datos_fragmento.find('#');
            bool es_ultimo = (pos_relleno != string::npos);
            string datos_reales = (es_ultimo) ? datos_fragmento.substr(0, pos_relleno) : datos_fragmento;

            bandeja_actual.fragmentos[num_secuencia] = datos_reales;
            bandeja_actual.datos_recibidos_actual += datos_reales.size();
            
            if (es_ultimo) {
                bandeja_actual.ultimo_fragmento_recibido = true;
                cout << "Se recibió el último fragmento (seq " << num_secuencia << "). Total: " 
                     << bandeja_actual.datos_recibidos_actual << "/" << bandeja_actual.tamano_total_esperado << endl;
            }

            if (bandeja_actual.ultimo_fragmento_recibido && bandeja_actual.datos_recibidos_actual == bandeja_actual.tamano_total_esperado) {
                cout << "\n¡Transferencia completa! Ensamblando '" << bandeja_actual.nombre_archivo << "'..." << endl;
                
                string datos_archivo_completos;
                for (const auto& par : bandeja_actual.fragmentos) {
                    datos_archivo_completos.append(par.second);
                }

                if (datos_archivo_completos.size() == bandeja_actual.tamano_total_esperado) {
                    ofstream out("recibido_" + bandeja_actual.nombre_archivo, ios::binary);
                    out.write(datos_archivo_completos.c_str(), datos_archivo_completos.size());
                    out.close();

                    cout << "Archivo de " << bandeja_actual.apodo_remitente << ": " << bandeja_actual.nombre_archivo 
                         << " (" << datos_archivo_completos.size() << " bytes) guardado como recibido_" << bandeja_actual.nombre_archivo << "\n";
                } else {
                    cout << "Fallo al ensamblar: Tamaño final no coincide. Esperado: " 
                         << bandeja_actual.tamano_total_esperado << ", Obtenido: " << datos_archivo_completos.size() << endl;
                }
                
                bandejas_archivos.erase(clave_transferencia);
            
            } else if (bandeja_actual.ultimo_fragmento_recibido) {
                cout << "Error: Aún faltan datos. Total: " 
                     << bandeja_actual.datos_recibidos_actual << "/" << bandeja_actual.tamano_total_esperado << ". Esperando retransmisión..." << endl;
            }

        } 
        
        else if (bytes_recibidos > 0) {
            char tipo_paquete = paquete_recibido[0];

            if (tipo_paquete == 'M') { 
                int longitud_apodo = stoi(paquete_recibido.substr(1, 2));
                string apodo_remitente = paquete_recibido.substr(3, longitud_apodo);
                int longitud_mensaje = stoi(paquete_recibido.substr(3 + longitud_apodo, 3));
                string contenido_mensaje = paquete_recibido.substr(3 + longitud_apodo + 3, longitud_mensaje);
                cout << "\n[" << apodo_remitente << " (global)]: " << contenido_mensaje << endl;

            } else if (tipo_paquete == 'T') { 
                int longitud_apodo = stoi(paquete_recibido.substr(1, 2));
                string apodo_remitente = paquete_recibido.substr(3, longitud_apodo);
                int longitud_mensaje = stoi(paquete_recibido.substr(3 + longitud_apodo, 3));
                string contenido_mensaje = paquete_recibido.substr(3 + longitud_apodo + 3, longitud_mensaje);
                cout << "\n[PRIVADO de " << apodo_remitente << "]: " << contenido_mensaje << endl;

            } else if (tipo_paquete == 'L') { 
                int total_usuarios = stoi(paquete_recibido.substr(1, 2));
                cout << "\n--- Lista de Usuarios (" << total_usuarios << ") ---" << endl;
                int offset = 3;
                for (int i = 0; i < total_usuarios; ++i) {
                    int longitud_apodo = stoi(paquete_recibido.substr(offset, 2));
                    offset += 2;
                    string apodo = paquete_recibido.substr(offset, longitud_apodo);
                    offset += longitud_apodo;
                    cout << "-> " << apodo << endl;
                }
                cout << "-----------------------------------" << endl;

            } else if (tipo_paquete == 'V') { 
                mi_simbolo_juego = paquete_recibido.substr(1, 1);
                cout << "\n¡Te toca! Tu símbolo es '" << mi_simbolo_juego << "'" << endl;

            } else if (tipo_paquete == 'v') { 
                string tablero_texto = paquete_recibido.substr(1, DIM_TABLERO * DIM_TABLERO); 
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

            } else if (tipo_paquete == 'F') { 
                cerr << "Aviso: Recibido paquete 'F' no fragmentado. Ignorando." << endl;

            } else if (tipo_paquete == 'O') { 
                mi_simbolo_juego = "_"; 
                string resultado = paquete_recibido.substr(1, 3);
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
    string paquete_apodo = string(1, 'n') + longitud_apodo_str + apodo_inicial;
    
    paquete_apodo.append(BUFFER_SIZE - paquete_apodo.size(), '#'); 
    
    cout << "client envio: " << paquete_apodo.substr(0, 70) << endl;
    
    sendto(socket_cliente, paquete_apodo.c_str(), paquete_apodo.size(), 0, (const struct sockaddr *) &dir_servidor, sizeof(dir_servidor));


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

        string contenido, longitud_str, paquete_a_enviar, apodo_destino, longitud_destino_str;
        char tipo_paquete;

        if (opcion_menu == 1) { 
            tipo_paquete = 'n';
            cout << "Ingresa tu nuevo alias: ";
            cin >> contenido;
            int tamNick = contenido.size();
            longitud_str = (tamNick < 10) ? "0" + to_string(tamNick) : to_string(tamNick);
            paquete_a_enviar = string(1, tipo_paquete) + longitud_str + contenido;
        } else if (opcion_menu == 2) { 
            tipo_paquete = 't';
            cout << "Destinatario: ";
            cin >> apodo_destino;
            cout << "Mensaje: ";
            cin.ignore();
            getline(cin, contenido);
            int longitud_destino = apodo_destino.size();
            int longitud_mensaje = contenido.size();
            longitud_destino_str = (longitud_destino < 10) ? "0" + to_string(longitud_destino) : to_string(longitud_destino);
            longitud_str = (longitud_mensaje < 10) ? "00" + to_string(longitud_mensaje) : (longitud_mensaje < 100) ? "0" + to_string(longitud_mensaje) : to_string(longitud_mensaje);
            paquete_a_enviar = string(1, tipo_paquete) + longitud_destino_str + apodo_destino + longitud_str + contenido;
        } else if (opcion_menu == 3) { 
            tipo_paquete = 'm';
            cout << "Mensaje para todos: ";
            cin.ignore();
            getline(cin, contenido);
            int longitud_mensaje = contenido.size();
            longitud_str = (longitud_mensaje < 10) ? "00" + to_string(longitud_mensaje) : (longitud_mensaje < 100) ? "0" + to_string(longitud_mensaje) : to_string(longitud_mensaje);
            paquete_a_enviar = string(1, tipo_paquete) + longitud_str + contenido;
        } else if (opcion_menu == 4) { 
            tipo_paquete = 'l';
            paquete_a_enviar = string(1, tipo_paquete);
        } 
        else if(opcion_menu == 5){ 
            tipo_paquete = 'f'; 
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

            string encabezado_archivo = string(1, tipo_paquete) + longitud_destino_str + apodo_destino + longitud_nombre_archivo_str + nombre_archivo_local + tamano_archivo_str;

            int tamano_fragmento_datos = BUFFER_SIZE - encabezado_archivo.size() - 5; 
            int num_secuencia = 0;
            int bytes_enviados = 0;

            while (bytes_enviados < tamano_archivo) {
                string seq_como_texto = to_string(num_secuencia);
                string seq_texto_formato = string(5 - seq_como_texto.size(), '0') + seq_como_texto;
                
                int tamano_fragmento_actual = min(tamano_fragmento_datos, tamano_archivo - bytes_enviados);
                
                string fragmento_datos = datos_archivo.substr(bytes_enviados, tamano_fragmento_actual);
                
                string paquete_fragmento = seq_texto_formato + encabezado_archivo + fragmento_datos;
                
                bool es_ultimo = (bytes_enviados + tamano_fragmento_actual == tamano_archivo);

                if (es_ultimo) {
                    paquete_fragmento.append(BUFFER_SIZE - paquete_fragmento.size(), '#');
                }

                cout << "client envio: " << paquete_fragmento.substr(0, 70) 
                     << "... (seq=" << seq_texto_formato << ", tam=" << paquete_fragmento.size() << (es_ultimo ? " (ULTIMO)" : "") << ")\n";

                sendto(socket_cliente, paquete_fragmento.c_str(), paquete_fragmento.size(), 0, (const struct sockaddr *) &dir_servidor, sizeof(dir_servidor));

                bytes_enviados += tamano_fragmento_actual;
                num_secuencia++;

                usleep(1000); 
            }
            
            continue;
        }
        else if (opcion_menu == 6) { 
            tipo_paquete = 'p';
            paquete_a_enviar = string(1, tipo_paquete);
        } else if (opcion_menu == 7) { 
            if (mi_simbolo_juego == "_") {
                cout << "No puedes jugar ahora (no es tu turno o no estás en partida)." << endl;
                continue;
            }
            cout << "Ingresa tu movimiento (posición 1-" << DIM_TABLERO * DIM_TABLERO << "): ";
            int posicion_jugada;
            cin >> posicion_jugada;
            paquete_a_enviar = string(1, 'w') + mi_simbolo_juego + to_string(posicion_jugada);
        } else if (opcion_menu == 8) { 
            tipo_paquete = 'x';
            paquete_a_enviar = string(1, tipo_paquete);
            
            paquete_a_enviar.append(BUFFER_SIZE - paquete_a_enviar.size(), '#');

            cout << "client envio: " << paquete_a_enviar.substr(0, 70) << endl;
            
            sendto(socket_cliente, paquete_a_enviar.c_str(), paquete_a_enviar.size(), 0, (const struct sockaddr *) &dir_servidor, sizeof(dir_servidor));
            break; 
        } else {
            cout << "Opción no reconocida." << endl;
            continue;
        }
        
        paquete_a_enviar.append(BUFFER_SIZE - paquete_a_enviar.size(), '#');
        
        cout << "client envio: " << paquete_a_enviar.substr(0, 70) << endl;
        sendto(socket_cliente, paquete_a_enviar.c_str(), paquete_a_enviar.size(), 0, (const struct sockaddr *) &dir_servidor, sizeof(dir_servidor));
    }

    close(socket_cliente);
    cout << "Conexión cerrada." << endl;
    exit(0); 
    return 0;
}