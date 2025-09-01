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

void manejarCliente(int clienteSock) {
    char buffer[1024];
    string nickCliente = "Cliente";

    while (true) {
        int bytes = recv(clienteSock, buffer, sizeof(buffer) - 1, 0);
        if (bytes <= 0) {
            cout << nickCliente << " se desconecto" << endl;
            close(clienteSock);
            return;
        }
        buffer[bytes] = '\0';
        string recibido(buffer);

        char tipo = recibido[0];
        if (tipo == 'n') {
            int tam = stoi(recibido.substr(1, 2));
            nickCliente = recibido.substr(3, tam);
            cout << "(Se cambio nick): " << nickCliente << endl;
        } else if (tipo == 'm') {
            int tam = stoi(recibido.substr(1, 3));
            string msg = recibido.substr(4, tam);
            cout << nickCliente << ": " << msg << endl;
        }
    }
}

int main(int argc, char* argv[]) {
    int puerto = stoi(argv[1]);
    int servidor = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in stSockAddr{};
    stSockAddr.sin_family = AF_INET;
    stSockAddr.sin_port = htons(puerto);
    stSockAddr.sin_addr.s_addr = INADDR_ANY;

    bind(servidor, (sockaddr*)&stSockAddr, sizeof(stSockAddr));
    listen(servidor, 1);

    cout << "Esperando conexion\n";
    sockaddr_in direccionCliente;
    socklen_t tam = sizeof(direccionCliente);
    int clienteSock = accept(servidor, (sockaddr*)&direccionCliente, &tam);

    cout << "Cliente conectado\n";

    thread t(manejarCliente, clienteSock);
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

        send(clienteSock, enviar.c_str(), enviar.size(), 0);
    }

    close(clienteSock);
    close(servidor);
    return 0;
}