#!/bin/bash

set -o errexit
set -o nounset
set -o pipefail

LOG=/tmp/certs.log
CERTS_DIR=/certs
PREFIX=tls-managed

function createToken {
	openssl rand -hex 12
}

function doEnsureToken {
	local k="token"
	if [[ -f "$CERTS_DIR/token" ]]
	then
		export TOKEN=$(cat "$CERTS_DIR/token")
	else
		export TOKEN=`kubectl get secret "$PREFIX-$k" -n "$POD_NAMESPACE" -o jsonpath='{.data.token}' | base64 -d 2>> $LOG || err=$?`
		if [[ ! -z "${TOKEN-}" ]]
		then
			echo "$TOKEN" > "$CERTS_DIR/token"
		else
			export TOKEN=`createToken`
			
			kubectl create configmap "$CONFIGMAP_NAME" -n "$POD_NAMESPACE" --from-literal=root-ca.url="http://$SVC_NAME:8080/root-ca" --from-literal=server-cert.url="https://$SVC_NAME:8443/server-cert/$TOKEN" 2>> $LOG
			
			kubectl create secret generic "$PREFIX-$k" -n "$POD_NAMESPACE" --from-literal=token="$TOKEN" 2>> $LOG
			echo "$TOKEN" > "$CERTS_DIR/token"
		fi
	fi
}

function ensureToken {
	exec 200>"$CERTS_DIR/token.lck"
	flock -n 200 >> $LOG 2>&1
	doEnsureToken
}

function downloadKey {
	kubectl get secret "$PREFIX-$1" -n "$2" -o jsonpath='{.data.tls\.key}' | base64 -d > "$3"
}

function downloadCrt {
	kubectl get secret "$PREFIX-$1" -n "$2" -o jsonpath='{.data.tls\.crt}' | base64 -d > "$3"
}

function uploadTls {	
	kubectl create secret tls "$PREFIX-$1" -n "$2" --key "$3" --cert "$4"
}

function getFilePath {
	local n
	[[ "${1:0:1}" = "_" ]] && "Names must not start with underscores!" && return 1
	[[ "$2" = "kube-system" ]] && n="_$1" || n="$1"
	echo "$CERTS_DIR/k8s-$n$3"
}

function doEnsurePrivCert {
	local key=`getFilePath "$1" "$2" ".key"`
	local cer=`getFilePath "$1" "$2" ".cer"`
	local tgz=`getFilePath "$1" "$2" ".tgz"`
	local create_func=$3
	
	if [[ ! -f "$tgz" ]]
	then
		downloadCrt "$1" "$2" "$cer" && err=0 || err=$?
		if [[ $err -eq 0 ]]
		then
			downloadKey "$1" "$2" "$key"
		else
			eval $create_func "$key" "$cer" "$1" "$2"
			uploadTls "$1" "$2" "$key" "$cer"
		fi
		( cd "$CERTS_DIR"; tar fcz "$tgz" "`basename "$key"`" "`basename "$cer"`" )
	fi
	export KEY="$key"
	export CER="$cer"
	export FILE="$tgz"
}

function ensurePrivCert {
	local lck=`getFilePath "$1" "$2" ".lck"`
	exec 200>"$lck"
	flock -n 200
	doEnsurePrivCert "$1" "$2" "$3"
}

function createRootCA {
	openssl req -x509 -nodes -days 3650 -newkey rsa:4096 -sha256 -keyout "$1" -out "$2" -subj "/CN=root ca/OU=`createToken`/O=k8s"
}

function ensureRootCA {
	ensurePrivCert "root-ca" "kube-system" "createRootCA" >> $LOG 2>&1
	export CA_KEY=$KEY
	export CA_CER=$CER
	export FILE=$CER
}

function createServerCert {
	local csr=`getFilePath "$3" "$4" ".csr"`

	openssl genrsa -out "$1" 2048

	openssl req -new -sha256 \
    	-key "$1" \
    	-subj "/CN=$3/O=k8s" \
    	-reqexts SAN \
    	-config <(cat /etc/ssl/openssl.cnf \
        		<(printf "\n[SAN]\nsubjectAltName=DNS:$3,DNS:localhost")) \
		-out "$csr"
		    
	openssl req -in "$csr" -noout -text
		
	openssl x509 -req -in "$csr" -CA "$CA_CER" -CAkey "$CA_KEY" -CAcreateserial -out "$2" -days 500 -sha256
		
	openssl x509 -in "$2" -text -noout
}


function ensureServerCert {
	ensurePrivCert "$1" "$POD_NAMESPACE" "createServerCert"  >> $LOG 2>&1
}


function sendResponse {
	echo "Status: 200 OK"
	echo "Content-Length: `stat -c %s "$FILE"`"
	echo "Content-Type: application/octet-stream"
	echo "Content-Disposition: attachment; filename=\"`basename $FILE`\""
	echo
	cat $FILE
}
