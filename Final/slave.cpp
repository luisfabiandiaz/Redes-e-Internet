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
    string buffer_total = "s";
    send(sock, buffer_total.c_str(), buffer_total.size(), 0);
    while(1){
        
    }
    close(sock);
    return 0;
}