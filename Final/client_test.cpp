#include <iostream>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <Eigen/Dense>
#include <sstream>
#include <iomanip>

using namespace std;
using namespace Eigen;

// Función para "empacar" la matriz en un string
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


    // 2. Crear y mostrar la matriz original
    cout << "uwu" << endl;
    MatrixXd A = MatrixXd::Random(10000, 10000); 
    cout << sizeof(int) << endl;
    cout << "Cliente: Matriz original a enviar:\n" << "\n\n";

    // 3. Empacar la matriz
    string packed_data = pack_matrix(A);

    cout << "A: " << packed_data.size() << endl;
    return 0;
}