# FAM-Graph

FAM-Graph is a semi-external graph processing system designed for the disaggregated FAM (Fabric Attached Memory) architecture.
# Build Instructions
FAM-Graph uses cmake
```
mkdir build
cd build
cmake ..
make
```

# Configuring you systems for RDMA
## Increasing Memory Limit
It is important to increase the amount of memory that you are allowed to pin, using ulimit.
## Configure IB interfaces with IP addresses for IPoIB
FAM-Graph uses the RDMA connection manager which in turn uses IPoIB to bootstrap QP connections. We have configured our client interface with:
```
sudo ifconfig ib0 192.168.12.1 netmask 255.255.255.0 up
```
And our server with:
```
sudo ifconfig ib0 192.168.12.3 netmask 255.255.255.0 up
```

# Running the system
The following examples use the input [MOLIERE_2016](https://sparse.tamu.edu/Sybrandt/MOLIERE_2016) (from the next section). Here are the commands run on both the client and server.

## Breadth First Search

Run on a directed or symmetric directed graph.

Server Command
```
./main -m server -e /mnt/graph1/fam-graph/MOLIERE.adj -t 10
```
Client Command
```
./main -m client -k bfs -i /mnt/graphs/fam-graph/MOLIERE.idx -t 10 --start-vertex 1
```
## Pagerank

Run on a directed graph.

Server Command
```
./main -m server -e /mnt/graph1/fam-graph/twitter7.adj -t 10
```
Client Command
```
./main -m client -k pagerank_delta -i /mnt/graphs/fam-graph/twitter7.idx -t 10
```
## Weakly Connected Components

Run on a symmetric directed graph.

Server Command
```
./main -m server -e /mnt/graph1/fam-graph/MOLIERE.adj -t 10
```
Client Command
```
./main -m client -k CC -i /mnt/graphs/fam-graph/MOLIERE.idx -t 10
```
## K-Core Decomposition

Run on a symmetric directed graph.

Server Command
```
./main -m server -e /mnt/graph1/fam-graph/MOLIERE.adj -t 10
```
Client Command
```
./main -m client -k kcore --kcore-k 200 -i /mnt/graphs/fam-graph/MOLIERE.idx -t 10
```
## Maximal Independent Set

Run on a symmetric directed graph.

Server Command
```
./main -m server -e /mnt/graph1/fam-graph/MOLIERE.adj -t 10
```
Client Command
```
./main -m client -k MIS -i /mnt/graphs/fam-graph/MOLIERE.idx -t 10
```

# Obtaining Inputs
The inputs used in the FAM-Graph paper are from [https://law.di.unimi.it/](https://law.di.unimi.it/datasets.php) and [https://sparse.tamu.edu/](https://sparse.tamu.edu/). The exact inputs used are:
- [clueweb12](https://law.di.unimi.it/webdata/clueweb12/)
- [AGATHA_2015](https://sparse.tamu.edu/Sybrandt/AGATHA_2015)
- [MOLIERE_2016](https://sparse.tamu.edu/Sybrandt/MOLIERE_2016)
- [sk-2005](https://sparse.tamu.edu/LAW/sk-2005)
- [twitter7](https://sparse.tamu.edu/SNAP/twitter7)
