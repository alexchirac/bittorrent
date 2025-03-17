#include <mpi.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>
#include <iostream>

#define TRACKER_RANK 0
#define MAX_FILES 10
#define MAX_FILENAME 15
#define HASH_SIZE 32
#define MAX_CHUNKS 100

#define TRACKER_TAG 0
#define INTER_PEER_TAG 1
#define CHUNK_TAG 2

using namespace std;

vector<string> requested_files;
unordered_map<string, vector<string>> files;
unordered_map<string, vector<int>> file_owners;
unordered_map<string, int> file_size;
int nr_owned_files;
int nr_requested_files;
pthread_mutex_t mutex;

void *download_thread_func(void *arg)
{
	int rank = *(int*) arg;
	vector<int> peers;
	MPI_Status status;
	int cnt = 0;
	vector<string> chunks;

	for (auto &file : requested_files) {
		char message = '1';
		MPI_Send(&message, 1, MPI_CHAR, TRACKER_RANK, TRACKER_TAG, MPI_COMM_WORLD);
		MPI_Send(file.c_str(), MAX_FILENAME, MPI_CHAR, TRACKER_RANK, TRACKER_TAG, MPI_COMM_WORLD);

		long unsigned int file_size;
		MPI_Recv(&file_size, 1, MPI_INT, TRACKER_RANK, TRACKER_TAG, MPI_COMM_WORLD, &status);
		chunks = vector<string>(file_size);

		for (unsigned int i = 0; i < file_size; i++) {
			char chunk[HASH_SIZE];
			MPI_Recv(chunk, HASH_SIZE, MPI_CHAR, TRACKER_RANK, TRACKER_TAG, MPI_COMM_WORLD, &status);
			chunks[i] = string(chunk);
		}

		while (files[file].size() < file_size)
		{
			if (files[file].size() % 10 == 0) {
				char message = '2';
				MPI_Send(&message, 1, MPI_CHAR, TRACKER_RANK, TRACKER_TAG, MPI_COMM_WORLD);
				MPI_Send(file.c_str(), MAX_FILENAME, MPI_CHAR, TRACKER_RANK, TRACKER_TAG, MPI_COMM_WORLD);
				int nr_peers;
				MPI_Recv(&nr_peers, 1, MPI_INT, TRACKER_RANK, TRACKER_TAG, MPI_COMM_WORLD, &status);

				peers.resize(nr_peers);
				MPI_Recv(&peers[0], nr_peers, MPI_INT, TRACKER_RANK, TRACKER_TAG, MPI_COMM_WORLD, &status);
			}

			while (true) {
				int chunk_index = files[file].size();

				cnt++;
				int peer_index = cnt % peers.size();
				int peer = peers[peer_index];
				if (peer == rank) {
					continue;
				}
				char message = 'R';
				MPI_Send(&message, 1, MPI_CHAR, peer, INTER_PEER_TAG, MPI_COMM_WORLD);

				MPI_Send(file.c_str(), MAX_FILENAME, MPI_CHAR, peer, INTER_PEER_TAG, MPI_COMM_WORLD);
				MPI_Send(&chunk_index, 1, MPI_INT, peer, INTER_PEER_TAG, MPI_COMM_WORLD);
				char chunk[HASH_SIZE];
				MPI_Recv(chunk, HASH_SIZE, MPI_CHAR, peer, CHUNK_TAG, MPI_COMM_WORLD, &status);
				string chunkStr(chunk);
				if (chunkStr != "DOESN'T EXIST" && chunkStr == chunks[chunk_index]) {
					pthread_mutex_lock(&mutex);
					files[file].push_back(chunkStr);
					pthread_mutex_unlock(&mutex);
					break;
				}
			}
		}

		message = '3';
		MPI_Send(&message, 1, MPI_CHAR, TRACKER_RANK, TRACKER_TAG, MPI_COMM_WORLD);
		MPI_Send(file.c_str(), MAX_FILENAME, MPI_CHAR, TRACKER_RANK, TRACKER_TAG, MPI_COMM_WORLD);

		ofstream out("client" + to_string(rank) + "_" + file);
		for (auto &hash : files[file]) {
			out << hash << '\n';
		}
	}

	char x = 'F';
	MPI_Send(&x, 1, MPI_CHAR, TRACKER_RANK, TRACKER_TAG, MPI_COMM_WORLD);

	return NULL;
}

void *upload_thread_func(void *arg)
{
	MPI_Status status;

	while (1) {
		char message;

		MPI_Recv(&message, 1, MPI_CHAR, MPI_ANY_SOURCE, INTER_PEER_TAG, MPI_COMM_WORLD, &status);
		if (message == 'Q') {
			break;
		}
		if (message == 'R') {
			char filename[MAX_FILENAME];
			unsigned int chunk_index;

			MPI_Recv(filename, MAX_FILENAME, MPI_CHAR, status.MPI_SOURCE, INTER_PEER_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
			MPI_Recv(&chunk_index, 1, MPI_INT, status.MPI_SOURCE, INTER_PEER_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

			string filenameStr(filename);

			pthread_mutex_lock(&mutex);
			if (chunk_index < files[filenameStr].size()) {
				string chunk = files[filenameStr][chunk_index];
				pthread_mutex_unlock(&mutex);

				MPI_Send(chunk.c_str(), HASH_SIZE, MPI_CHAR, status.MPI_SOURCE, CHUNK_TAG, MPI_COMM_WORLD);
			} else {
				string chunk = "DOESN'T EXIST";
				pthread_mutex_unlock(&mutex);
	
				MPI_Send(chunk.c_str(), HASH_SIZE, MPI_CHAR, status.MPI_SOURCE, CHUNK_TAG, MPI_COMM_WORLD);
			}
		}
	}

	return NULL;
}

void receive_files(int rank) {
	MPI_Recv(&nr_owned_files, 1, MPI_INT, rank, TRACKER_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	for (int i = 0; i < nr_owned_files; i++) {
		char filename[MAX_FILENAME];
		int chunks;

		MPI_Recv(filename, MAX_FILENAME, MPI_CHAR, rank, TRACKER_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
		MPI_Recv(&chunks, 1, MPI_INT, rank, TRACKER_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

		file_owners[filename].push_back(rank);
		file_size[filename] = chunks;
		files[filename] = vector<string>(chunks);
		for (int j = 0; j < chunks; j++) {
			char chunk[HASH_SIZE];
			MPI_Recv(chunk, HASH_SIZE, MPI_CHAR, rank, TRACKER_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
			files[filename][j] = chunk;
		}
	} 
}

void tracker(int numtasks, int rank) {
	MPI_Status status;

	for (int i = 1; i < numtasks; i++) {
		receive_files(i);
	}

	for (int i = 1; i < numtasks; i++) {
		MPI_Send(NULL, 0, MPI_CHAR, i, TRACKER_TAG, MPI_COMM_WORLD);
	}

	int nr_finished = 0;
	while (true) {
		char message;
		MPI_Recv(&message, 1, MPI_CHAR, MPI_ANY_SOURCE, TRACKER_TAG, MPI_COMM_WORLD, &status);
		if (message == '1') {
			char filename[MAX_FILENAME];
			MPI_Recv(&filename, MAX_FILENAME, MPI_CHAR, status.MPI_SOURCE, TRACKER_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
			MPI_Send(&file_size[filename], 1, MPI_INT, status.MPI_SOURCE, TRACKER_TAG, MPI_COMM_WORLD);

			for (int i = 0; i < file_size[filename]; i++) {
				MPI_Send(files[filename][i].c_str(), HASH_SIZE, MPI_CHAR, status.MPI_SOURCE, TRACKER_TAG, MPI_COMM_WORLD);
			}

			string filenameStr(filename);
			file_owners[filenameStr].push_back(status.MPI_SOURCE);
		}
		if (message == '2') {
			char filename[MAX_FILENAME];
			MPI_Recv(&filename, MAX_FILENAME, MPI_CHAR, status.MPI_SOURCE, TRACKER_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
			int size = file_owners[filename].size();
			MPI_Send(&size, 1, MPI_INT, status.MPI_SOURCE, TRACKER_TAG, MPI_COMM_WORLD);
			MPI_Send(&file_owners[filename][0], file_owners[filename].size(), MPI_INT, status.MPI_SOURCE, TRACKER_TAG, MPI_COMM_WORLD);
		}
		if (message == '3') {
			char filename[MAX_FILENAME];
			MPI_Recv(&filename, MAX_FILENAME, MPI_CHAR, status.MPI_SOURCE, TRACKER_TAG, MPI_COMM_WORLD, &status);
			cout << "Client " << status.MPI_SOURCE << " finished downloading file " << filename << endl;
		}
		if (message == 'F') {
			nr_finished++;
			if (nr_finished == numtasks - 1) {
				message = 'Q';
				for (int i = 1; i < numtasks; i++) {
					MPI_Send(&message, 1, MPI_CHAR, i, INTER_PEER_TAG, MPI_COMM_WORLD);
				}
				break;
			}
		}
	}
}

void send_owned_files(int rank) {
	MPI_Send(&nr_owned_files, 1, MPI_INT, TRACKER_RANK, TRACKER_TAG, MPI_COMM_WORLD);

	for (auto &file : files) {
		MPI_Send(file.first.c_str(), MAX_FILENAME, MPI_CHAR, TRACKER_RANK, TRACKER_TAG, MPI_COMM_WORLD);
		int num_chunks = file.second.size();
		MPI_Send(&num_chunks, 1, MPI_INT, TRACKER_RANK, TRACKER_TAG, MPI_COMM_WORLD);
		for (auto &chunk : file.second) {
			MPI_Send(chunk.c_str(), HASH_SIZE, MPI_CHAR, TRACKER_RANK, TRACKER_TAG, MPI_COMM_WORLD);
		}
	}
}

void peer(int numtasks, int rank) {
	pthread_t download_thread;
	pthread_t upload_thread;
	void *status;
	int r;

	pthread_mutex_init(&mutex, NULL);
	string s = "in" + to_string(rank) + ".txt";
	ifstream in(s);

	in >> nr_owned_files;

	for (int i = 0; i < nr_owned_files; i++) {
		string filename;
		int chunks;
		in >> filename;
		in >> chunks;
		files[filename] = vector<string>(chunks);
		for (int j = 0; j < chunks; j++) {
			in >> files[filename][j];
		}
	}

	in >> nr_requested_files;

	requested_files = vector<string>(nr_requested_files);
	for (int i = 0; i < nr_requested_files; i++) {
		in >> requested_files[i];
	}

	send_owned_files(rank);

	MPI_Recv(NULL, 0, MPI_CHAR, TRACKER_RANK, TRACKER_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

	r = pthread_create(&download_thread, NULL, download_thread_func, (void *) &rank);
	if (r) {
		printf("Eroare la crearea thread-ului de download\n");
		exit(-1);
	}

	r = pthread_create(&upload_thread, NULL, upload_thread_func, (void *) &rank);
	if (r) {
		printf("Eroare la crearea thread-ului de upload\n");
		exit(-1);
	}

	r = pthread_join(download_thread, &status);
	if (r) {
		printf("Eroare la asteptarea thread-ului de download\n");
		exit(-1);
	}

	r = pthread_join(upload_thread, &status);
	if (r) {
		printf("Eroare la asteptarea thread-ului de upload\n");
		exit(-1);
	}
}
 
int main (int argc, char *argv[]) {
	int numtasks, rank;
 
	int provided;
	MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
	if (provided < MPI_THREAD_MULTIPLE) {
		fprintf(stderr, "MPI nu are suport pentru multi-threading\n");
		exit(-1);
	}
	MPI_Comm_size(MPI_COMM_WORLD, &numtasks);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);

	if (rank == TRACKER_RANK) {
		tracker(numtasks, rank);
	} else {
		peer(numtasks, rank);
	}

	MPI_Finalize();
}
