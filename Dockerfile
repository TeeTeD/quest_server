FROM clean_ubuntu:v1

RUN apt-get update && apt-get install -y git python3 curl

COPY entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh
ENTRYPOINT ["/entrypoint.sh"]