#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <functional> // Para std::ref
#include <cmath>      // Para std::sqrt
#include <Eigen/Dense>

using namespace Eigen;

/**
 * -----------------------------------------------------------------
 * TRABAJADOR 1: Multiplicación de Matriz (para Pasos 1 y 4)
 * -----------------------------------------------------------------
 * Calcula un bloque de filas de C = A * B
 */
void worker_matmul(const MatrixXd& A, 
                   const MatrixXd& B, 
                   MatrixXd& C, 
                   int fila_inicio, 
                   int fila_fin) 
{
    // C[filas...] = A[filas...] * B
    C.block(fila_inicio, 0, fila_fin - fila_inicio, B.cols()) = 
        A.block(fila_inicio, 0, fila_fin - fila_inicio, A.cols()) * B;
}

/**
 * -----------------------------------------------------------------
 * TRABAJADOR 2: Ordenar Resultados y Sigma (para Paso 3)
 * -----------------------------------------------------------------
 * Calcula un bloque de columnas de S y V
 */
void worker_reorder_and_sigma(const VectorXd& eigenvalues_in,
                            const MatrixXd& eigenvectors_in,
                            VectorXd& S_out,
                            MatrixXd& V_out,
                            int col_inicio,
                            int col_fin)
{
    int N = eigenvalues_in.size();
    for (int i = col_inicio; i < col_fin; ++i) {
        // Invierte el orden (de mayor a menor)
        // S[i] = sqrt(lambda[N-1-i])
        // V.col(i) = Eig.col(N-1-i)
        S_out(i) = std::sqrt(eigenvalues_in(N - 1 - i));
        V_out.col(i) = eigenvectors_in.col(N - 1 - i);
    }
}

/**
 * -----------------------------------------------------------------
 * TRABAJADOR 3: Escalar U (para Paso 5)
 * -----------------------------------------------------------------
 * Calcula un bloque de columnas de U = U_temp * S^-1
 */
void worker_scale_u(const MatrixXd& U_temp_in,
                    const VectorXd& S_in,
                    MatrixXd& U_out,
                    int col_inicio,
                    int col_fin)
{
    double tolerance = 1e-10;
    for (int i = col_inicio; i < col_fin; ++i) {
        if (S_in(i) > tolerance) {
            U_out.col(i) = U_temp_in.col(i) / S_in(i);
        } else {
            U_out.col(i) = VectorXd::Zero(U_out.rows());
        }
    }
}


/**
 * -----------------------------------------------------------------
 * FUNCIÓN "MAESTRO" para MatMul (usada en Pasos 1 y 4)
 * -----------------------------------------------------------------
 */
void parallel_matmul(const MatrixXd& A, const MatrixXd& B, MatrixXd& C) {
    unsigned int num_hilos = std::thread::hardware_concurrency();
    std::vector<std::thread> hilos;
    int filas_por_hilo = A.rows() / num_hilos;

    std::cout << "  [Maestro-MatMul] Paralelizando (" << A.rows() << "x" << A.cols() 
              << ") * (" << B.rows() << "x" << B.cols() << ") en " 
              << num_hilos << " hilos..." << std::endl;

    for (int i = 0; i < num_hilos; ++i) {
        int fila_inicio = i * filas_por_hilo;
        int fila_fin = (i == num_hilos - 1) ? A.rows() : (i + 1) * filas_por_hilo;
        
        hilos.emplace_back(
            worker_matmul, std::ref(A), std::ref(B), std::ref(C), fila_inicio, fila_fin
        );
    }
    for (std::thread& h : hilos) {
        h.join();
    }
}


/**
 * -----------------------------------------------------------------
 * FUNCIÓN PRINCIPAL (El flujo de trabajo completo de SVD)
 * -----------------------------------------------------------------
 */
int main() {
    // 0. Configuración
    int M = 1000;
    int N = 500;
    unsigned int num_hilos = std::thread::hardware_concurrency();
    std::cout << "Calculando SVD para " << M << "x" << N << " (método A^T*A) con " 
              << num_hilos << " hilos." << std::endl;

    MatrixXd A = MatrixXd::Random(M, N);
    
    // Matrices resultado
    MatrixXd U(M, N);
    VectorXd S(N);
    MatrixXd V(N, N);

    auto start_total = std::chrono::high_resolution_clock::now();

    // ---------------------------------------------------------------
    // PASO 1 (Paralelo): Calcular B = A^T * A
    // ---------------------------------------------------------------
    std::cout << "\n--- PASO 1: B = A^T * A (Paralelo) ---" << std::endl;
    auto start_paso1 = std::chrono::high_resolution_clock::now();
    MatrixXd At = A.transpose();
    MatrixXd B(N, N);
    parallel_matmul(At, A, B); // ✅ Paralelizado
    auto end_paso1 = std::chrono::high_resolution_clock::now();
    std::cout << "  Paso 1 completado en " << std::chrono::duration<double>(end_paso1 - start_paso1).count() << "s" << std::endl;

    // ---------------------------------------------------------------
    // PASO 2 (Secuencial): Calcular Eigen-descomposición de B
    // ---------------------------------------------------------------
    std::cout << "\n--- PASO 2: Eigen-descomposición (Secuencial) ---" << std::endl;
    // 🚫 NO PARALELIZABLE (con std::thread). Es una "caja negra" secuencial.
    SelfAdjointEigenSolver<MatrixXd> eigensolver(B);
    if (eigensolver.info() != Success) abort();
    std::cout << "  Paso 2 completado." << std::endl;


    // ---------------------------------------------------------------
    // PASO 3 (Paralelo): Ordenar y calcular Sigma
    // ---------------------------------------------------------------
    std::cout << "\n--- PASO 3: Ordenar y Sigma (Paralelo) ---" << std::endl;
    auto start_paso3 = std::chrono::high_resolution_clock::now();
    {
        std::vector<std::thread> hilos_paso3;
        int cols_por_hilo = N / num_hilos;
        for (int i = 0; i < num_hilos; ++i) {
            int col_inicio = i * cols_por_hilo;
            int col_fin = (i == num_hilos - 1) ? N : (i + 1) * cols_por_hilo;
            
            // ✅ Paralelizado
            hilos_paso3.emplace_back(
                worker_reorder_and_sigma,
                std::cref(eigensolver.eigenvalues()), // cref = const ref
                std::cref(eigensolver.eigenvectors()),
                std::ref(S),
                std::ref(V),
                col_inicio,
                col_fin
            );
        }
        for (std::thread& h : hilos_paso3) {
            h.join();
        }
    }
    auto end_paso3 = std::chrono::high_resolution_clock::now();
    std::cout << "  Paso 3 completado en " << std::chrono::duration<double>(end_paso3 - start_paso3).count() << "s" << std::endl;


    // ---------------------------------------------------------------
    // PASO 4 (Paralelo): Calcular U_temp = A * V
    // ---------------------------------------------------------------
    std::cout << "\n--- PASO 4: U_temp = A * V (Paralelo) ---" << std::endl;
    auto start_paso4 = std::chrono::high_resolution_clock::now();
    MatrixXd U_temp(M, N);
    parallel_matmul(A, V, U_temp); // ✅ Paralelizado
    auto end_paso4 = std::chrono::high_resolution_clock::now();
    std::cout << "  Paso 4 completado en " << std::chrono::duration<double>(end_paso4 - start_paso4).count() << "s" << std::endl;

    // ---------------------------------------------------------------
    // PASO 5 (Paralelo): Escalar U = U_temp * Sigma_inversa
    // ---------------------------------------------------------------
    std::cout << "\n--- PASO 5: U = U_temp * S^-1 (Paralelo) ---" << std::endl;
    auto start_paso5 = std::chrono::high_resolution_clock::now();
    {
        std::vector<std::thread> hilos_paso5;
        int cols_por_hilo = N / num_hilos;
        for (int i = 0; i < num_hilos; ++i) {
            int col_inicio = i * cols_por_hilo;
            int col_fin = (i == num_hilos - 1) ? N : (i + 1) * cols_por_hilo;
            
            // ✅ Paralelizado
            hilos_paso5.emplace_back(
                worker_scale_u,
                std::cref(U_temp),
                std::cref(S),
                std::ref(U),
                col_inicio,
                col_fin
            );
        }
        for (std::thread& h : hilos_paso5) {
            h.join();
        }
    }
    auto end_paso5 = std::chrono::high_resolution_clock::now();
    std::cout << "  Paso 5 completado en " << std::chrono::duration<double>(end_paso5 - start_paso5).count() << "s" << std::endl;


    // --- ¡TERMINADO! ---
    auto end_total = std::chrono::high_resolution_clock::now();
    std::cout << "\n================================================" << std::endl;
    std::cout << "SVD COMPLETO en " << std::chrono::duration<double>(end_total - start_total).count() << " segundos." << std::endl;
    std::cout << "================================================" << std::endl;
    
    // Opcional: Verificar el resultado
    // MatrixXd A_reconstruida = U * S.asDiagonal() * V.transpose();
    // std::cout << "Error (norma de A - USV^T): " << (A - A_reconstruida).norm() << std::endl;

    return 0;
}