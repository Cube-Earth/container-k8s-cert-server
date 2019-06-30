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

function downloadKey {
	kubectl get secret "$PREFIX-$POD_NAME-$CERT_KEY" -n "$POD_NAMESPACE" -o jsonpath='{.data.tls\.key}' | base64 -d > "$KEY_FILE"
}

function downloadCrt {
	kubectl get secret "$PREFIX-$POD_NAME-$CERT_KEY" -n "$POD_NAMESPACE" -o jsonpath='{.data.tls\.crt}' | base64 -d > "$CER_FILE"
}

function uploadTls {	
	kubectl create secret tls "$PREFIX-$POD_NAME-$CERT_KEY" -n "$POD_NAMESPACE" --key "$KEY_FILE" --cert "$CER_FILE"
}

function findDeploymentName {
	local json=$(kubectl get -n "$POD_NAMESPACE" --output json "$OWNER_KIND" "$OWNER_NAME" 2>> $LOG | jq '.metadata' 2>> $LOG)
	[[ $? -ne 0 ]] && return 1

	OWNER_KIND=""
	OWNER_NAME=""
	eval `echo $json | jq -r '.ownerReferences | select (.!=null) | .[] | "OWNER_KIND=\"\(.kind)\"\nOWNER_NAME=\"\(.name)\""'`
	
	[[ -z "${OWNER_KIND-}" ]] && return 0
	
	case "$OWNER_KIND" in
		ReplicaSet)
			findDeploymentName
			;;
		Deployment)
			DEPLOYMENT_NAME=$OWNER_NAME
			;;
	esac
}

function findPodData {
	local json=$(kubectl get -n "$POD_NAMESPACE" --output json pods 2>> $LOG | jq --arg ip "$REMOTE_ADDR" '.items[] | select(.status.podIP==$ip) | .metadata' 2>> $LOG)
	[[ $? -ne 0 ]] && return 1
	POD_NAME=`echo $json | jq -r '.name // empty'`
	[[ -z "${POD_NAME-}" ]] && echo "ERROR: Too few or too many pods with IP adress '$REMOTE_ADDR' found!" && return 1
	[[ "${POD_NAME:0:1}" = "_" ]] && echo "ERROR: Pod names must not start with underscores!" && return 1 
#	declare -A POD_ANNOTS
	eval `echo $json | jq -r ".annotations | select (.!=null) | to_entries|map(\"POD_ANNOTS['\\(.key)']='\\(.value|tostring)'\") | .[]" | grep -e "^POD_ANNOTS\\['[^']*'\\]='[^']*'$"` || err=$?
	
	DEPLOYMENT_NAME=""
	eval `echo $json | jq -r '.ownerReferences | select (.!=null) | .[] | "OWNER_KIND=\"\(.kind)\"\nOWNER_NAME=\"\(.name)\""'`
	findDeploymentName
	[[ -z "${DEPLOYMENT_NAME-}" ]] && echo "ERROR: Pod must be part of a Deployment!" && return 1
	POD_NAME=$DEPLOYMENT_NAME
}


function ensurePrivCert {
	KEY_FILE="$POD_DIR/$CERT_KEY.key"
	CER_FILE="$POD_DIR/$CERT_KEY.cer"
	local create_func=$1
	
	if [[ ! -f "$CER_FILE" ]]
	then
		mkdir -p $POD_DIR
		downloadCrt && err=0 || err=$?
		if [[ $err -eq 0 ]]
		then
			downloadKey
		else
			eval $create_func
			uploadTls
		fi
	fi	
}


function createRootCA {
	openssl req -x509 -nodes -days 3650 -newkey rsa:4096 -sha256 -keyout "$KEY_FILE" -out "$CER_FILE" -subj "/CN=root ca/OU=`createToken`/O=k8s"
}

function ensureRootCA {
	local old_ns=$POD_NAMESPACE
	POD_NAME=""
	POD_NAMESPACE="kube-system"
	POD_DIR="$CERTS_DIR/_root_"
	CERT_KEY="root-ca"

	CER_FILE="$POD_DIR/$CERT_KEY.cer"
	KEY_FILE="$POD_DIR/$CERT_KEY.key"
	TGZ_FILE="$POD_DIR/$CERT_KEY.tgz"
	
	LCK="$CERTS_DIR/_root_.lck"
	touch $LCK
	exec 200>"$LCK"
	flock 200 >> $LOG 2>&1
	
	if [[ ! -f "$TGZ_FILE" ]]
	then
		ensurePrivCert "createRootCA" >> $LOG 2>&1
		
		pushd "$POD_DIR" >> $LOG 2>&1
		tar fcz "$TGZ_FILE" *.cer >> $LOG 2>&1
		popd >> $LOG 2>&1
	fi
	
	POD_NAMESPACE=$old_ns
	CA_CER_FILE=$CER_FILE
	CA_KEY_FILE=$KEY_FILE
	CA_TGZ_FILE=$TGZ_FILE
	FILE=$CER_FILE

	exec 200>&-
}


function createServerCert {
	local csr="$POD_DIR/$CERT_KEY.csr"
	
	local i=0
	local dns=""
	for e in "${CERT_DNS[@]}"
	do
		[[ $i -gt 0 ]] && dns="$dns,"
		dns="${dns}DNS:$e"
	    i=$((i+1))
	done

	openssl genrsa -out "$KEY_FILE" 2048

	openssl req -new -sha256 \
    	-key "$KEY_FILE" \
    	-subj "/CN=$POD_NAME/CN=$CERT_KEY/O=k8s" \
    	-reqexts SAN \
    	-config <(cat /etc/ssl/openssl.cnf \
        		<(printf "\n[SAN]\nsubjectAltName=$dns")) \
		-out "$csr"
		    
	openssl req -in "$csr" -noout -text
		
	openssl x509 -req -in "$csr" -extensions SAN \
	    -extfile <(cat /etc/ssl/openssl.cnf \
               <(printf "\n[SAN]\nsubjectAltName=$dns")) \
        -CA "$CA_CER_FILE" -CAkey "$CA_KEY_FILE" -CAcreateserial -out "$CER_FILE" -days 500 -sha256
	
	rm "$csr"
		
	openssl x509 -in "$CER_FILE" -text -noout
}


function ensureServerCert {
	ensurePrivCert "createServerCert"  >> $LOG 2>&1
}

function ensureAuthCert {
	IFS='.' read -r -a tmp <<< "$CERT_KEY"
	local POD_NAME="${tmp[0]}"
	local CERT_KEY="${tmp[1]}"
	local TARGET_POD_DIR=$POD_DIR
	local POD_DIR="$CERTS_DIR/_auth_/$POD_NAME"
	ensureServerCert
	mkdir -p "$TARGET_POD_DIR"
	cp "$KEY_FILE" "$TARGET_POD_DIR/$POD_NAME-$CERT_KEY.key"
	cp "$CER_FILE" "$TARGET_POD_DIR/$POD_NAME-$CERT_KEY.cer"
}

function ensureLocalServerCert {
	ensureRootCA
	
	local POD_NAME="local"
	local CERT_DNS="$1"
	local CERT_KEY="${2-server}"
	local POD_DIR="$CERTS_DIR/_local_"
	ensureServerCert
}

function sendResponse {
	echo "Status: 200 OK"
	echo "Content-Length: `stat -c %s "$FILE"`"
	echo "Content-Type: application/octet-stream"
	echo "Content-Disposition: attachment; filename=\"`basename $FILE`\""
	echo
	cat $FILE
}

function createCerts {
	ensureRootCA
	
	declare -A POD_ANNOTS
	findPodData >> $LOG 2>&1
	POD_DIR="$CERTS_DIR/$POD_NAME"
	
	# TODO: how to handle StateFulSets?? (due to dynamic numbering ? --> POD_NAME, TGZ, DNS, etc.) --> use wildcard in DNS
	
	TGZ="$CERTS_DIR/$POD_NAME.tgz"
	FILE="$TGZ"
	
	LCK="$CERTS_DIR/$POD_NAME.lck"
	touch $LCK
	exec 200>"$LCK"
	flock 200 >> $LOG 2>&1
	
	[[ -f "$TGZ" ]] && return 0
	
	local n=0
	for key in "${!POD_ANNOTS[@]}"
	do
		CERT_KEY=`echo "$key" | awk '/^ssl-cert\..+$/ { match($0, /\..*$/); print substr($0,RSTART+1,RLENGTH-1) }'`
		if [[ ! -z "${CERT_KEY-}" ]]
		then
			IFS=':' read -r -a CERT_PARAMS <<< "${POD_ANNOTS[$key]}"
			CERT_TYPE=${CERT_PARAMS[0]-undefined}
			IFS=',' read -r -a CERT_DNS <<< "${CERT_PARAMS[1]-$POD_NAME}"
			
			case "$CERT_TYPE" in
				server)
					ensureServerCert
					n=$((n+1))
					;;

				shared)
					ensureAuthCert
					n=$((n+1))
					;;

			esac
		fi
	done
	
#	[[ "$n" -eq 0 ]] && echo "Status: 500 Missing annotation 'ssl-cert.*' to configure certificate creations!" && return 1
#	[[ "$n" -eq 0 ]] && FILE="$CA_TGZ_FILE" && return 0
	
	cp "$CA_CER_FILE" "$POD_DIR"
	
	pushd "$POD_DIR" >> $LOG 2>&1
	tar fcz "$TGZ" * >> $LOG 2>&1
	popd >> $LOG 2>&1
	
	rm -Rf "$POD_DIR"
	
	FILE=$TGZ
}
