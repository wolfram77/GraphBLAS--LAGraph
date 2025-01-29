#!/usr/bin/env bash
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --ntasks-per-node=1
#SBATCH --cpus-per-task=64
#SBATCH --exclusive
#SBATCH --job-name slurm
#SBATCH --output=slurm.out
# source scl_source enable gcc-toolset-11
# module load hpcx-2.7.0/hpcx-ompi
# module load openmpi/4.1.5
# module load cuda/12.3
source /opt/rh/gcc-toolset-13/enable
src="GraphBLAS--LAGraph"
out="$HOME/Logs/$src$1.log"
ulimit -s unlimited
printf "" > "$out"

# Download GraphBLAS, LAGraph and install GraphBLAS
if [[ "$DOWNLOAD" != "0" ]]; then
  rm -rf GraphBLAS $src
  git clone https://github.com/DrTimothyAldenDavis/GraphBLAS
  git clone https://github.com/wolfram77/$src
  cd GraphBLAS
  make uninstall
  make JOBS=32
  make install
  cd ..
  cd $src
fi

# Build LAGraph
make JOBS=32
# make test JOBS=32

# Run on a graph
runOne() {
  # $1: input file name
  # $2: is graph weighted (0/1)
  # $3: is graph symmetric (0/1)
  stdbuf --output=L printf "Running on $1 ...\n"                                       | tee -a "$out"
  stdbuf --output=L ./build/experimental/benchmark/helloworld_demo "$1" "$2" "$3" 2>&1 | tee -a "$out"
  stdbuf --output=L printf "\n\n"                                                      | tee -a "$out"
}

# Run on all graphs
runAll() {
  # runOne "$HOME/Data/web-Stanford.mtx"    0 0
  runOne "$HOME/Data/indochina-2004.mtx"  0 0
  runOne "$HOME/Data/uk-2002.mtx"         0 0
  runOne "$HOME/Data/arabic-2005.mtx"     0 0
  runOne "$HOME/Data/uk-2005.mtx"         0 0
  runOne "$HOME/Data/webbase-2001.mtx"    0 0
  runOne "$HOME/Data/it-2004.mtx"         0 0
  runOne "$HOME/Data/sk-2005.mtx"         0 0
  runOne "$HOME/Data/com-LiveJournal.mtx" 0 1
  runOne "$HOME/Data/com-Orkut.mtx"       0 1
  runOne "$HOME/Data/asia_osm.mtx"        0 1
  runOne "$HOME/Data/europe_osm.mtx"      0 1
  runOne "$HOME/Data/kmer_A2a.mtx"        0 1
  runOne "$HOME/Data/kmer_V1r.mtx"        0 1
}

# Run 5 times
for i in {1..5}; do
  runAll
done

# Signal completion
curl -X POST "https://maker.ifttt.com/trigger/puzzlef/with/key/${IFTTT_KEY}?value1=$src$1"
