#include <iostream>
#include <string>
#include <vector>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <Eigen/Dense>
#include <cstring> // para memcpy

using namespace std;
using namespace Eigen;

// Función para "desempacar" el string de vuelta a una matriz
Eigen::MatrixXd unpack_matrix(const string& buffer) {
    size_t offset = 0;
    long rows, cols;
    memcpy(&rows, buffer.data() + offset, sizeof(rows));
    offset += sizeof(rows);
    memcpy(&cols, buffer.data() + offset, sizeof(cols));
    offset += sizeof(cols);
    Eigen::MatrixXd mat(rows, cols);
    long data_size = rows * cols * sizeof(double);
    memcpy(mat.data(), buffer.data() + offset, data_size);
    return mat;
}

// Función de ayuda para recibir un número exacto de bytes
bool recv_all(int sock, char* buffer, int total_bytes) {
    int bytes_recibidos = 0;
    while (bytes_recibidos < total_bytes) {
        int n = recv(sock, buffer + bytes_recibidos, total_bytes - bytes_recibidos, 0);
        if (n <= 0) return false;
        bytes_recibidos += n;
    }
    return true;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        cerr << "Uso: " << argv[0] << " <puerto>\n";
        return 1;
    }

    int puerto = stoi(argv[1]);
    int servidor_sock = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in stSockAddr{};
    stSockAddr.sin_family = AF_INET;
    stSockAddr.sin_port = htons(puerto);
    stSockAddr.sin_addr.s_addr = INADDR_ANY;

    bind(servidor_sock, (sockaddr*)&stSockAddr, sizeof(stSockAddr));
    listen(servidor_sock, 1);
    cout << "Servidor: Esperando conexión en el puerto " << puerto << "...\n";

    int cliente_sock = accept(servidor_sock, nullptr, nullptr);
    if (cliente_sock == -1) {
        perror("Error en accept");
        return 1;
    }
    cout << "Servidor: Cliente conectado!\n";

    // 1. Leer el tipo de cliente
    char tipo_buffer[1];
    if (!recv_all(cliente_sock, tipo_buffer, 1) || tipo_buffer[0] != 'c') {
        cerr << "Servidor: No se pudo identificar al cliente o tipo incorrecto.\n";
        close(cliente_sock);
        close(servidor_sock);
        return 1;
    }

    // 2. Leer la cabecera de 5 bytes
    const int HEADER_SIZE = 5;
    char header_buffer[HEADER_SIZE];
    if (!recv_all(cliente_sock, header_buffer, HEADER_SIZE)) {
        cerr << "Servidor: El cliente se desconectó al leer la cabecera.\n";
        close(cliente_sock);
        return 1;
    }
    int incoming_size = stoi(string(header_buffer, HEADER_SIZE));
    cout << "Servidor: Cabecera recibida. Esperando " << incoming_size << " bytes de datos.\n";

    // 3. Recibir los datos de la matriz
    string received_data;
    received_data.resize(incoming_size);
    if (!recv_all(cliente_sock, (char*)received_data.data(), incoming_size)) {
        cerr << "Servidor: El cliente se desconectó al leer los datos.\n";
        close(cliente_sock);
        return 1;
    }

    // 4. Desempacar e imprimir la matriz
    MatrixXd A_recibida = unpack_matrix(received_data);
    cout << "Servidor: ¡Matriz recibida exitosamente!\n" << A_recibida << endl;

    cout << "Servidor: Trabajo terminado. Cerrando.\n";
    close(cliente_sock);
    close(servidor_sock);
    return 0;
}