# pkill dashd
rm -rf ~/.dashcore_data_1
rm -rf ~/.dashcore_data_2
rm -rf ~/.dashcore_data_3

mkdir ~/.dashcore_data_1
mkdir ~/.dashcore_data_2
mkdir ~/.dashcore_data_4

./dashd -regtest -daemon -debug -use-raptor=1 -port=8330 -rpcport=8331 -datadir=/home/nchawla3/.dashcore_data_1 -conf=/home/nchawla3/.dashcore/dash.conf

sleep 10

./dashd -regtest -daemon -debug -use-raptor=1 -port=8332 -rpcport=8333 -datadir=/home/nchawla3/.dashcore_data_2 -conf=/home/nchawla3/.dashcore/dash.conf

sleep 10

./dashd -regtest -daemon -debug -use-raptor=1 -port=8334 -rpcport=8335 -datadir=/home/nchawla3/.dashcore_data_4 -conf=/home/nchawla3/.dashcore/dash.conf

sleep 10
 
# ./dash-cli -regtest -debug -use-raptor=1 -port=8332 -rpcport=8333 addnode "172.17.0.1:8330" "onetry"

./dash-cli -regtest -debug -use-raptor=1 -port=8330 -rpcport=8331 addnode "172.17.0.1:8332" "onetry"
./dash-cli -regtest -debug -use-raptor=1 -port=8330 -rpcport=8331 addnode "172.17.0.1:8334" "onetry"

./dash-cli -regtest -debug -use-raptor=1 -port=8332 -rpcport=8333 addnode "172.17.0.1:8334" "onetry"


./dash-cli -regtest -debug -use-raptor=1 -port=8334 -rpcport=8335 addnode "172.17.0.1:8330" "onetry"
./dash-cli -regtest -debug -use-raptor=1 -port=8334 -rpcport=8335 addnode "172.17.0.1:8332" "onetry"


sleep 1
./dash-cli -regtest -debug -use-raptor=1 -port=8330 -rpcport=8331 getconnectioncount
./dash-cli -regtest -debug -use-raptor=1 -port=8332 -rpcport=8333 getconnectioncount
./dash-cli -regtest -debug -use-raptor=1 -port=8334 -rpcport=8335 getconnectioncount

sleep 1
./dash-cli -regtest -debug -use-raptor=1 -port=8332 -rpcport=8333 generate 2
sleep 1
./dash-cli -regtest -debug -use-raptor=1 -port=8330 -rpcport=8331 generate 2
./dash-cli -regtest -debug -use-raptor=1 -port=8334 -rpcport=8335 generate 2
sleep 10


# ./dash-cli -regtest -debug -use-raptor=1 -port=8330 -rpcport=8331 getbalance 
# ./dash-cli -regtest -debug -use-raptor=1 -port=8332 -rpcport=8333 getbalance
# ./dash-cli -regtest -debug -use-raptor=1 -port=8334 -rpcport=8335 getbalance
# ./dash-cli -regtest -debug -use-raptor=1 -port=8336 -rpcport=8337 getbalance

