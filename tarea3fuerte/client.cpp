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
    cout<<"n. Change your nickname"<<endl;
    cout<<"t. Send message to someone"<<endl;
    cout<<"m. Send broadcast"<<endl;
    cout<<"l. Show list"<<endl;
    cout<<"x. Exit"<<endl;
}

void recibirMensajes(int socketCliente) {
    char buffer[1024];

    while (true) {
        int bytesLeidos = recv(socketCliente, buffer, 1, 0); 
        if (bytesLeidos <= 0) {
            cout << "Desconectado del servidor" << endl;
            close(socketCliente);
            exit(0);
        }
        char tipoMensaje = buffer[0];

        if (tipoMensaje == 'M') { 
            recv(socketCliente, buffer, 2, 0);
            int tamNick = stoi(string(buffer, 2));

            recv(socketCliente, buffer, tamNick, 0);
            string nickEmisor(buffer, tamNick);

            recv(socketCliente, buffer, 3, 0);
            int tamMensaje = stoi(string(buffer, 3));

            recv(socketCliente, buffer, tamMensaje, 0);
            string mensaje(buffer, tamMensaje);

            cout << "\nnick: " << nickEmisor << " msg: " << mensaje << endl;
        } 
        else if (tipoMensaje == 'T') {
            recv(socketCliente, buffer, 2, 0);
            int tamNick = stoi(string(buffer, 2));

            recv(socketCliente, buffer, tamNick, 0);
            string nickEmisor(buffer, tamNick);

            recv(socketCliente, buffer, 3, 0);
            int tamMensaje = stoi(string(buffer, 3));

            recv(socketCliente, buffer, tamMensaje, 0);
            string mensaje(buffer, tamMensaje);

            cout << "\nmensaje privado de " << nickEmisor << " msg: " << mensaje << endl;
        } 
        else if (tipoMensaje == 'L') { 
            recv(socketCliente, buffer, 2, 0);
            int cantidadClientes = stoi(string(buffer, 2));
            cout << "\nnum de conectados: " << cantidadClientes << endl;

            for (int i = 0; i < cantidadClientes; ++i) {
                recv(socketCliente, buffer, 2, 0);
                int tamNick = stoi(string(buffer, 2));

                recv(socketCliente, buffer, tamNick, 0);
                string nick(buffer, tamNick);

                cout << nick << endl;
            }
        }
        else if (tipoMensaje == 'N') { 
            recv(socketCliente, buffer, 2, 0);
            int tamNick = stoi(string(buffer, 2));

            recv(socketCliente, buffer, tamNick, 0);
            string nickNuevo(buffer, tamNick);

            cout << "\n" << nickNuevo << " cambio su nick" << endl;
        }
        else if (tipoMensaje == 'X') { 
            recv(socketCliente, buffer, 2, 0);
            int tamNick = stoi(string(buffer, 2));

            recv(socketCliente, buffer, tamNick, 0);
            string nickDesconectado(buffer, tamNick);

            cout << "\n" << nickDesconectado << " se desconecto" << endl;
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        cerr << "Uso: " << argv[0] << " <puerto>\n";
        return 1;
    }

    int puerto = atoi(argv[1]);

    int socketCliente = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in direccionServidor{};
    direccionServidor.sin_family = AF_INET;
    direccionServidor.sin_port = htons(puerto);
    direccionServidor.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(socketCliente, (sockaddr*)&direccionServidor, sizeof(direccionServidor)) == -1) {
        cerr << "Error conectando al servidor\n";
        return 1;
    }

    cout << "Conectado al servidor!\n";

    thread hiloRecepcion(recibirMensajes, socketCliente);
    hiloRecepcion.detach();

    char opcionMenu;
    while (true) {
        showMenu();
        cin >> opcionMenu;

        string contenido, mensajeAEnviar, destinatario;
        string tamNickStr, tamMsgStr, tamDestStr;
        char tipoMensaje;

        if(opcionMenu == 'n'){ 
            tipoMensaje = 'n';
            cout<<"Enter your nickname: ";
            cin >> contenido;
            int tamNick = contenido.size();
            tamNickStr = (tamNick < 10) ? "0" + to_string(tamNick) : to_string(tamNick);
            mensajeAEnviar = string(1, tipoMensaje) + tamNickStr + contenido;
        }
        else if(opcionMenu == 't'){ 
            tipoMensaje = 't';
            cout<<"Enter user: ";
            cin >> destinatario;
            cout<<"Enter your msg: ";
            cin.ignore();
            getline(cin, contenido);

            int tamDest = destinatario.size();
            int tamMsg = contenido.size();

            tamDestStr = (tamDest < 10) ? "0" + to_string(tamDest) : to_string(tamDest);
            if (tamMsg < 10) tamMsgStr = "00" + to_string(tamMsg);
            else if (tamMsg < 100) tamMsgStr = "0" + to_string(tamMsg);
            else tamMsgStr = to_string(tamMsg);

            mensajeAEnviar = string(1, tipoMensaje) + tamDestStr + destinatario + tamMsgStr + contenido;
        }
        else if(opcionMenu == 'm'){ 
            tipoMensaje = 'm';
            cout<<"Enter broadcast: ";
            cin.ignore();
            getline(cin, contenido);
            int tamMsg = contenido.size();

            if (tamMsg < 10) tamMsgStr = "00" + to_string(tamMsg);
            else if (tamMsg < 100) tamMsgStr = "0" + to_string(tamMsg);
            else tamMsgStr = to_string(tamMsg);

            mensajeAEnviar = string(1, tipoMensaje) + tamMsgStr + contenido;
        }
        else if(opcionMenu == 'l'){ 
            tipoMensaje = 'l';
            mensajeAEnviar = string(1, tipoMensaje);
        }
        else if(opcionMenu == 'x'){
            tipoMensaje = 'x';
            mensajeAEnviar = string(1, tipoMensaje);
            send(socketCliente, mensajeAEnviar.c_str(), mensajeAEnviar.size(), 0);
            break;
        }

        send(socketCliente, mensajeAEnviar.c_str(), mensajeAEnviar.size(), 0);
    }

    close(socketCliente);
    return 0;
}
