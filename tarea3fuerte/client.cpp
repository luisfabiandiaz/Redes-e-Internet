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

void showMenu(){
    cout<<"1. Change your nickname"<<endl;
    cout<<"2. Send message to someone"<<endl;
    cout<<"3. Send broadcast"<<endl;
    cout<<"4. Show list"<<endl;
    cout<<"5. Exit"<<endl;
}

void recibirMensajes(int socketCliente) {
    char buffer[1024];

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

            cout << "M"
                 << ((nick.size()<10) ? "0"+to_string(nick.size()):to_string(nick.size()))
                 << nick
                 << ((msg.size()<10) ? "00"+to_string(msg.size()):(msg.size()<100)?"0"+to_string(msg.size()):to_string(msg.size()))
                 << msg << endl;

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

            cout << "T"
                 << ((nick.size()<10) ? "0"+to_string(nick.size()):to_string(nick.size()))
                 << nick
                 << ((msg.size()<10) ? "00"+to_string(msg.size()):(msg.size()<100)?"0"+to_string(msg.size()):to_string(msg.size()))
                 << msg << endl;

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

            cout << "N"
                 << ((nick.size()<10) ? "0"+to_string(nick.size()):to_string(nick.size()))
                 << nick << endl;

            cout << "\n" << nick << " cambio su nick" << endl;
        }
        else if (tipo == 'X') {
            recv(socketCliente, buffer, 2, 0);
            int tamNick = stoi(string(buffer, 2));
            recv(socketCliente, buffer, tamNick, 0);
            string nick(buffer, tamNick);

            cout << "X"
                 << ((nick.size()<10) ? "0"+to_string(nick.size()):to_string(nick.size()))
                 << nick << endl;

            cout << "\n" << nick << " se desconecto" << endl;
        }
        else if (tipo == 'E') {
            recv(socketCliente, buffer, 3, 0);
            int tamMsg = stoi(string(buffer, 3));
            recv(socketCliente, buffer, tamMsg, 0);
            string msg(buffer, tamMsg);

            cout << "E"
                 << ((msg.size()<10) ? "00"+to_string(msg.size()):(msg.size()<100)?"0"+to_string(msg.size()):to_string(msg.size()))
                 << msg << endl;

            cout << "\nERROR: " << msg << endl;
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
        else if(entrada == 5){
            tipo='x';
            enviar = string(1, tipo);
            send(sock, enviar.c_str(), enviar.size(), 0);
            cout << enviar << endl;
            break;
        }
        cout << enviar << endl;
        send(sock, enviar.c_str(), enviar.size(), 0);
    }

    close(sock);
    return 0;
}