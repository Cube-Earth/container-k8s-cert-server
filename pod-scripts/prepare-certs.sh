#!/bin/lsh

os=$(cat /etc/os-release | awk '/^ID=/ { sub(/^ID=/, ""); print $0 }')

case "$os" in
	debian|ubuntu)
		apt-get update
		apt-get install -y curl wget openssl
		;;

	centos)
		yum install -y ca-certificates wget
		;;

	alpine)
		apk add --no-cache ca-certificates wget curl openssl
		mkdir -p /etc/pki/tls/certs
		ln -s /etc/ssl/certs/ca-certificates.crt /etc/pki/tls/certs/ca-bundle.crt  # for curl
		;;
esac

$DOWNLOAD https://raw.githubusercontent.com/Cube-Earth/container-k8s-cert-server/master/pod-scripts/update-certs.sh > /usr/local/bin/update-certs.sh
chmod +x /usr/local/bin/update-certs.sh

mkdir /certs
chmod 777 /certs
