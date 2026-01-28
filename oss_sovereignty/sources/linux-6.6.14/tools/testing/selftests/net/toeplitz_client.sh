send_traffic() {
	expiration=$((SECONDS+20))
	while [[ "${SECONDS}" -lt "${expiration}" ]]
	do
		if [[ "${PROTO}" == "-u" ]]; then
			echo "msg $i" | nc "${IPVER}" -u -w 0 "${ADDR}" "${PORT}"
		else
			echo "msg $i" | nc "${IPVER}" -w 0 "${ADDR}" "${PORT}"
		fi
		sleep 0.001
	done
}
PROTO=$1
IPVER=$2
ADDR=$3
PORT=$4
send_traffic
