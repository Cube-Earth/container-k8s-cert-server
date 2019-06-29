#!/bin/bash
for f in `find "$(dirname "$0")" -type f -name "*.inc"`;do . "$f"; done

function start {
	umask 0007
	. /var/www/localhost/cgi-bin/functions.inc
	
	export KUBECONFIG="/var/www/.kube/config"
	
	ensureLocalServerCert "${SVC_NAME-pod-cert-server}"
		
	touch /tmp/certs.log
	chown apache:apache /tmp/certs.log

	/usr/sbin/httpd -DFOREGROUND
}

term_safe_start "start"
