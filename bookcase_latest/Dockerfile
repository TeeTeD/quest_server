FROM clean_ubuntu:v1

RUN apt-get update && apt-get install -y git python3 curl

COPY entrypoint.sh /usr/local/bin/
RUN chmod +x /usr/local/bin/entrypoint.sh

ENTRYPOINT ["/usr/local/bin/entrypoint.sh"]
