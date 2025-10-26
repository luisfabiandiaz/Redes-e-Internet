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
#include <fstream>
#include <cstring>
#include <vector>

using namespace std;

// 1. Clase Sillón
class Sillon {
public:
    int int_sillon;
    char char_sillon;
    float float_sillon;

    Sillon(int is, char cs, float fs) 
    : int_sillon(is), char_sillon(cs), float_sillon(fs) {}
};

// 2. Clase Mesa
class Mesa {
public:
    int int_mesa;
    char char_mesa;
    float float_mesa;

    Mesa(int im, char cm, float fm) 
    : int_mesa(im), char_mesa(cm), float_mesa(fm) {}
};

// 3. Clase Cocina
class Cocina {
public:
    int int_cocina;
    char char_cocina;
    float float_cocina;

    Cocina(int ic, char cc, float fc) 
    : int_cocina(ic), char_cocina(cc), float_cocina(fc) {}
};

// 4. Clase Sala
class Sala {
public:
    int n;
    char str[1000];
    Sillon s;
    Mesa m;
    Cocina* c; // Puntero a un objeto de tipo Cocina

    Sala(int n_n, const char* n_str, const Sillon& n_s, const Mesa& n_m, Cocina* n_c)
        : n(n_n), s(n_s), m(n_m), c(n_c) {
        std::strncpy(str, n_str, sizeof(str) - 1);
        str[sizeof(str) - 1] = '\0'; 
    }
};

// --- Funciones para la serialización de bits (MÉTODO INCORRECTO) ---

// Función para serializar el objeto Sala a un string
string serializarSala(const Sala& sala) {
    size_t sala_size = sizeof(sala);
    char* aux_ptr = (char*)(&sala);
    string serialized_data;
    serialized_data.resize(sala_size);
    for (size_t i = 0; i < sala_size; ++i) {
        serialized_data[i] = *(aux_ptr + i);
    }
    return serialized_data;
}

// Función para deserializar el string a un objeto Sala
// CUIDADO: ESTO ES INSEGURO Y PUEDE CAUSAR ERRORES
Sala deserializarSala(const string& serialized_data) {
    Sala sala_recibida = *reinterpret_cast<const Sala*>(serialized_data.data());
    return sala_recibida;
}
// -----------------------------------------------------------------

void showMenu(){
    cout<<"1. Change your nickname"<<endl;
    cout<<"2. Send message to someone"<<endl;
    cout<<"3. Send broadcast"<<endl;
    cout<<"4. Show list"<<endl;
    cout<<"5. Send a file"<<endl;
    cout<<"6. Exit"<<endl;
    cout<<"7. Send an object (experimental)"<<endl; // Nueva opción
}

void recibirMensajes(int socketCliente) {
    char buffer[2048];

    while (true) {
        int n = recv(socketCliente, buffer, 1, 0); 
        if (n <= 0) {
            cout << "Desconectado del servidor" << endl;
            close(socketCliente);
            exit(0);
        }
        char tipo = buffer[0];

        if (tipo == 'M') {
            recv(socketCliente, buffer, 2, 0);
            int tamNick = stoi(string(buffer, 2));
            recv(socketCliente, buffer, tamNick, 0);
            string nick(buffer, tamNick);
            recv(socketCliente, buffer, 3, 0);
            int tamMsg = stoi(string(buffer, 3));
            recv(socketCliente, buffer, tamMsg, 0);
            string msg(buffer, tamMsg);

            string crudo = "M" 
                + ((nick.size()<10) ? "0"+to_string(nick.size()):to_string(nick.size()))
                + nick
                + ((msg.size()<10) ? "00"+to_string(msg.size()):(msg.size()<100)?"0"+to_string(msg.size()):to_string(msg.size()))
                + msg;
            cout << crudo << endl;

            cout << "\nnick: " << nick << " msg: " << msg << endl;
        } 
        else if (tipo == 'T') {
            recv(socketCliente, buffer, 2, 0);
            int tamNick = stoi(string(buffer, 2));
            recv(socketCliente, buffer, tamNick, 0);
            string nick(buffer, tamNick);
            recv(socketCliente, buffer, 3, 0);
            int tamMsg = stoi(string(buffer, 3));
            recv(socketCliente, buffer, tamMsg, 0);
            string msg(buffer, tamMsg);

            string crudo = "T" 
                + ((nick.size()<10) ? "0"+to_string(nick.size()):to_string(nick.size()))
                + nick
                + ((msg.size()<10) ? "00"+to_string(msg.size()):(msg.size()<100)?"0"+to_string(msg.size()):to_string(msg.size()))
                + msg;
            cout << crudo << endl;

            cout << "\nmensaje privado de " << nick << " msg: " << msg << endl;
        } 
        else if (tipo == 'L') {
            recv(socketCliente, buffer, 2, 0);
            int cantClientes = stoi(string(buffer, 2));
            string cantStr = (cantClientes < 10) ? "0" + to_string(cantClientes) : to_string(cantClientes);

            string crudo = "L" + cantStr;
            cout << "\nnum de conectados: " << cantClientes << endl;

            for (int i = 0; i < cantClientes; ++i) {
                recv(socketCliente, buffer, 2, 0);
                int tamNick = stoi(string(buffer, 2));
                string tamNickStr = (tamNick < 10) ? "0" + to_string(tamNick) : to_string(tamNick);

                recv(socketCliente, buffer, tamNick, 0);
                string nick(buffer, tamNick);

                crudo += tamNickStr + nick;
                cout << nick << endl;
            }
            cout << crudo << endl;
        }
        else if (tipo == 'N') {
            recv(socketCliente, buffer, 2, 0);
            int tamNick = stoi(string(buffer, 2));
            recv(socketCliente, buffer, tamNick, 0);
            string nick(buffer, tamNick);

            string crudo = "N" 
                + ((nick.size()<10) ? "0"+to_string(nick.size()):to_string(nick.size()))
                + nick;
            cout << crudo << endl;

            cout << "\n" << nick << " cambio su nick" << endl;
        }
        else if (tipo == 'X') {
            recv(socketCliente, buffer, 2, 0);
            int tamNick = stoi(string(buffer, 2));
            recv(socketCliente, buffer, tamNick, 0);
            string nick(buffer, tamNick);

            string crudo = "X" 
                + ((nick.size()<10) ? "0"+to_string(nick.size()):to_string(nick.size()))
                + nick;
            cout << crudo << endl;

            cout << "\n" << nick << " se desconecto" << endl;
        }
        else if (tipo == 'E') {
            recv(socketCliente, buffer, 3, 0);
            int tamMsg = stoi(string(buffer, 3));
            recv(socketCliente, buffer, tamMsg, 0);
            string msg(buffer, tamMsg);

            string crudo = "E" 
                + ((msg.size()<10) ? "00"+to_string(msg.size()):(msg.size()<100)?"0"+to_string(msg.size()):to_string(msg.size()))
                + msg;
            cout << crudo << endl;

            cout << "\nERROR: " << msg << endl;
        }
        else if (tipo == 'F') {

            //remitente (2 bytes)
            recv(socketCliente, buffer, 2, 0);
            int tamRem = stoi(string(buffer, 2));
            recv(socketCliente, buffer, tamRem, 0);
            string remitente(buffer, tamRem);

            //nombre de archivo (3 bytes)
            recv(socketCliente, buffer, 3, 0);
            int tamFileName = stoi(string(buffer, 3));
            recv(socketCliente, buffer, tamFileName, 0);
            string fileName(buffer, tamFileName);

            //tamaño de archivo (10 bytes)
            recv(socketCliente, buffer, 10, 0);
            int fileSize = stoi(string(buffer, 10));

            //datos del archivo
            string fileData(fileSize, '\0');
            int recibidos = 0;
            while (recibidos < fileSize) {
                int n = recv(socketCliente, &fileData[recibidos], fileSize - recibidos, 0);
                recibidos += n;
            }

            //guardar archivo recibido
            ofstream out("recv_" + fileName, ios::binary);
            out.write(fileData.c_str(), fileSize);
            out.close();

            string stamFileName = to_string(fileName.size());
            cout << "F"
                << (remitente.size() < 10 ? "0" + to_string(remitente.size()) : to_string(remitente.size()))
                << remitente
                << string(3 - stamFileName.size(), '0') + stamFileName
                << fileName
                << string(10 - to_string(fileSize).size(), '0') + to_string(fileSize)
                << endl;
        }
        else if (tipo == 'O') {
            // --- Leer tamaño remitente ---
            recv(socketCliente, buffer, 2, 0);
            int tamRem = stoi(string(buffer, 2));

            // --- Leer nick remitente ---
            recv(socketCliente, buffer, tamRem, 0);
            string remitente(buffer, tamRem);

            // --- Leer tamaño objeto (10 bytes) ---
            recv(socketCliente, buffer, 10, 0);
            int tamObj = stoi(string(buffer, 10));

            // --- Leer objeto ---
            string objData(tamObj, '\0');
            int recibidos = 0;
            while (recibidos < tamObj) {
                int n = recv(socketCliente, &objData[recibidos], tamObj - recibidos, 0);
                recibidos += n;
            }

            cout << "Objeto recibido de " << remitente 
                << " (" << tamObj << " bytes)" << endl;

            // Deserializamos para ver contenido
            Sala obj = deserializarSala(objData);
            cout << "Sala recibida: n=" << obj.n 
                << ", str=" << obj.str 
                << ", sillon.int=" << obj.s.int_sillon 
                << ", mesa.char=" << obj.m.char_mesa 
                //<< ", cocina.float=" << obj.c->float_cocina 
                //da error por cositas
                << endl;
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        cerr << "Uso: " << argv[0] << " <puerto>\n";
        return 1;
    }

    int puerto = atoi(argv[1]);
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in stSockAddr{};
    stSockAddr.sin_family = AF_INET;
    stSockAddr.sin_port = htons(puerto);
    stSockAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(sock, (sockaddr*)&stSockAddr, sizeof(stSockAddr)) == -1) {
        cerr << "Error conectando al servidor\n";
        return 1;
    }

    cout << "Conectado al servidor!\n";

    thread t(recibirMensajes, sock);
    t.detach();

    int entrada;
    while (true) {
        showMenu();
        cin >> entrada;
        string cont, tamaño, enviar, destin, tamañoDestin;
        char tipo;
        if(entrada == 1){
            tipo='n';
            cout<<"Enter your nickname: ";
            cin>>cont;
            int tamNick = cont.size();
            tamaño = (tamNick < 10) ? "0" + to_string(tamNick) : to_string(tamNick);
            enviar = string(1, tipo) + tamaño + cont;
        }
        else if(entrada == 2){
            tipo='t';
            cout<<"Enter user: ";
            cin>>destin;
            cout<<"Enter your msg: ";
            cin.ignore();
            getline(cin,cont);
            int tamDest = destin.size();
            int tamMsg = cont.size();
            tamañoDestin = (tamDest < 10) ? "0" + to_string(tamDest) : to_string(tamDest);
            if (tamMsg < 10) tamaño = "00" + to_string(tamMsg);
            else if (tamMsg < 100) tamaño = "0" + to_string(tamMsg);
            else tamaño = to_string(tamMsg);
            enviar = string(1, tipo) + tamañoDestin + destin + tamaño + cont;
        }
        else if(entrada == 3){
            tipo='m';
            cout<<"Enter broadcast: ";
            cin.ignore();
            getline(cin,cont);
            int tamMsg = cont.size();
            if (tamMsg < 10) tamaño = "00" + to_string(tamMsg);
            else if (tamMsg < 100) tamaño = "0" + to_string(tamMsg);
            else tamaño = to_string(tamMsg);
            enviar = string(1, tipo) + tamaño + cont;
        }
        else if(entrada == 4){
            tipo='l';
            enviar = string(1, tipo);
        }
        else if (entrada == 5) {
            string dest, fileName;
            cout << "Ingrese destinatario: ";
            cin >> dest;
            cout << "Ingrese nombre del archivo (mismo directorio): ";
            cin >> fileName;

            ifstream in(fileName, ios::binary);
            if (!in) {
                cerr << "No se pudo abrir el archivo.\n";
                continue;
            }

            in.seekg(0, ios::end);
            int fileSize = in.tellg();
            in.seekg(0, ios::beg);

            string fileData(fileSize, '\0');
            in.read(&fileData[0], fileSize);

            string enviar = "f";

            enviar += (dest.size() < 10 ? "0" + to_string(dest.size()) : to_string(dest.size()));
            enviar += dest;

            string tamFileName = to_string(fileName.size());
            enviar += string(3 - tamFileName.size(), '0') + tamFileName;
            enviar += fileName;

            string tamFileSize = to_string(fileSize);
            enviar += string(10 - tamFileSize.size(), '0') + tamFileSize;

            enviar += fileData;

            send(sock, enviar.c_str(), enviar.size(), 0);

            cout << "f"
                << (dest.size() < 10 ? "0" + to_string(dest.size()) : to_string(dest.size()))
                << dest
                << string(3 - tamFileName.size(), '0') + tamFileName
                << fileName
                << string(10 - tamFileSize.size(), '0') + to_string(fileSize)
                << endl;
        }
        else if(entrada == 6){
            tipo='x';
            enviar = string(1, tipo);
            cout << enviar << endl;
            send(sock, enviar.c_str(), enviar.size(), 0);
            break;
        }
        else if (entrada == 7) {
            tipo = 'o'; // objeto

            // --- Pedimos destinatario ---
            cout << "Ingrese el nick del destinatario: ";
            string destinatario;
            cin >> destinatario;

            // Longitud en 2 bytes (padding con ceros)
            string tamDest = to_string(destinatario.size());
            tamDest = string(2 - tamDest.size(), '0') + tamDest;

            // --- Creamos objeto de ejemplo ---
            Sillon sillon_ejemplo(123, 'S', 150.5f);
            Mesa mesa_ejemplo(456, 'M', 75.0f);
            Cocina cocina_ejemplo(789, 'C', 2000.75f);
            Sala sala_ejemplo(101, "Sala de estar principal", sillon_ejemplo, mesa_ejemplo, &cocina_ejemplo);

            // Serializamos
            string datos_serializados = serializarSala(sala_ejemplo);

            // Tamaño en 10 bytes
            string tamObj = to_string(datos_serializados.size());
            tamObj = string(10 - tamObj.size(), '0') + tamObj;

            // --- Construcción del mensaje ---
            string enviar = string(1, tipo) + tamDest + destinatario + tamObj + datos_serializados;

            // Debug
            cout << "Enviando objeto a " << destinatario << endl;
            cout << "Mensaje crudo: o" << tamDest << destinatario 
                << tamObj << "[objeto binario]" << endl;

            // Envío
            send(sock, enviar.c_str(), enviar.size(), 0);
        }
        else {
            cout << "Opción no válida." << endl;
            continue;
        }

        if (entrada != 7) {
            cout << enviar << endl;
            send(sock, enviar.c_str(), enviar.size(), 0);
        }
    }

    close(sock);
    return 0;
}