export ROCE=0
export APT=1
NUM_SERVERS=1

node_map=(0 1 2 3)
cpu_map=(0 8 16 24 1 9 17 25 2 10 18 26 3 11 19 27 4 12 20 28 5 13 21 29 6 14 22 30 7 15 23 31) 


rm -rf client-tput
mkdir client-tput
killall main
for i in `seq 1 $NUM_SERVERS`; do
	id=`expr $i - 1`
	sock_port=`expr 5500 + $i - 1`

	if [ $APT -eq 1 ]
	then
	    val=$(($i%4)) 
        val1=$(($i%32))
		numactl -C 16  ./main $id $sock_port &
                arg1= pgrep main
#                ps -p $arg1 -o %cpu,%mem,cmd	
		#numactl -N ${node_map[val]} -m ${node_map[val]} -C  ${cpu_map[val1]} strace -c ./main $id $sock_port &
	else
		if [ $ROCE -eq 1 ]
		then
			core=`expr 0 + $id`
			 numactl --physcpubind $core --interleave 0,1 ./main $id $sock_port &
		else
			core=`expr 32 + $id`
			 numactl --physcpubind $core --interleave 4,5 ./main $id $sock_port &
		fi
	fi
	
	sleep .1
done
