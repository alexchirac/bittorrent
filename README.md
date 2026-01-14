# BitNet - Distributed P2P File Sharing

A high-performance distributed peer-to-peer file sharing system built with MPI (Message Passing Interface) and pthreads. BitNet implements a BitTorrent-like architecture for efficient file distribution across a network of peers.

## Features

- **Distributed Architecture**: Uses MPI for inter-process communication across multiple nodes
- **Multi-threaded Design**: Concurrent download and upload threads for non-blocking operations
- **Tracker-based System**: Centralized tracker for peer coordination and metadata management
- **Chunk-based Transfer**: Files are split into chunks for parallel downloading from multiple peers
- **Hash Verification**: Built-in cryptographic verification of downloaded chunks
- **Load Balancing**: Round-robin peer selection for balanced load distribution

## Architecture

### Components

1. **Tracker (Rank 0)**: Maintains file metadata, peer lists, and coordinates downloads
2. **Peer Nodes**: Download and upload files concurrently with multiple threads

### Communication Protocol

- **TRACKER_TAG (0)**: Client-tracker communication
- **INTER_PEER_TAG (1)**: Peer-to-peer chunk requests
- **CHUNK_TAG (2)**: Chunk data transfer

## System Flow

### 1. Initialization Phase
- Each peer reads its input file and loads locally owned files
- Peer sends file metadata (hashes) to tracker
- Tracker registers files and seed information

### 2. Download Phase
- Peers request peer lists from tracker for desired files
- Tracker maintains up-to-date peer lists (updated every 10 chunks)
- Peers request chunks from other peers in round-robin fashion
- Chunks are verified against known hashes before acceptance

### 3. Completion Phase
- After all chunks downloaded, peer saves file locally
- Tracker is notified of completion
- Once all peers finish, tracker signals shutdown

## Building

### Prerequisites
- MPI implementation (OpenMPI or MPICH)
- C++11 compatible compiler
- pthread support
- GNU Make

### Compile

```bash
cd src
make build
```

This creates the executable at `bin/bitnet`.

### Clean

```bash
make clean
```

## Usage

### Running with MPI

```bash
# Run with N processes
mpirun -np 4 ./bin/bitnet

# With hostfile
mpirun -hostfile hosts.txt -np 8 ./bin/bitnet

# With OpenMPI specific options
mpirun -np 4 --mca mpi_cuda_support 0 ./bin/bitnet
```

### Input Format

Each peer requires an input file with the following format:

```
<num_files> <num_requested_files>
<filename1> <hash1>
<filename2> <hash2>
...
```

Requested files should be specified after owned files.

### Output Format

Downloaded files are saved as `client<rank>_<filename>` containing one hash per line.

## Configuration

Edit the constants in `src/tema2.cpp` to customize:

```cpp
#define MAX_FILES 10        // Maximum files per peer
#define MAX_FILENAME 15     // Max filename length
#define HASH_SIZE 32        // Hash string size
#define MAX_CHUNKS 100      // Maximum chunks per file
```

## Docker Support

Build and run with Docker:

```bash
docker build -t bitnet .
docker-compose up
```

## Performance Considerations

- Chunk size and update frequency impact bandwidth efficiency
- Round-robin selection ensures no single peer is overwhelmed
- Mutex protection on concurrent file updates prevents data corruption
- Scalable to hundreds of peers with proper resource allocation

## Thread Model

- **Download Thread**: Handles chunk requests and downloads
- **Upload Thread**: Services peer requests (implicit in peer communication)
- Both threads protected with mutex locks for file data integrity

## Error Handling

- Peers validate chunk hashes against tracker's known values
- Invalid chunks trigger retry with next peer
- Peer failures handled gracefully with load redistribution

## Testing

Test cases included in `checker/tests/` directory:
- test1: Basic single-file transfer
- test2: Multiple files with multiple peers
- test3: Large-scale distribution
- test4: Complex peer interactions

Run tests with:
```bash
cd checker
./checker.sh
```

## Project Structure

```
.
├── src/
│   ├── tema2.cpp          # Main implementation
│   ├── Makefile           # Build configuration
│   ├── README             # Implementation notes
│   └── in{1,2,3}.txt      # Sample input files
├── bin/
│   └── bitnet             # Compiled executable
├── checker/
│   ├── tests/             # Test cases
│   └── checker.sh         # Test runner
├── Dockerfile
├── docker-compose.yml
└── README.md              # This file
```

## Author

Stefan Alexandru Chirac

## License

Educational use only

## References

- MPI Standard: https://www.mpi-forum.org/
- BitTorrent Protocol: https://www.bittorrent.org/

## Notes

- This implementation prioritizes simplicity and correctness
- Production use requires additional error handling and optimization
- Hash verification assumes trusted tracker (no adversarial peers)
