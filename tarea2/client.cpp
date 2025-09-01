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
using namespace std;


void recibirMensajes(int socketCliente) {
    char buffer[1024];
    string nickServidor = "Servidor";

    while (true) {
        int bytes = recv(socketCliente, buffer, sizeof(buffer) - 1, 0);
        if (bytes <= 0) {
            cout << "Desconectado del servidor" << endl;
            close(socketCliente);
            exit(0);
        }
        buffer[bytes] = '\0';
        string recibido(buffer);

        char tipo = recibido[0];
        if (tipo == 'n') {
            int tam = stoi(recibido.substr(1, 2));
            nickServidor = recibido.substr(3, tam);
            cout << "(El servidor cambio nick a): " << nickServidor << endl;
        } else if (tipo == 'm') {
            int tam = stoi(recibido.substr(1, 3));
            string msg = recibido.substr(4, tam);
            cout << nickServidor << ": " << msg << endl;
        }
    }
}

int main(int argc, char* argv[]) {
    int puerto = stoi(argv[1]);
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in stSockAddr{};
    stSockAddr.sin_family = AF_INET;
    stSockAddr.sin_port = htons(puerto);
    stSockAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(sock, (sockaddr*)&stSockAddr, sizeof(stSockAddr)) == -1) {
        cerr << "Error conectando al servidor\n";
        return 1;
    }

    cout << "Conectado al servidor\n";

    thread t(recibirMensajes, sock);
    t.detach();

    string entrada;
    while (true) {
        cout << "Ingrese mensaje: \n";
        cin >> entrada;

        if (entrada == "salir") break;

        char tipo = entrada[0];
        string contenido = entrada.substr(1);

        string tamaño, enviar;
        if (tipo == 'n') {
            int tamNick = contenido.size();
            if(tamNick < 10){
                tamaño = "0" + to_string(tamNick);
            }
            else{
                tamaño = to_string(tamNick);
            }
            enviar = string(1, tipo) + tamaño + contenido;
        } 
        else if (tipo == 'm') {
            int tamMsg = contenido.size();
            if (tamMsg < 10) {
                tamaño = "00" + to_string(tamMsg);
            }
            else if (tamMsg < 100){ 
                tamaño = "0" + to_string(tamMsg);
            }
            else{
                tamaño = to_string(tamMsg);
            }
            enviar = string(1, tipo) + tamaño + contenido;
        }

        send(sock, enviar.c_str(), enviar.size(), 0);
    }

    close(sock);
    return 0;
}