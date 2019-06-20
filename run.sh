#!/bin/bash
for f in `find "$(dirname "$0")" -type f -name "*.inc"`;do . "$f"; done

function start {
	. /var/www/localhost/cgi-bin/functions.inc
	
	export KUBECONFIG="/var/www/.kube/config"
	
	ensureToken
	ensureRootCA
	ensureServerCert "$SVC_NAME"
	
	mkdir -p /etc/apache2/certs
	cp /certs/k8s-$SVC_NAME.key /etc/apache2/certs/server.key
	cp /certs/k8s-$SVC_NAME.cer /etc/apache2/certs/server.cer
	
	chmod -R 700 /etc/apache2/certs
	
	touch /tmp/certs.log
	chown apache:apache /tmp/certs.log /certs/*.lck

	/usr/sbin/httpd -DFOREGROUND
}

term_safe_start "start"
