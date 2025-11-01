#include <iostream>
#include <string>
#include <thread>
#include <cstring>
#include <vector>
#include <mutex>
#include <algorithm>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <Eigen/Dense>

using namespace std;
using namespace Eigen;

vector<int> slaves;
std::mutex slaves_mutex;

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
Eigen::MatrixXd unpack_matrix(const string& buffer) {
    size_t offset = 0;
    long rows;
    memcpy(&rows, buffer.c_str() + offset, sizeof(rows));
    offset += sizeof(rows);
    long cols;
    memcpy(&cols, buffer.c_str() + offset, sizeof(cols));
    offset += sizeof(cols);
    Eigen::MatrixXd mat(rows, cols);
    long data_size = rows * cols * sizeof(double);
    memcpy(mat.data(), buffer.c_str() + offset, data_size);
    return mat;
}
void parallel_matmul(const MatrixXd& A, const MatrixXd& B, MatrixXd& C) {
    std::lock_guard<std::mutex> lock(slaves_mutex);
    unsigned int num_slaves = slaves.size();
    int filas_por_slave = A.rows() / num_slaves;

        // 1. REPARTIR EL TRABAJO
    for (int i = 0; i < num_slaves; ++i) {
        int fila_inicio = i * filas_por_slave;
        int fila_fin = (i == num_slaves - 1) ? A.rows() : (i + 1) * filas_por_slave;
        
        MatrixXd A_chunk = A.block(fila_inicio, 0, fila_fin - fila_inicio, A.cols());

        
        string packed_A_chunk = pack_matrix(A_chunk);
        string packed_B_completa = pack_matrix(B); 
        

        int data_size_1 = packed_A_chunk.size();
        int data_size_2 = packed_B_completa.size(); 
        string size_header_1 = to_string(data_size_1); 
        string size_header_2 = to_string(data_size_2); 
        if (size_header_1.length() > 5) {
            cerr << "Error: Datos demasiado grandes ( " << size_header_1.length() 
                << " dígitos) para cabecera de 5 dígitos (max 99999)." << endl;
            close(slaves[i]);
        }
        if (size_header_2.length() > 5) {
            cerr << "Error: Datos demasiado grandes ( " << size_header_2.length() 
                << " dígitos) para cabecera de 5 dígitos (max 99999)." << endl;
            close(slaves[i]);
        }
        while (size_header_1.length() < 5) {
            size_header_1 = "0" + size_header_1;
        }
        while (size_header_2.length() < 5) {
            size_header_2 = "0" + size_header_2;
        }
        string enviar = "M" + size_header_1 + packed_A_chunk + size_header_1 + packed_B_completa;
        send(slaves[i], enviar.c_str(), enviar.size(), 0); 
    }

    // 2. RECOGER LOS RESULTADOS
    // for (int i = 0; i < num_slaves; ++i) {
    //     // Recibir el 'C_chunk' (que es un 'B_chunk')
    //     std::string chunk_recibido = recibir_con_cabecera(fds_esclavos[i]);
    //     MatrixXd B_chunk = unpack_matrix(chunk_recibido);

    //     // 2.1 Re-ensamblar la matriz final
    //     int fila_inicio = i * filas_por_slave;
    //     B_resultado.block(fila_inicio, 0, B_chunk.rows(), B_chunk.cols()) = B_chunk;
    // }
}
void manejarCliente(int clienteSock) {
    char buffer[2048];
    while(1){
        int n = recv(clienteSock, buffer, 1, 0);
        if (n <= 0) {
            { 
                std::lock_guard<std::mutex> lock(slaves_mutex); 
                auto iterador = std::find(slaves.begin(), slaves.end(), clienteSock);
                if (iterador != slaves.end()) {
                    slaves.erase(iterador);
                    std::cout << "Socket " << clienteSock << " eliminado del vector." << std::endl;
                }
            } 
            close(clienteSock);
            return;
        }
        char tipo = buffer[0];
        if(tipo == 's'){
            std::lock_guard<std::mutex> lock(slaves_mutex);
            slaves.push_back(clienteSock);
            cout << "ingreso de un nuevo slave" << endl;
        }
        else if(tipo == 'c'){
            int n = recv(clienteSock, buffer, 5, 0);
            int incoming_size = stoi(string(buffer, 5));

            cout << "Master: Cabecera recibida: \"" << buffer 
                << "\". Esperando " << incoming_size << " bytes de datos." << endl;

            n = recv(clienteSock, buffer, incoming_size, 0);
            string received_data(buffer, incoming_size);

            cout << "Master: Datos de la matriz recibidos." << endl;

            // Deserializar la matriz
            MatrixXd A = unpack_matrix(received_data);
            cout << "Master: ¡Matriz desempacada exitosamente!" << endl;
            cout << "Master: Dimensiones: " << A.rows() << "x" << A.cols() << endl;
            int M = A.rows();
            int N = A.cols();
            // Matrices resultado
            MatrixXd U(M, N);
            VectorXd S(N);
            MatrixXd V(N, N);
            cout << "\n--- PASO 1: B = A^T * A (Paralelo) ---" <<endl;
            MatrixXd At = A.transpose();
            MatrixXd B(N, N);
            parallel_matmul(At, A, B);


            const char* confirm_msg = "1";
            send(clienteSock, confirm_msg, strlen(confirm_msg), 0);
            cout << "Master: Conexión con cliente cerrada." << endl;
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
