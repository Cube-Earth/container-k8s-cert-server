#!/bin/sh

set -o errexit
set -o nounset
set -o pipefail

os=$(cat /etc/os-release | awk '/^ID=/ { sub(/^ID=/, ""); print $0 }')

case "$os" in
	ubuntu)
		wget -qO /usr/local/share/ca-certificates/k8s-root-ca.crt http://pod-cert-server/root-ca
		update-ca-certificates
		;;

	centos)
		update-ca-trust force-enable
		wget -qO /etc/pki/ca-trust/source/anchors/k8s-root-ca.crt http://pod-cert-server/root-ca
		update-ca-trust extract
		;;

	alpine)
		wget -qO /usr/local/share/ca-certificates/k8s-root-ca.crt http://pod-cert-server/root-ca
		update-ca-certificates
		;;
esac

#openssl s_client -showcerts -connect pod-cert-server:443
wget -qO- https://pod-cert-server/hello
curl https://pod-cert-server/hello
	
curl -Ls https://pod-cert-server/certs | tar xz -oC /certs
chmod -R 755 /certs
