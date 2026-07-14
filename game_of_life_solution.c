#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ========================================================================= */
/* 0. Path Utilities                                                         */
/* ========================================================================= */

/*
 * Builds a full file path from an optional directory prefix and a filename.
 */
static char *build_path(const char *dir, const char *filename) {
    if (dir == NULL || dir[0] == '\0') {
        char *path = (char *)malloc(strlen(filename) + 1);
        strcpy(path, filename);
        return path;
    }
    size_t len = strlen(dir) + 1 + strlen(filename) + 1;
    char *path = (char *)malloc(len);
    if (dir[strlen(dir) - 1] == '/')
        snprintf(path, len, "%s%s", dir, filename);
    else
        snprintf(path, len, "%s/%s", dir, filename);
    return path;
}

/* ========================================================================= */
/* 1. Game of Life Logic                                                     */
/* ========================================================================= */
/*
 *  my_stated - 0 / 1 state of current cell
 *  sum_neighbors - sum of neighbors current state value
 *  function return new cell state
 * Computes the next state of a single cell:
 * - Live cell with 0 live neighbors -> dies (loneliness).
 * - Live cell with 1 or 2 live neighbors -> stays alive.
 * - Live cell with > 2 live neighbors -> dies (overcrowding).
 * - Dead cell with exactly 3 live neighbors -> becomes alive (reproduction).
 */
int compute_next_state(int my_state, int sum_neighbors) {
    // TODO: Implement the 4-neighbor Game of Life rules here:

    if (my_state == 1) {
        if (sum_neighbors == 0) return 0;         /* Loneliness */
        if (sum_neighbors <= 2) return 1;         /* Survival */
        return 0;                                 /* Overcrowding (3 or 4) */
    } else {
        if (sum_neighbors == 3) return 1;         /* Reproduction */
    }
    return 0;
}

/* ========================================================================= */
/* 2. Communication: Halo Exchange                                           */
/* ========================================================================= */

void perform_halo_exchange(MPI_Comm cart_comm, int **local_grid, int B) {
    /* TODO: Implement MPI_Cart_shift to find neighbors and MPI_Sendrecv
     * to exchange the boundary data (rows and columns) between processes.
     */

     int up, down, left, right;
    
    /* Find neighbors in 2D Cartesian grid */
    MPI_Cart_shift(cart_comm, 0, 1, &up, &down);   /* Shift for rows */
    MPI_Cart_shift(cart_comm, 1, 1, &left, &right); /* Shift for cols */

    /* Exchange Rows: Send top inner row to UP, receive bottom halo from DOWN */
    MPI_Sendrecv(&local_grid[1][1], B, MPI_INT, up, 0,
                 &local_grid[B + 1][1], B, MPI_INT, down, 0,
                 cart_comm, MPI_STATUS_IGNORE);

    /* Exchange Rows: Send bottom inner row to DOWN, receive top halo from UP */
    MPI_Sendrecv(&local_grid[B][1], B, MPI_INT, down, 1,
                 &local_grid[0][1], B, MPI_INT, up, 1,
                 cart_comm, MPI_STATUS_IGNORE);

    /* Prepare buffers for Column Exchange */
    int *send_left_col = (int *)malloc(B * sizeof(int));
    int *send_right_col = (int *)malloc(B * sizeof(int));
    int *recv_left_col = (int *)malloc(B * sizeof(int));
    int *recv_right_col = (int *)malloc(B * sizeof(int));

    /* Pack left and right boundaries into contiguous buffers */
    for (int i = 1; i <= B; i++) {
        send_left_col[i - 1] = local_grid[i][1];
        send_right_col[i - 1] = local_grid[i][B];
    }

    /* Send left col to LEFT, receive right halo from RIGHT */
    MPI_Sendrecv(send_left_col, B, MPI_INT, left, 2,
                 recv_right_col, B, MPI_INT, right, 2,
                 cart_comm, MPI_STATUS_IGNORE);

    /* Send right col to RIGHT, receive left halo from LEFT */
    MPI_Sendrecv(send_right_col, B, MPI_INT, right, 3,
                 recv_left_col, B, MPI_INT, left, 3,
                 cart_comm, MPI_STATUS_IGNORE);

    /* Unpack received columns into halo zones */
    for (int i = 1; i <= B; i++) {
        local_grid[i][B + 1] = recv_right_col[i - 1];
        local_grid[i][0] = recv_left_col[i - 1];
    }

    /* Free buffers */
    free(send_left_col);
    free(send_right_col);
    free(recv_left_col);
    free(recv_right_col);
}

/* ========================================================================= */
/* 3. Setup and Data Distribution                                            */
/* ========================================================================= */

int read_and_distribute(MPI_Comm cart_comm, int rank, int size,
                        int *N, int *K, int *Iters, int ***local_grid,
                        const char *dir) {
    /* TODO:
     * 1. Process 0 reads the file matrix.txt, which contains K, Iters, and the NxN matrix.
     * 2. Calculate B = N / K.
     * 3. Process 0 distributes the BxB blocks to all processes using MPI_Send/MPI_Recv or MPI_Scatter.
     * 4. Each process allocates its local BxB grid (including ghost cells).
     */

    int metadata[3] = {0};
    int *global_matrix = NULL;
    /* Process 0 reads file and parses initial parameters */
    if (rank == 0) {
        char *path = build_path(dir, "matrix.txt");
        FILE *fp = fopen(path, "r");
        free(path);
        if (!fp) {
            printf("Error: matrix.txt not found.\n");
        } else {
            fscanf(fp, "%d %d %d", &metadata[0], &metadata[1], &metadata[2]);
            global_matrix = (int *)malloc(metadata[0] * metadata[0] * sizeof(int));
            for (int i = 0; i < metadata[0] * metadata[0]; i++)
                fscanf(fp, "%d", &global_matrix[i]);
            fclose(fp);
        }
    }

    /* TODO: Implement distribution of data according to problem */
    /* Broadcast metadata to all processes */
    MPI_Bcast(metadata, 3, MPI_INT, 0, cart_comm);
    *N = metadata[0];
    *K = metadata[1];
    *Iters = metadata[2];

    int B = (*N) / (*K);

    /* Allocate local grid with +2 buffer for Ghost cells */
    *local_grid = (int **)malloc((B + 2) * sizeof(int *));
    for (int i = 0; i < B + 2; i++) {
        (*local_grid)[i] = (int *)calloc((B + 2), sizeof(int));
    }

    /* Distribute blocks */
    if (rank == 0) {
        for (int r = 0; r < size; r++) {
            int coords[2];
            MPI_Cart_coords(cart_comm, r, 2, coords);
            int start_row = coords[0] * B;
            int start_col = coords[1] * B;

            int *block = (int *)malloc(B * B * sizeof(int));
            for (int i = 0; i < B; i++) {
                for (int j = 0; j < B; j++) {
                    block[i * B + j] = global_matrix[(start_row + i) * (*N) + (start_col + j)];
                }
            }

            if (r == 0) {
                /* Process 0 copies its own block directly */
                for (int i = 0; i < B; i++) {
                    for (int j = 0; j < B; j++) {
                        (*local_grid)[i + 1][j + 1] = block[i * B + j];
                    }
                }
            } else {
                /* Send blocks to respective processes */
                MPI_Send(block, B * B, MPI_INT, r, 0, cart_comm);
            }
            free(block);
        }
        free(global_matrix);
    } else {
        /* Workers receive their block and insert it into local_grid */
        int *block = (int *)malloc(B * B * sizeof(int));
        MPI_Recv(block, B * B, MPI_INT, 0, 0, cart_comm, MPI_STATUS_IGNORE);
        for (int i = 0; i < B; i++) {
            for (int j = 0; j < B; j++) {
                (*local_grid)[i + 1][j + 1] = block[i * B + j];
            }
        }
        free(block);
    }
    return B;
}

/* ========================================================================= */
/* 4. Printing Results
 * Don't change this function
 * Needed for Automatic tests
 */
/* ========================================================================= */

void print_matrix(int N, int *global_matrix, const char *dir) {
    /* Printing logic provided for uniform output to match test cases. */
    char *exp_path = build_path(dir, "expected.txt");
    FILE *fp = fopen(exp_path, "r");
    free(exp_path);

    if (fp) {
        int passed = 1, expected_val;
        for (int i = 0; i < N * N; i++) {
            if (fscanf(fp, "%d", &expected_val) == 1) {
                if (global_matrix[i] != expected_val) passed = 0;
            }
        }
        fclose(fp);
        if (passed) printf("\n[+] TEST PASSED: Output matches expected.txt.\n\n");
        else printf("\n[-] TEST FAILED: Output differs from expected.txt.\n\n");
    }

    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++)
            printf("%d ", global_matrix[i * N + j]);
        printf("\n");
    }
}

/* ========================================================================= */
/* 5. Main Flow                                                              */
/* ========================================================================= */

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);

    /* TODO:
     * 1. Create Cartesian communicator.
     * 2. Call read_and_distribute.
     * 3. Loop Iters times: call halo exchange, compute new generation, update local grid.
     * 4. Call gather_data and print_matrix.
     */

     int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    /* 1. Create Cartesian communicator (Torus topology) */
    int dims[2] = {0, 0};
    MPI_Dims_create(size, 2, dims); /* Factors 'size' into a KxK grid */
    int periods[2] = {1, 1};        /* Torus wrapping */
    MPI_Comm cart_comm;
    MPI_Cart_create(MPI_COMM_WORLD, 2, dims, periods, 0, &cart_comm);

    const char *dir = (argc > 1) ? argv[1] : "";
    int N, K, Iters;
    int **local_grid = NULL;

    /* 2. Read file and distribute blocks to processes */
    int B = read_and_distribute(cart_comm, rank, size, &N, &K, &Iters, &local_grid, dir);

    /* Allocate a temporary grid to hold next generation updates */
    int **temp_grid = (int **)malloc((B + 2) * sizeof(int *));
    for (int i = 0; i < B + 2; i++) {
        temp_grid[i] = (int *)calloc((B + 2), sizeof(int));
    }

    /* 3. Main Simulation Loop */
    for (int iter = 0; iter < Iters; iter++) {
        /* Exchange borders with 4 neighbors */
        perform_halo_exchange(cart_comm, local_grid, B);

        /* Compute new state based on inner block (1 to B) */
        for (int i = 1; i <= B; i++) {
            for (int j = 1; j <= B; j++) {
                int sum = local_grid[i - 1][j] + local_grid[i + 1][j] + 
                          local_grid[i][j - 1] + local_grid[i][j + 1];
                temp_grid[i][j] = compute_next_state(local_grid[i][j], sum);
            }
        }

        /* Swap states to update current generation */
        for (int i = 1; i <= B; i++) {
            for (int j = 1; j <= B; j++) {
                local_grid[i][j] = temp_grid[i][j];
            }
        }
    }

    /* 4. Gather data from all workers back to Process 0 */
    int *global_matrix = NULL;
    if (rank == 0) {
        global_matrix = (int *)malloc(N * N * sizeof(int));
    }

    /* Pack active local grid (without halos) into a flat array */
    int *send_block = (int *)malloc(B * B * sizeof(int));
    for (int i = 0; i < B; i++) {
        for (int j = 0; j < B; j++) {
            send_block[i * B + j] = local_grid[i + 1][j + 1];
        }
    }

    if (rank == 0) {
        for (int r = 0; r < size; r++) {
            int coords[2];
            MPI_Cart_coords(cart_comm, r, 2, coords);
            int start_row = coords[0] * B;
            int start_col = coords[1] * B;

            int *recv_block;
            if (r == 0) {
                recv_block = send_block; /* Process 0 handles its own block */
            } else {
                recv_block = (int *)malloc(B * B * sizeof(int));
                MPI_Recv(recv_block, B * B, MPI_INT, r, 0, cart_comm, MPI_STATUS_IGNORE);
            }

            /* Map received block into the continuous global_matrix */
            for (int i = 0; i < B; i++) {
                for (int j = 0; j < B; j++) {
                    global_matrix[(start_row + i) * N + (start_col + j)] = recv_block[i * B + j];
                }
            }
            if (r != 0) free(recv_block);
        }
    } else {
        MPI_Send(send_block, B * B, MPI_INT, 0, 0, cart_comm);
    }
    free(send_block);

    /* Process 0 prints the final global matrix */
    if (rank == 0) {
        print_matrix(N, global_matrix, dir);
        free(global_matrix);
    }

    /* 5. Cleanup Resources */
    for (int i = 0; i < B + 2; i++) {
        free(local_grid[i]);
        free(temp_grid[i]);
    }
    free(local_grid);
    free(temp_grid);

    MPI_Comm_free(&cart_comm);
    MPI_Finalize();
    return 0;
}