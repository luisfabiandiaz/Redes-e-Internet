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
#include <unordered_map>
using namespace std;

unordered_map<string, int> clientesConectados;

void broadcast(const string& mensaje, int emisor = -1) {
    for (auto& cliente : clientesConectados) {
        if (cliente.second != emisor) {
            send(cliente.second, mensaje.c_str(), mensaje.size(), 0);
        }
    }
}

void manejarCliente(int socketCliente) {
    char buffer[1024];
    string nick = "Anon";

    while (true) {
        int bytesLeidos = recv(socketCliente, buffer, 1, 0); 
        if (bytesLeidos <= 0) {
            string aviso = "X" + ((nick.size()<10) ? "0"+to_string(nick.size()):to_string(nick.size())) + nick;
            broadcast(aviso, socketCliente);
            clientesConectados.erase(nick);
            cout << nick << " se desconectó." << endl;
            close(socketCliente);
            return;
        }

        char tipoMensaje = buffer[0];

        if (tipoMensaje == 'n') { 
            recv(socketCliente, buffer, 2, 0);
            int tamNick = stoi(string(buffer, 2));

            recv(socketCliente, buffer, tamNick, 0);
            string nuevoNick(buffer, tamNick);
            clientesConectados.erase(nick);
            nick = nuevoNick;
            clientesConectados[nick] = socketCliente;

            cout << "Nuevo nick: " << nick << endl;
            string aviso = "N" + ((nick.size()<10) ? "0"+to_string(nick.size()):to_string(nick.size()))+ nick;
            broadcast(aviso, socketCliente);
        } 
        else if (tipoMensaje == 'm') { 
            recv(socketCliente, buffer, 3, 0);
            int tamMensaje = stoi(string(buffer, 3));

            recv(socketCliente, buffer, tamMensaje, 0);
            string mensaje(buffer, tamMensaje);

            cout << nick << ": " << mensaje << endl;

            string enviar = "M" + ((nick.size()<10) ? "0"+to_string(nick.size()):to_string(nick.size()))+ nick + ((mensaje.size()<10) ? "00"+to_string(mensaje.size()):(mensaje.size()<100)?"0"+to_string(mensaje.size()):to_string(mensaje.size())) + mensaje;
            broadcast(enviar, socketCliente);
        } 
        else if (tipoMensaje == 't') { 
            recv(socketCliente, buffer, 2, 0);
            int tamDest = stoi(string(buffer, 2));

            recv(socketCliente, buffer, tamDest, 0);
            string destinatario(buffer, tamDest);

            recv(socketCliente, buffer, 3, 0);
            int tamMensaje = stoi(string(buffer, 3));

            recv(socketCliente, buffer, tamMensaje, 0);
            string mensaje(buffer, tamMensaje);

            string enviar = "T" + ((nick.size()<10) ? "0"+to_string(nick.size()):to_string(nick.size()))+ nick+ ((mensaje.size()<10) ? "00"+to_string(mensaje.size()):(mensaje.size()<100)?"0"+to_string(mensaje.size()):to_string(mensaje.size()))+ mensaje;

            if (clientesConectados.count(destinatario))
                send(clientesConectados[destinatario], enviar.c_str(), enviar.size(), 0);
        } 
        else if (tipoMensaje == 'l') { 
            string enviar = "L";
            string cantidad = (clientesConectados.size()<10) ? "0"+to_string(clientesConectados.size()) : to_string(clientesConectados.size());
            enviar += cantidad;

            for (auto& cliente : clientesConectados) {
                string tamNick = (cliente.first.size()<10) ? "0"+to_string(cliente.first.size()):to_string(cliente.first.size());
                enviar += tamNick + cliente.first;
            }
            send(socketCliente, enviar.c_str(), enviar.size(), 0);
        } 
        else if (tipoMensaje == 'x') { 
            string aviso = "X" + ((nick.size()<10) ? "0"+to_string(nick.size()):to_string(nick.size()))+ nick;
            broadcast(aviso, socketCliente);

            clientesConectados.erase(nick);
            cout << nick << " salió." << endl;
            close(socketCliente);
            return;
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        cerr << "Uso: " << argv[0] << " <puerto>\n";
        return 1;
    }

    int puerto = atoi(argv[1]);
    int socketServidor = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in direccionServidor{};
    direccionServidor.sin_family = AF_INET;
    direccionServidor.sin_port = htons(puerto);
    direccionServidor.sin_addr.s_addr = INADDR_ANY;

    if (bind(socketServidor, (sockaddr*)&direccionServidor, sizeof(direccionServidor)) == -1) {
        perror("Error en bind");
        close(socketServidor);
        exit(1);
    }

    listen(socketServidor, 10);
    cout << "servidor activado en puerto: " << puerto << endl;

    while (true) {
        int socketNuevoCliente = accept(socketServidor, nullptr, nullptr);
        cout << "Nuevo cliente conectado" << endl;

        thread t(manejarCliente, socketNuevoCliente);
        t.detach();
    }

    close(socketServidor);
    return 0;
}
