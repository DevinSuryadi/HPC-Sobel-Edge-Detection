/*
 * Tahap 2: uji komunikasi antar-node.
 * Setiap rank melaporkan host-nya, lalu sebuah token dikirim berkeliling
 * (rank 0 -> 1 -> ... -> N-1 -> 0) dengan MPI_Send/MPI_Recv.
 * Jika token kembali ke rank 0 dengan nilai N, komunikasi antar-node berfungsi.
 */
#include <mpi.h>
#include <stdio.h>

int main(int argc, char **argv)
{
    int rank, size, len, token;
    char host[MPI_MAX_PROCESSOR_NAME];

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Get_processor_name(host, &len);

    printf("rank %d dari %d berjalan di %s\n", rank, size, host);
    fflush(stdout);
    MPI_Barrier(MPI_COMM_WORLD);

    if (size > 1) {
        int next = (rank + 1) % size;
        int prev = (rank - 1 + size) % size;
        if (rank == 0) {
            token = 1;
            MPI_Send(&token, 1, MPI_INT, next, 0, MPI_COMM_WORLD);
            MPI_Recv(&token, 1, MPI_INT, prev, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            printf("token kembali ke rank 0 dengan nilai %d (harus %d): %s\n",
                   token, size, token == size ? "OK" : "GAGAL");
        } else {
            MPI_Recv(&token, 1, MPI_INT, prev, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            token++;
            MPI_Send(&token, 1, MPI_INT, next, 0, MPI_COMM_WORLD);
        }
    }

    MPI_Finalize();
    return 0;
}
