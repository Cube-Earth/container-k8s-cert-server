#!/bin/bash
for f in `find "$(dirname "$0")" -type f -name "*.inc"`;do . "$f"; done

function start {
	. /var/www/localhost/cgi-bin/functions.inc
	
	export KUBECONFIG="/var/www/.kube/config"
	
	ensureLocalServerCert "$SVC_NAME"
	
	mkdir -p /etc/apache2/certs
	
	chmod -R 700 /etc/apache2/certs
	
	touch /tmp/certs.log
	chown apache:apache /tmp/certs.log /certs/*.lck

	/usr/sbin/httpd -DFOREGROUND
}

term_safe_start "start"
