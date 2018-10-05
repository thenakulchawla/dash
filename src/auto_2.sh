rm -rf ~/.dashcore_data_4
mkdir ~/.dashcore_data_3

./dashd -regtest -daemon -debug -use-raptor=1 -port=8336 -rpcport=8337 -datadir=/home/nchawla3/.dashcore_data_3 -conf=/home/nchawla3/.dashcore/dash.conf

sleep 10

./dash-cli -regtest -debug -use-raptor=1 -port=8336 -rpcport=8337 addnode "172.17.0.1:8330" "onetry"
./dash-cli -regtest -debug -use-raptor=1 -port=8336 -rpcport=8337 addnode "172.17.0.1:8332" "onetry"
./dash-cli -regtest -debug -use-raptor=1 -port=8336 -rpcport=8337 addnode "172.17.0.1:8334" "onetry"

# ./dash-cli -regtest -debug -use-raptor=1 -port=8334 -rpcport=8335 addnode "172.17.0.1:8336" "onetry"
#
#
# ./dash-cli -regtest -debug -use-raptor=1 -port=8330 -rpcport=8331 addnode "172.17.0.1:8336" "onetry"
#
# ./dash-cli -regtest -debug -use-raptor=1 -port=8332 -rpcport=8333 addnode "172.17.0.1:8336" "onetry"

sleep 1

./dash-cli -regtest -debug -use-raptor=1 -port=8336 -rpcport=8337 getconnectioncount

sleep 1
./dash-cli -regtest -debug -use-raptor=1 -port=8336 -rpcport=8337 generate 2
