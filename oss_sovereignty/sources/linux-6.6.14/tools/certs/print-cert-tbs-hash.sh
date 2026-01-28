set -u -e -o pipefail
CERT="${1:-}"
BASENAME="$(basename -- "${BASH_SOURCE[0]}")"
if [ $
	echo "usage: ${BASENAME} <certificate>" >&2
	exit 1
fi
if ! PEM="$(openssl x509 -inform DER -in "${CERT}" 2>/dev/null || openssl x509 -in "${CERT}")"; then
	echo "ERROR: Failed to parse certificate" >&2
	exit 1
fi
RANGE_AND_DIGEST_RE='
2s/^\s*\([0-9]\+\):d=\s*[0-9]\+\s\+hl=\s*[0-9]\+\s\+l=\s*\([0-9]\+\)\s\+cons:\s*SEQUENCE\s*$/\1 \2/p;
7s/^\s*[0-9]\+:d=\s*[0-9]\+\s\+hl=\s*[0-9]\+\s\+l=\s*[0-9]\+\s\+prim:\s*OBJECT\s*:\(.*\)$/\1/p;
'
RANGE_AND_DIGEST=($(echo "${PEM}" | \
	openssl asn1parse -in - | \
	sed -n -e "${RANGE_AND_DIGEST_RE}"))
if [ "${
	echo "ERROR: Failed to parse TBSCertificate." >&2
	exit 1
fi
OFFSET="${RANGE_AND_DIGEST[0]}"
END="$(( OFFSET + RANGE_AND_DIGEST[1] ))"
DIGEST="${RANGE_AND_DIGEST[2]}"
DIGEST_MATCH=""
while read -r DIGEST_ITEM; do
	if [ -z "${DIGEST_ITEM}" ]; then
		break
	fi
	if echo "${DIGEST}" | grep -qiF "${DIGEST_ITEM}"; then
		DIGEST_MATCH="${DIGEST_ITEM}"
		break
	fi
done < <(openssl list -digest-commands | tr ' ' '\n' | sort -ur)
if [ -z "${DIGEST_MATCH}" ]; then
	echo "ERROR: Unknown digest algorithm: ${DIGEST}" >&2
	exit 1
fi
echo "${PEM}" | \
	openssl x509 -in - -outform DER | \
	dd "bs=1" "skip=${OFFSET}" "count=${END}" "status=none" | \
	openssl dgst "-${DIGEST_MATCH}" - | \
	awk '{printf "tbs:" $2}'
