#include <iostream>
#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <Eigen/Dense>

using namespace std;
using namespace Eigen;
char buffer[2048];
// (pack_matrix sigue igual)
std::string pack_matrix(const Eigen::MatrixXd& mat) {
    long rows = mat.rows();
    long cols = mat.cols();
    long data_size = rows * cols * sizeof(double);
    std::string buffer;
    buffer.reserve(sizeof(rows) + sizeof(cols) + data_size);
    buffer.append((char*)&rows, sizeof(rows));
    buffer.append((char*)&cols, sizeof(cols));
    buffer.append((char*)mat.data(), data_size);
    return buffer;
}


int main(int argc, char* argv[]) {
    if (argc < 2) {
        cerr << "Uso: " << argv[0] << " <puerto>\n";
        return 1;
    }

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
    cout << "Conectado al servidor!\n";
    string client = "c";

    MatrixXd A = MatrixXd::Random(10, 5); 
    cout << "Cliente: Creando matriz de " << A.rows() << "x" << A.cols() << endl;

    string packed_data = pack_matrix(A);
    int data_size = packed_data.size(); 
    string size_header = to_string(data_size); 

    // Chequeo de seguridad
    if (size_header.length() > 5) {
        cerr << "Error: Datos demasiado grandes ( " << size_header.length() 
             << " dígitos) para cabecera de 5 dígitos (max 99999)." << endl;
        close(sock);
        return 1;
    }
    
    while (size_header.length() < 5) {
        size_header = "0" + size_header;
    }

    cout << "Cliente: Tamaño de datos: " << data_size << ". Cabecera: \"" << size_header << "\"" << endl;

    // 7. Crear el buffer TOTAL (Cabecera + Datos)
    string buffer_total = client + size_header + packed_data;

    // 8. Enviar TODO de una vez
    cout << "Cliente: Enviando buffer total de " << buffer_total.size() << " bytes..." << endl;
    send(sock, buffer_total.c_str(), buffer_total.size(), 0);

    int n = recv(sock, buffer, 1, 0);
    if (n > 0) {
        cout << "Cliente: Respuesta del Servidor: \"" << buffer[0] << "\"" << endl;
    } else {
        cout << "Cliente: El servidor cerró la conexión." << endl;
    }

    close(sock);
    return 0;
}