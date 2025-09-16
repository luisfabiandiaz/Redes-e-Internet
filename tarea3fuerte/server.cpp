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
unordered_map<string, int> clientes_conectados;

void broadcast(const string& msg, int emisor = -1) {
    for (auto& p : clientes_conectados) {
        if (p.second != emisor) {
            send(p.second, msg.c_str(), msg.size(), 0);
        }
    }
}

void manejarCliente(int client_socket) {
    char buffer[1024];
    string nick = "Anon";

    while (true) {
        int n = recv(client_socket, buffer, 1, 0); 
        if (n <= 0) {
            string aviso = "X" 
                + ((nick.size()<10) ? "0"+to_string(nick.size()):to_string(nick.size()))
                + nick;
            broadcast(aviso, client_socket);
            clientes_conectados.erase(nick);
            cout << nick << " se desconectó." << endl;
            close(client_socket);
            return;
        }

        char tipo = buffer[0];

        if (tipo == 'n') { 
            recv(client_socket, buffer, 2, 0);
            int tamNick = stoi(string(buffer, 2));
            recv(client_socket, buffer, tamNick, 0);
            string nuevoNick(buffer, tamNick);

            string recibido = "n" 
                + ((nuevoNick.size()<10) ? "0"+to_string(nuevoNick.size()):to_string(nuevoNick.size()))
                + nuevoNick;
            cout << recibido << endl;

            if (clientes_conectados.count(nuevoNick)) {
                string msg = "E" 
                    + ((string("nickname already in use").size()<10) ? "00"+to_string(string("nickname already in use").size()):(string("nickname already in use").size()<100)?"0"+to_string(string("nickname already in use").size()):to_string(string("nickname already in use").size()))
                    + "nickname already in use";
                send(client_socket, msg.c_str(), msg.size(), 0);
                cout << msg << endl;
                continue;
            }

            clientes_conectados.erase(nick);
            nick = nuevoNick;
            clientes_conectados[nick] = client_socket;

            cout << "Nuevo nick: " << nick << endl;
            string aviso = "N" 
                + ((nick.size()<10) ? "0"+to_string(nick.size()):to_string(nick.size()))
                + nick;
            broadcast(aviso, client_socket);
            cout << aviso << endl;
        } 
        else if (tipo == 'm') { 
            recv(client_socket, buffer, 3, 0);
            int tamMsg = stoi(string(buffer, 3));
            recv(client_socket, buffer, tamMsg, 0);
            string msg(buffer, tamMsg);

            string recibido = "m" 
                + ((msg.size()<10) ? "00"+to_string(msg.size()):(msg.size()<100)?"0"+to_string(msg.size()):to_string(msg.size()))
                + msg;
            cout << recibido << endl;

            string enviar = "M" 
                + ((nick.size()<10) ? "0"+to_string(nick.size()):to_string(nick.size()))
                + nick
                + ((msg.size()<10) ? "00"+to_string(msg.size()):(msg.size()<100)?"0"+to_string(msg.size()):to_string(msg.size()))
                + msg;
            broadcast(enviar, client_socket);
            cout << enviar << endl;
        } 
        else if (tipo == 't') { 
            recv(client_socket, buffer, 2, 0);
            int tamDest = stoi(string(buffer, 2));
            recv(client_socket, buffer, tamDest, 0);
            string dest(buffer, tamDest);
            recv(client_socket, buffer, 3, 0);
            int tamMsg = stoi(string(buffer, 3));
            recv(client_socket, buffer, tamMsg, 0);
            string msg(buffer, tamMsg);

            string recibido = "t" 
                + ((dest.size()<10) ? "0"+to_string(dest.size()):to_string(dest.size()))
                + dest
                + ((msg.size()<10) ? "00"+to_string(msg.size()):(msg.size()<100)?"0"+to_string(msg.size()):to_string(msg.size()))
                + msg;
            cout << recibido << endl;

            string enviar = "T" 
                + ((nick.size()<10) ? "0"+to_string(nick.size()):to_string(nick.size()))
                + nick
                + ((msg.size()<10) ? "00"+to_string(msg.size()):(msg.size()<100)?"0"+to_string(msg.size()):to_string(msg.size()))
                + msg;

            if (clientes_conectados.count(dest))
                send(clientes_conectados[dest], enviar.c_str(), enviar.size(), 0);
            cout << enviar << endl;
        } 
        else if (tipo == 'l') { 
            string recibido = "l";
            cout << recibido << endl;

            string enviar = "L";
            string cant = (clientes_conectados.size()<10) ? "0"+to_string(clientes_conectados.size()) : to_string(clientes_conectados.size());
            enviar += cant;
            for (auto& p : clientes_conectados) {
                string tamNick = (p.first.size()<10) ? "0"+to_string(p.first.size()):to_string(p.first.size());
                enviar += tamNick + p.first;
            }
            send(client_socket, enviar.c_str(), enviar.size(), 0);
            cout << enviar << endl;
        } 
        else if (tipo == 'x') { 
            string recibido = "x";
            cout << recibido << endl;

            string aviso = "X" 
                + ((nick.size()<10) ? "0"+to_string(nick.size()):to_string(nick.size()))
                + nick;
            broadcast(aviso, client_socket);
            clientes_conectados.erase(nick);
            cout << aviso << endl;
            cout << nick << " salió." << endl;
            close(client_socket);
            return;
        }
        else if (tipo == 'f') {
            // --- destinatario (2 bytes) ---
            recv(client_socket, buffer, 2, 0);
            int tamDest = stoi(string(buffer, 2));
            recv(client_socket, buffer, tamDest, 0);
            string dest(buffer, tamDest);

            // --- nombre de archivo (3 bytes) ---
            recv(client_socket, buffer, 3, 0);
            int tamFileName = stoi(string(buffer, 3));
            recv(client_socket, buffer, tamFileName, 0);
            string fileName(buffer, tamFileName);

            // --- tamaño de archivo (10 bytes) ---
            recv(client_socket, buffer, 10, 0);
            int fileSize = stoi(string(buffer, 10));

            // --- datos del archivo ---
            string fileData(fileSize, '\0');
            int recibidos = 0;
            while (recibidos < fileSize) {
                int n = recv(client_socket, &fileData[recibidos], fileSize - recibidos, 0);
                recibidos += n;
            }

            // --- buscar nick del remitente ---
            string nick;
            for (auto& p : clientes_conectados)
                if (p.second == client_socket) { nick = p.first; break; }

            // --- imprimir lo recibido (sin fileData) ---
            string recibido = "f"
                + ( (dest.size()<10) ? "0"+to_string(dest.size()) : to_string(dest.size()) )
                + dest
                + ( (fileName.size()<100) ? string(3 - to_string(fileName.size()).size(), '0') + to_string(fileName.size()) : to_string(fileName.size()) )
                + fileName
                + string(10 - to_string(fileSize).size(), '0') + to_string(fileSize);
            cout << recibido << endl;

            // --- armar mensaje para enviar al destinatario ---
            string enviar = "F"
                + ( (nick.size()<10) ? "0"+to_string(nick.size()) : to_string(nick.size()) )
                + nick
                + string(3 - to_string(fileName.size()).size(), '0') + to_string(fileName.size())
                + fileName
                + string(10 - to_string(fileSize).size(), '0') + to_string(fileSize)
                + fileData;

            if (clientes_conectados.count(dest))
                send(clientes_conectados[dest], enviar.c_str(), enviar.size(), 0);

            // --- imprimir lo enviado (sin fileData) ---
            cout << "F"
                << ( (nick.size()<10) ? "0"+to_string(nick.size()) : to_string(nick.size()) )
                << nick
                << string(3 - to_string(fileName.size()).size(), '0') + to_string(fileName.size())
                << fileName
                << string(10 - to_string(fileSize).size(), '0') + to_string(fileSize)
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
    int servidor = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in stSockAddr{};
    stSockAddr.sin_family = AF_INET;
    stSockAddr.sin_port = htons(puerto);
    stSockAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(servidor, (sockaddr*)&stSockAddr, sizeof(stSockAddr)) == -1) {
        perror("Error en bind");
        close(servidor);
        exit(1);
    }

    listen(servidor, 10);
    cout << "servidor activado en puerto: " << puerto << endl;

    while (true) {
        int clienteSock = accept(servidor, nullptr, nullptr);
        cout << "Nuevo cliente conectado" << endl;
        thread t(manejarCliente, clienteSock);
        t.detach();
    }

    close(servidor);
    return 0;
}