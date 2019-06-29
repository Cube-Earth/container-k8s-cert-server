FROM alpine

RUN apk --no-cache add apache2 apache2-ssl curl bash openssl jq && \
	mkdir -p /run/apache2 /certs && \
	chown apache:apache /run/apache2 /certs && \
	chmod g+sw /certs
	
RUN curl -Lo /usr/local/bin/kubectl https://storage.googleapis.com/kubernetes-release/release/$(curl -s https://storage.googleapis.com/kubernetes-release/release/stable.txt)/bin/linux/amd64/kubectl && \
	chmod +x /usr/local/bin/kubectl

COPY httpd.conf /etc/apache2/
COPY run.sh /usr/local/bin/
COPY cgi/* /var/www/localhost/cgi-bin/
ADD https://raw.githubusercontent.com/Cube-Earth/Scripts/master/shell/docker/term_safe_start.inc /usr/local/bin/

EXPOSE 80 443

ENTRYPOINT [ "/usr/local/bin/run.sh" ]
