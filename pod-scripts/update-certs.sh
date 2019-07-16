#!/bin/lsh

[ $(ls -1A /certs | wc -l) -ne 0 ] && echo "/certs already initialized." && exit 0

os=$(cat /etc/os-release | awk '/^ID=/ { sub(/^ID=/, ""); print $0 }')

case "$os" in
	debian|ubuntu)
		$DOWNLOAD http://pod-cert-server/root-ca > /usr/local/share/ca-certificates/k8s-root-ca.crt
		update-ca-certificates
		;;

	centos)
		update-ca-trust force-enable
		$DOWNLOAD http://pod-cert-server/root-ca > /etc/pki/ca-trust/source/anchors/k8s-root-ca.crt
		update-ca-trust extract
		;;

	alpine)
		$DOWNLOAD http://pod-cert-server/root-ca > /usr/local/share/ca-certificates/k8s-root-ca.crt
		update-ca-certificates
		;;
esac

#openssl s_client -showcerts -connect pod-cert-server:443
#wget -qO- https://pod-cert-server/hello
#curl https://pod-cert-server/hello
	
$DOWNLOAD https://pod-cert-server/certs | tar xz -oC /certs
chmod -R 755 /certs
