#!/bin/bash

APP="/home/$USER/FAM-Graph/build/src/main"
REMOTE_DIR="/home/$USER/fg-bin/"
MEMORY_SERVER=""
COMPUTE_SERVER=""
MEMORY_SERVER_IPoIB="192.168.12.1"
COMPUTE_SERVER_IPoIB="192.168.12.2"
THREADS=${THREADS:-"10"} 
HP=${HP:-"--hp"} 
DB=${DB:-""}
NET_SCRATCH="/net/netscratch/fam-graph"
COMPUTE_GRAPH_DIR=$NET_SCRATCH
SERVER_GRAPH_DIR=$NET_SCRATCH

UNDIRECTED_GRAPHS=("AGATHA" "MOLIERE" "sk-2005-undirected" "twitter7-undirected")
DIRECTED_GRAPHS=("twitter7" "sk-2005") # "parmat_360000000_9000000000" "twitter7" "sk-2005" "clueweb12"
BFS_START_NODES=("1" "1" "42265166" "1")
KCORE_K=("100" "135" "200" "200")

OUTDIR="/tmp"
mkdir -p $OUTDIR
mkdir -p results
RESULT_FILE=results/${RESULT_FILE:-results}.txt
ATTR=${ATTR:-NONE}
echo 'system,input,app,attributes,runtime,spin_time,function_time,threads' > $RESULT_FILE

START_SERVER() {
    clush -w $MEMORY_SERVER "source .profile > /dev/null; ${REMOTE_DIR}main -m server -a $MEMORY_SERVER_IPoIB -e $SERVER_GRAPH_DIR/$1".adj" -t $THREADS $HP" &
    sleep 20
}

START_CLIENT() {
    # $1 APP
    # $2 INPUT twitter7-undirected
    # $3 extra options
    clush -w $COMPUTE_SERVER "source .profile > /dev/null; ${REMOTE_DIR}main -m client -a $MEMORY_SERVER_IPoIB -k $1 -i $COMPUTE_GRAPH_DIR/$2.idx -t $THREADS $HP $DB $NUMA $3" | tee $OUTDIR/$1-$2.txt
    local TIME=$( cat $OUTDIR/$1-$2.txt | perl -nle 'm/Running Time\(s\): ([-+]?[0-9]*\.?[0-9]+)/ and print $1;')
    local SPIN_TIME=$( cat $OUTDIR/$1-$2.txt | perl -nle 'm/Total Spin Time \(s\): ([-+]?[0-9]*\.?[0-9]+)/ and print $1;')
    local FUNCTION_TIME=$( cat $OUTDIR/$1-$2.txt | perl -nle 'm/Total Function Time \(s\) ([-+]?[0-9]*\.?[0-9]+)/ and print $1;')
    echo "FAM-Graph,$2,$1,$ATTR,$TIME,$SPIN_TIME,$FUNCTION_TIME,$THREADS" >> $RESULT_FILE
}

SEND_BINARY() {
    clush -w $COMPUTE_SERVER "mkdir -p $REMOTE_DIR"
    clush -w $MEMORY_SERVER "mkdir -p $REMOTE_DIR"
    scp $APP $USER@$COMPUTE_SERVER:$REMOTE_DIR
    scp $APP $USER@$MEMORY_SERVER:$REMOTE_DIR
}

RUN_APP () {
    START_SERVER "$2"
    START_CLIENT "$1" "$2" "$3"    
}

RUN_CC_ALL () {
    for INPUT in "${UNDIRECTED_GRAPHS[@]}"; do
        RUN_APP "CC" $INPUT
    done
}

RUN_BFS_ALL () {
    for i in "${!UNDIRECTED_GRAPHS[@]}"; do
        echo "${UNDIRECTED_GRAPHS[$i]}"
        RUN_APP "bfs" "${UNDIRECTED_GRAPHS[$i]}" "--start-vertex ${BFS_START_NODES[$i]}"
    done
}

RUN_KCORE_ALL () {
    for i in "${!UNDIRECTED_GRAPHS[@]}"; do
        echo "${UNDIRECTED_GRAPHS[$i]}"
        RUN_APP "kcore" "${UNDIRECTED_GRAPHS[$i]}" "--kcore-k ${KCORE_K[$i]}"
    done
}

RUN_MIS_ALL () {
    for INPUT in "${UNDIRECTED_GRAPHS[@]}"; do
        RUN_APP "MIS" $INPUT
    done
}

RUN_PAGERANK_ALL () {
    for INPUT in "${DIRECTED_GRAPHS[@]}"; do
        RUN_APP "pagerank_delta" $INPUT
    done
}

SEND_BINARY
# RUN_PAGERANK_ALL
# RUN_MIS_ALL
RUN_KCORE_ALL
RUN_CC_ALL
RUN_BFS_ALL
