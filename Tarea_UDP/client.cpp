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
#include <set>
#include <chrono>

using namespace std;

#define BUFFER_SIZE 1024

const int TIMEOUT_MS = 2000; 
const int MAX_REINTENTOS = 5;  

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
    // 'ultimo_fragmento_recibido' ya no es necesario aquí, 
    // lo manejamos con el tipo de paquete 'E'
};

std::mutex mtx_acks;
std::set<int> acks_recibidos_para_envio_actual;


unsigned char calcular_crc(const string& datos) {
    unsigned char crc = 0;
    for (char c : datos) {
        crc += (unsigned char)c;
    }
    return crc;
}

string crc_a_texto(unsigned char crc) {
    string crc_str = to_string(crc);
    return string(3 - crc_str.size(), '0') + crc_str; 
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
            cout << "\nError al recibir datos del servidor. Desconectando." << endl;
            close(socket_cliente);
            return;
        }

        string paquete_recibido(bufer_recepcion, bytes_recibidos); 
        cout.flush();

        cout << "\nclient recv:  " << paquete_recibido.substr(0, 70) << "..." << endl;

        if (bytes_recibidos > 0 && isdigit(paquete_recibido[0])) {
            
            string seq_texto = paquete_recibido.substr(0, 5);
            int num_secuencia = stoi(seq_texto);
            
            // +++ MODIFICADO: LEER 'F' o 'E' +++
            char tipo_fragmento = paquete_recibido[5];
            bool es_ultimo_paquete = (tipo_fragmento == 'E');

            if (tipo_fragmento != 'F' && tipo_fragmento != 'E') { 
                 cout<< "Error: Paquete con secuencia pero no es tipo 'F' o 'E'" << endl;
                 continue;
            }
            // ---
            
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
            string crc_recibido_str = paquete_recibido.substr(offset, 3);
            unsigned char crc_recibido = (unsigned char)stoi(crc_recibido_str);
            offset += 3;
            
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
            
            // +++ MODIFICADO: Lógica de 'datos_reales' +++
            string datos_reales;
            if (es_ultimo_paquete) {
                // Solo si es el último paquete, buscamos '#'
                size_t pos_relleno = datos_fragmento.find('#');
                if (pos_relleno != string::npos) {
                    datos_reales = datos_fragmento.substr(0, pos_relleno);
                } else {
                    datos_reales = datos_fragmento; // No deberia pasar, pero por si acaso
                }
            } else {
                // Si NO es el último paquete, tomamos *todos* los datos.
                datos_reales = datos_fragmento;
            }
            // ---

            unsigned char crc_calculado = calcular_crc(datos_reales);
            if (crc_calculado != crc_recibido) {
                cout << "\n¡ERROR DE CHECKSUM! Paquete " << seq_texto << " de " << apodo_remitente << " está corrupto. Descartando." << endl;
                cout << "  CRC Recibido: " << (int)crc_recibido << ", CRC Calculado: " << (int)crc_calculado << endl;
                continue; 
            }

            string longitud_destino_str = (apodo_remitente.size() < 10) ? "0"+to_string(apodo_remitente.size()) : to_string(apodo_remitente.size());
            string paquete_ack = "A" + seq_texto + longitud_destino_str + apodo_remitente;
            paquete_ack.append(BUFFER_SIZE - paquete_ack.size(), '#');
            
            cout << "client envio (ACK): " << paquete_ack.substr(0, 70) << "..." << endl;
            sendto(socket_cliente, paquete_ack.c_str(), paquete_ack.size(), 0, (const struct sockaddr *) &dir_servidor, sizeof(dir_servidor));

            if (bandeja_actual.fragmentos.count(num_secuencia)) {
                cout << "Aviso: Fragmento duplicado (seq " << num_secuencia << "). Omitiendo." << endl;
                continue;
            }

            bandeja_actual.fragmentos[num_secuencia] = datos_reales;
            bandeja_actual.datos_recibidos_actual += datos_reales.size();
            
            // +++ MODIFICADO: Lógica de ensamblaje +++
            if (es_ultimo_paquete) {
                cout << "Se recibió el último fragmento (seq " << num_secuencia << "). Total: " 
                     << bandeja_actual.datos_recibidos_actual << "/" << bandeja_actual.tamano_total_esperado << endl;

                if (bandeja_actual.datos_recibidos_actual == bandeja_actual.tamano_total_esperado) {
                    cout << "\n¡Transferencia completa! Ensamblando '" << bandeja_actual.nombre_archivo << "'..." << endl;
                    
                    string datos_archivo_completos;
                    // Asumimos que los fragmentos llegaron en orden gracias a Stop-and-Wait
                    // (Si usáramos Window, tendríamos que reordenar el map aquí)
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
                } else {
                    cout << "Error: Se recibió el último paquete, pero el tamaño total no coincide. Total: " 
                         << bandeja_actual.datos_recibidos_actual << "/" << bandeja_actual.tamano_total_esperado << "..." << endl;
                }
            }
            // ---
        } 
        
        else if (bytes_recibidos > 0) {
            char tipo_paquete = paquete_recibido[0];

            if (tipo_paquete == 'A') { 
                string seq_ack_str = paquete_recibido.substr(1, 5);
                int seq_ack_num = stoi(seq_ack_str);
                int longitud_apodo = stoi(paquete_recibido.substr(6, 2));
                string apodo_origen = paquete_recibido.substr(8, longitud_apodo);
                cout << "\n[ACK Recibido] de " << apodo_origen << " para el fragmento seq=" << seq_ack_str << endl;
                {
                    std::lock_guard<std::mutex> lock(mtx_acks);
                    acks_recibidos_para_envio_actual.insert(seq_ack_num);
                }
            }
            // ... (Resto de 'M', 'T', 'L', 'V', 'v', 'O' no cambia) ...
            else if (tipo_paquete == 'M') { 
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

struct PaqueteEnVuelo {
    string paquete_completo;
    chrono::system_clock::time_point tiempo_envio;
    int reintentos = 0;
};

int main(int argc, char* argv[]) {
    // ... (Conexión inicial y lógica de apodo no cambia) ...
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
    cout << "client envio: " << paquete_apodo.substr(0, 70) << "..." << endl;
    sendto(socket_cliente, paquete_apodo.c_str(), paquete_apodo.size(), 0, (const struct sockaddr *) &dir_servidor, sizeof(dir_servidor));
    // --- FIN DE CONEXIÓN INICIAL ---


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
        char tipo_paquete_menu; // Renombrado para evitar conflicto

        // ... (Opciones 1, 2, 3, 4 no cambian) ...
        if (opcion_menu == 1) { 
            tipo_paquete_menu = 'n';
            cout << "Ingresa tu nuevo alias: ";
            cin >> contenido;
            int tamNick = contenido.size();
            longitud_str = (tamNick < 10) ? "0" + to_string(tamNick) : to_string(tamNick);
            paquete_a_enviar = string(1, tipo_paquete_menu) + longitud_str + contenido;
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
            paquete_a_enviar = string(1, tipo_paquete_menu) + longitud_destino_str + apodo_destino + longitud_str + contenido;
        } else if (opcion_menu == 3) { 
            tipo_paquete_menu = 'm';
            cout << "Mensaje para todos: ";
            cin.ignore();
            getline(cin, contenido);
            int longitud_mensaje = contenido.size();
            longitud_str = (longitud_mensaje < 10) ? "00" + to_string(longitud_mensaje) : (longitud_mensaje < 100) ? "0" + to_string(longitud_mensaje) : to_string(longitud_mensaje);
            paquete_a_enviar = string(1, tipo_paquete_menu) + longitud_str + contenido;
        } else if (opcion_menu == 4) { 
            tipo_paquete_menu = 'l';
            paquete_a_enviar = string(1, tipo_paquete_menu);
        } 
        
        else if(opcion_menu == 5){ 
            
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

            // IMPORTANTE: El tipo 'f'/'e' ahora se define DENTRO del bucle
            // string encabezado_archivo = string(1, 'f') + ... // <-- Eliminamos esto
            
            // Este es el encabezado base *sin* el tipo de paquete
            string encabezado_base = longitud_destino_str + apodo_destino + longitud_nombre_archivo_str + nombre_archivo_local + tamano_archivo_str;
            
            // -1 para el tipo, -5 seq, -3 CRC
            int tamano_fragmento_datos = BUFFER_SIZE - (1 + encabezado_base.size() + 5 + 3); 
            
            {
                std::lock_guard<std::mutex> lock(mtx_acks);
                acks_recibidos_para_envio_actual.clear();
            }

            map<int, PaqueteEnVuelo> bufer_envio; 
            int num_secuencia_actual = 0;
            int bytes_enviados_confirmados = 0;
            bool transferencia_exitosa = true;

            cout << "Iniciando transferencia fiable (Stop-and-Wait) de " << tamano_archivo << " bytes..." << endl;

            while (bytes_enviados_confirmados < tamano_archivo || !bufer_envio.empty()) {

                if (!acks_recibidos_para_envio_actual.empty()) {
                    std::lock_guard<std::mutex> lock(mtx_acks);
                    for (int seq_ack : acks_recibidos_para_envio_actual) {
                        if (bufer_envio.count(seq_ack)) {
                            // ACK recibido!
                            // Calculamos cuánto enviamos en ese paquete
                            string pkg_enviado = bufer_envio[seq_ack].paquete_completo;
                            int header_len = 1 + encabezado_base.size() + 5 + 3;
                            int datos_len = pkg_enviado.size() - header_len;
                            
                            // Si tiene padding, hay que restar el padding
                            if (pkg_enviado[5] == 'e') { // 'e' es el tipo
                                size_t pos_pad = pkg_enviado.find('#', header_len);
                                if(pos_pad != string::npos){
                                    datos_len = pos_pad - header_len;
                                }
                            }
                            
                            bytes_enviados_confirmados += datos_len;
                            bufer_envio.erase(seq_ack); 
                            cout << "-> ACK " << seq_ack << " recibido. (" << bytes_enviados_confirmados << "/" << tamano_archivo << ")" << endl;
                        }
                    }
                    acks_recibidos_para_envio_actual.clear();
                }

                auto bufer_envio_copia = bufer_envio; 
                for (auto& par : bufer_envio_copia) {
                    int seq_num = par.first;
                    PaqueteEnVuelo& paquete = par.second;
                    auto ahora = chrono::system_clock::now();
                    auto tiempo_transcurrido = chrono::duration_cast<chrono::milliseconds>(ahora - paquete.tiempo_envio).count();

                    if (tiempo_transcurrido > TIMEOUT_MS) {
                        if (paquete.reintentos >= MAX_REINTENTOS) {
                            cout << "\n¡ERROR! Paquete " << seq_num << " ha fallado " << MAX_REINTENTOS << " veces. Abortando transferencia." << endl;
                            transferencia_exitosa = false;
                            break; 
                        }
                        paquete.reintentos++;
                        paquete.tiempo_envio = chrono::system_clock::now();
                        bufer_envio[seq_num] = paquete; 
                        cout << "\n¡TIMEOUT! Retransmitiendo paquete seq=" << seq_num 
                             << " (Intento " << paquete.reintentos << ")" << endl;
                        cout << "client envio (RE): " << paquete.paquete_completo.substr(0, 70) << "..." << endl;
                        sendto(socket_cliente, paquete.paquete_completo.c_str(), paquete.paquete_completo.size(), 0, (const struct sockaddr *) &dir_servidor, sizeof(dir_servidor));
                    }
                }
                
                if (!transferencia_exitosa) {
                    bufer_envio.clear(); 
                    break;
                }

                if (bufer_envio.empty() && bytes_enviados_confirmados < tamano_archivo) {
                    
                    string seq_como_texto = to_string(num_secuencia_actual);
                    string seq_texto_formato = string(5 - seq_como_texto.size(), '0') + seq_como_texto;
                    
                    int tamano_fragmento_actual = min(tamano_fragmento_datos, tamano_archivo - bytes_enviados_confirmados);
                    string fragmento_datos = datos_archivo.substr(bytes_enviados_confirmados, tamano_fragmento_actual);
                    
                    unsigned char crc = calcular_crc(fragmento_datos);
                    string crc_str = crc_a_texto(crc);
                    
                    // +++ MODIFICADO: Decidir tipo 'f' o 'e' +++
                    bool es_ultimo = (bytes_enviados_confirmados + tamano_fragmento_actual == tamano_archivo);
                    char tipo_fragmento = (es_ultimo) ? 'e' : 'f';

                    string paquete_fragmento = seq_texto_formato + string(1, tipo_fragmento) + encabezado_base + crc_str + fragmento_datos;
                    // ---
                    
                    if (es_ultimo) {
                        paquete_fragmento.append(BUFFER_SIZE - paquete_fragmento.size(), '#');
                    }

                    PaqueteEnVuelo paquete_nuevo;
                    paquete_nuevo.paquete_completo = paquete_fragmento;
                    paquete_nuevo.tiempo_envio = chrono::system_clock::now();
                    paquete_nuevo.reintentos = 0;
                    
                    bufer_envio[num_secuencia_actual] = paquete_nuevo; 

                    cout << "client envio (NUEVO): " << paquete_fragmento.substr(0, 70) 
                         << "... (seq=" << seq_texto_formato << ", crc=" << (int)crc << (es_ultimo ? " (ULTIMO)" : "") << ")\n";

                    sendto(socket_cliente, paquete_fragmento.c_str(), paquete_fragmento.size(), 0, (const struct sockaddr *) &dir_servidor, sizeof(dir_servidor));
                    
                    num_secuencia_actual++; 
                }

                usleep(10000); // 10ms
            }

            if (transferencia_exitosa) {
                cout << "\n¡Transferencia de archivo fiable completada!" << endl;
            } else {
                cout << "\nTransferencia de archivo fallida." << endl;
            }

            continue; 
        }
        
        // ... (Opciones 6, 7, 8 no cambian) ...
        else if (opcion_menu == 6) { 
            tipo_paquete_menu = 'p';
            paquete_a_enviar = string(1, tipo_paquete_menu);
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
            tipo_paquete_menu = 'x';
            paquete_a_enviar = string(1, tipo_paquete_menu);
            paquete_a_enviar.append(BUFFER_SIZE - paquete_a_enviar.size(), '#');
            cout << "client envio: " << paquete_a_enviar.substr(0, 70) << "..." << endl;
            sendto(socket_cliente, paquete_a_enviar.c_str(), paquete_a_enviar.size(), 0, (const struct sockaddr *) &dir_servidor, sizeof(dir_servidor));
            break; 
        } else {
            cout << "Opción no reconocida." << endl;
            continue;
        }
        
        paquete_a_enviar.append(BUFFER_SIZE - paquete_a_enviar.size(), '#');
        cout << "client envio: " << paquete_a_enviar.substr(0, 70) << "..." << endl;
        sendto(socket_cliente, paquete_a_enviar.c_str(), paquete_a_enviar.size(), 0, (const struct sockaddr *) &dir_servidor, sizeof(dir_servidor));
    }

    close(socket_cliente);
    cout << "Conexión cerrada." << endl;
    exit(0); 
    return 0;
}