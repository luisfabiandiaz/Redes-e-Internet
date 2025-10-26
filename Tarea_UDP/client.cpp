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

#define MAXLINE 1024

string simbolo = "_";
const int tamTablero = 3;

struct sockaddr_in	 servaddr;
socklen_t len_servaddr = sizeof(servaddr);

struct FileFragmentBuffer {
    map<int, string> fragments; 
    int expectedTotalSize = 0;
    int currentDataSize = 0;
    string fileName;
    string senderNick;
    bool lastFragmentReceived = false;
};


void showMenu() {
    cout << "\n---------------------------------" << endl;
    cout << "1. Cambiar apodo" << endl;
    cout << "2. Enviar mensaje privado" << endl;
    cout << "3. Enviar mensaje a todos" << endl;
    cout << "4. Ver lista de participantes" << endl;
    cout << "5. Enviar un archivo" << endl;
    cout << "6. Unirse a una partida" << endl;
    cout << "7. Realizar jugada" << endl;
    cout << "8. Desconectar" << endl;
    cout << "---------------------------------" << endl;
    cout << "Elige una opción: ";
}

void recibirMensajes(int socketCliente) {
    char buffer[MAXLINE];
    
    static unordered_map <string, FileFragmentBuffer> file_reassembly_buffers;

    while (true) {
        memset(buffer, 0, sizeof(buffer));
        int n = recvfrom(socketCliente, buffer, MAXLINE, 0, (struct sockaddr *) &servaddr, &len_servaddr);

        if (n <= 0) {
            cout << "\nError al recibir del servidor." << endl;
            close(socketCliente);
            return;
        }
        
        // <<< NOTA: No es necesario cambiar nada aquí >>>
        // El servidor enviará sus mensajes (lista, chat, etc.) también con relleno,
        // pero como tu lógica de parseo aquí se basa en longitudes explícitas
        // (ej. stoi(cadena.substr(1, 2))), simplemente ignorará el relleno '#'
        
        string cadena(buffer, n); 
        cout.flush();

        cout << "\n[CLIENT RECV]: " << cadena.substr(0, 70) 
             << (cadena.size() > 70 ? "..." : "") 
             << " (size=" << n << ")" << endl;

        if (n > 0 && isdigit(cadena[0])) {
            string seq_m = cadena.substr(0, 5);
            int seq = stoi(seq_m);
            
            char tipo_paquete = cadena[5]; 

            if (tipo_paquete != 'f') { 
                 cerr << "Error: Paquete con seq pero no es tipo 'f'" << endl;
                 continue;
            }

            int offset = 6; 
            int tamNick = stoi(cadena.substr(offset, 2));
            offset += 2;
            string nick = cadena.substr(offset, tamNick);
            offset += tamNick;

            int tamFileName = stoi(cadena.substr(offset, 2));
            offset += 2;
            string fileName = cadena.substr(offset, tamFileName);
            offset += tamFileName;

            int fileSize = stoi(cadena.substr(offset, 10));
            offset += 10;

            string file_chunk_data = cadena.substr(offset);
            
            string transfer_key = nick + ":" + fileName;

            if (file_reassembly_buffers.count(transfer_key) == 0) {
                FileFragmentBuffer new_buffer;
                new_buffer.expectedTotalSize = fileSize;
                new_buffer.fileName = fileName;
                new_buffer.senderNick = nick;
                file_reassembly_buffers[transfer_key] = new_buffer;
                cout << "Iniciando recepción de archivo '" << fileName << "' de " << nick << endl;
            }

            FileFragmentBuffer& buffer = file_reassembly_buffers[transfer_key];

            if (buffer.fragments.count(seq)) {
                cout << "Advertencia: Recibido fragmento duplicado (seq " << seq << "). Ignorando." << endl;
                continue;
            }

            // Esta lógica ya maneja el padding '#' correctamente
            size_t padding_pos = file_chunk_data.find('#');
            bool is_last = (padding_pos != string::npos);
            string actual_data = (is_last) ? file_chunk_data.substr(0, padding_pos) : file_chunk_data;

            buffer.fragments[seq] = actual_data;
            buffer.currentDataSize += actual_data.size();
            
            if (is_last) {
                buffer.lastFragmentReceived = true;
                cout << "Recibido último fragmento (seq " << seq << "). Tamaño actual: " 
                     << buffer.currentDataSize << "/" << buffer.expectedTotalSize << endl;
            }

            if (buffer.lastFragmentReceived && buffer.currentDataSize == buffer.expectedTotalSize) {
                cout << "\n¡Archivo completo! Reensamblando '" << buffer.fileName << "'..." << endl;
                
                string fullFileData;
                for (const auto& pair : buffer.fragments) {
                    fullFileData.append(pair.second);
                }

                if (fullFileData.size() == buffer.expectedTotalSize) {
                    ofstream out("recv_" + buffer.fileName, ios::binary);
                    out.write(fullFileData.c_str(), fullFileData.size());
                    out.close();

                    cout << "Archivo recibido de " << buffer.senderNick << ": " << buffer.fileName 
                         << " (" << fullFileData.size() << " bytes) guardado como recv_" << buffer.fileName << "\n";
                } else {
                    cout << "Error de reensamblaje: Tamaño final no coincide. Esperado: " 
                         << buffer.expectedTotalSize << ", Obtenido: " << fullFileData.size() << endl;
                }
                
                file_reassembly_buffers.erase(transfer_key);
            
            } else if (buffer.lastFragmentReceived) {
                cout << "Error: Faltan paquetes. Tamaño actual: " 
                     << buffer.currentDataSize << "/" << buffer.expectedTotalSize << ". Esperando más paquetes..." << endl;
            }

        } 
        
        else if (n > 0) {
            char tipo = cadena[0];

            if (tipo == 'M') {
            
                int tamNick = stoi(cadena.substr(1, 2));
                string nick = cadena.substr(3, tamNick);
                int tamMsg = stoi(cadena.substr(3 + tamNick, 3));
                string msg = cadena.substr(3 + tamNick + 3, tamMsg);
                cout << "\n[" << nick << " dice]: " << msg << endl;

            } else if (tipo == 'T') {
            
                int tamNick = stoi(cadena.substr(1, 2));
                string nick = cadena.substr(3, tamNick);
                int tamMsg = stoi(cadena.substr(3 + tamNick, 3));
                string msg = cadena.substr(3 + tamNick + 3, tamMsg);
                cout << "\n[MENSAJE PRIVADO de " << nick << "]: " << msg << endl;

            } else if (tipo == 'L') {
            
                int cantClientes = stoi(cadena.substr(1, 2));
                cout << "\n--- Participantes Conectados (" << cantClientes << ") ---" << endl;
                int offset = 3;
                for (int i = 0; i < cantClientes; ++i) {
                    int tamNick = stoi(cadena.substr(offset, 2));
                    offset += 2;
                    string nick = cadena.substr(offset, tamNick);
                    offset += tamNick;
                    cout << "- " << nick << endl;
                }
                cout << "-------------------------------------" << endl;

            } else if (tipo == 'V') {
              
                simbolo = cadena.substr(1, 1);
                cout << "\n¡Es tu turno! Juegas con '" << simbolo << "'" << endl;

            } else if (tipo == 'v') {
            
                string tablero_str = cadena.substr(1, tamTablero * tamTablero); // Leemos solo los bytes del tablero
                cout << "\n--- Tablero Actual ---" << endl;
                for (int i = 0; i < tamTablero * tamTablero; i++) {
                    cout << " " << tablero_str[i] << " ";
                    if ((i + 1) % tamTablero == 0) {
                        cout << endl;
                        if (i < tamTablero * tamTablero - 1){
                            for(int k=0; k<tamTablero; ++k) cout << "---";
                            cout << endl;
                        }
                    } else {
                        cout << "|";
                    }
                }
                cout << "----------------------" << endl;

            } else if (tipo == 'F') {
                cerr << "Advertencia: Recibido paquete de archivo 'F' simple no fragmentado" << endl;

            } else if (tipo == 'O') {
                simbolo = "_";
                string result = cadena.substr(1, 3);
                if(result == "win"){
                    cout << "\n¡FELICIDADES! ¡HAS GANADO LA PARTIDA!" << endl;
                }
                else if (result == "los") {
                    cout << "\nHas perdido la partida. ¡Mejor suerte la próxima!" << endl;
                }
                else if (result == "emp") {
                    cout << "\nLa partida ha terminado en EMPATE." << endl;
                }
            }
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        cerr << "Uso: " << argv[0] << "<puerto>\n";
        return 1;
    }
    
    char* host_ip = "127.0.0.1";
    int port = atoi(argv[1]);
    struct hostent *host = (struct hostent *)gethostbyname(host_ip);
    
    char buffer[MAXLINE];

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("Fallo al crear socket");
        exit(EXIT_FAILURE);
    }

    memset(&servaddr, 0, sizeof(servaddr));

    servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(port);
    servaddr.sin_addr = *((struct in_addr *)host->h_addr);

    thread t(recibirMensajes, sock);
    t.detach();

    cout << "Introduce tu apodo: ";
    string Nicknamei;
    cin >> Nicknamei;
    string tamnick = (Nicknamei.size() < 10) ? "0" + to_string(Nicknamei.size()) : to_string(Nicknamei.size());
    string enviarnick = string(1, 'n') + tamnick + Nicknamei;
    
    enviarnick.append(MAXLINE - enviarnick.size(), '#');
    
    cout << "[CLIENT SEND]: " << enviarnick.substr(0, 70) 
         << "... (size=" << enviarnick.size() << ")" << endl;
    
    sendto(sock, enviarnick.c_str(), enviarnick.size(), 0, (const struct sockaddr *) &servaddr, sizeof(servaddr));


    int entrada;
    while (true) {
        showMenu();
        cin >> entrada;

        if (cin.fail()) {
            cout << "Entrada inválida. Por favor, introduce un número." << endl;
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            continue;
        }

        string cont, tamaño, enviar, destin, tamañoDestin;
        char tipo;

        if (entrada == 1) {
            tipo = 'n';
            cout << "Introduce tu nuevo apodo: ";
            cin >> cont;
            int tamNick = cont.size();
            tamaño = (tamNick < 10) ? "0" + to_string(tamNick) : to_string(tamNick);
            enviar = string(1, tipo) + tamaño + cont;
        } else if (entrada == 2) {
            tipo = 't';
            cout << "Destinatario: ";
            cin >> destin;
            cout << "Mensaje: ";
            cin.ignore();
            getline(cin, cont);
            int tamDest = destin.size();
            int tamMsg = cont.size();
            tamañoDestin = (tamDest < 10) ? "0" + to_string(tamDest) : to_string(tamDest);
            tamaño = (tamMsg < 10) ? "00" + to_string(tamMsg) : (tamMsg < 100) ? "0" + to_string(tamMsg) : to_string(tamMsg);
            enviar = string(1, tipo) + tamañoDestin + destin + tamaño + cont;
        } else if (entrada == 3) {
            tipo = 'm';
            cout << "Mensaje para todos: ";
            cin.ignore();
            getline(cin, cont);
            int tamMsg = cont.size();
            tamaño = (tamMsg < 10) ? "00" + to_string(tamMsg) : (tamMsg < 100) ? "0" + to_string(tamMsg) : to_string(tamMsg);
            enviar = string(1, tipo) + tamaño + cont;
        } else if (entrada == 4) {
            tipo = 'l';
            enviar = string(1, tipo);
        } 
        else if(entrada == 5){
            tipo = 'f';
            cout << "Destinatario: ";
            cin >> destin;
            cout << "Nombre del archivo (en la misma carpeta): ";
            string fileName;
            cin >> fileName;

            ifstream in(fileName, ios::binary);
            if (!in) {
                cout << "Error: no se pudo abrir el archivo " << fileName << endl;
                continue;
            }

            in.seekg(0, ios::end);
            int fileSize = in.tellg();
            in.seekg(0, ios::beg);
            string fileData(fileSize, '\0');
            in.read(&fileData[0], fileSize);
            in.close();

            string tamDestin = (destin.size() < 10) ? "0"+to_string(destin.size()) : to_string(destin.size());
            string tamFileName = (fileName.size() < 10) ? "0"+to_string(fileName.size()) : to_string(fileName.size());
            string tamFile = string(10 - to_string(fileSize).size(), '0') + to_string(fileSize);

            string file_header = string(1, tipo) + tamDestin + destin + tamFileName + fileName + tamFile;

            int data_chunk_size = MAXLINE - file_header.size() - 5; 
            int seq = 0;
            int bytes_sent = 0;

            while (bytes_sent < fileSize) {
                string seq_str = to_string(seq);
                string seq_m = string(5 - seq_str.size(), '0') + seq_str;
                
                int current_chunk_data_size = min(data_chunk_size, fileSize - bytes_sent);
                
                string file_chunk = fileData.substr(bytes_sent, current_chunk_data_size);
                
                string packet_to_send = seq_m + file_header + file_chunk;
                
                bool is_last = (bytes_sent + current_chunk_data_size == fileSize);

                if (is_last) {
                    packet_to_send.append(MAXLINE - packet_to_send.size(), '#');
                }

                cout << "[CLIENT SEND]: " << packet_to_send.substr(0, 70) 
                     << "... (seq=" << seq_m << ", size=" << packet_to_send.size() << (is_last ? " [LAST]" : "") << ")\n";

                sendto(sock, packet_to_send.c_str(), packet_to_send.size(), 0, (const struct sockaddr *) &servaddr, sizeof(servaddr));

                bytes_sent += current_chunk_data_size;
                seq++;

                usleep(1000); 
            }
            
            continue; 
        }
        else if (entrada == 6) {
            tipo = 'p';
            enviar = string(1, tipo);
        } else if (entrada == 7) {
            if (simbolo == "_") {
                cout << "No es tu turno o no estás en una partida." << endl;
                continue;
            }
            cout << "Introduce tu jugada (posición 1-" << tamTablero * tamTablero << "): ";
            int pos;
            cin >> pos;
            enviar = string(1, 'w') + simbolo + to_string(pos);
        } else if (entrada == 8) {
            tipo = 'x';
            enviar = string(1, tipo);
            
            enviar.append(MAXLINE - enviar.size(), '#');

            cout << "[CLIENT SEND]: " << enviar.substr(0, 70) 
                 << "... (size=" << enviar.size() << ")" << endl;
            
            sendto(sock, enviar.c_str(), enviar.size(), 0, (const struct sockaddr *) &servaddr, sizeof(servaddr));
            break; 
        } else {
            cout << "Opción no válida." << endl;
            continue;
        }
        
        enviar.append(MAXLINE - enviar.size(), '#');
        
        cout << "[CLIENT SEND]: " << enviar.substr(0, 70) 
             << "... (size=" << enviar.size() << ")" << endl;
        sendto(sock, enviar.c_str(), enviar.size(), 0, (const struct sockaddr *) &servaddr, sizeof(servaddr));
    }

    close(sock);
    cout << "Desconectado." << endl;
    exit(0); 
    return 0;
}