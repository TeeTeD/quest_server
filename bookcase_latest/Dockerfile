FROM clean_ubuntu:v1

RUN apt-get update && apt-get install -y git python3 curl dos2unix

ENTRYPOINT /usr/local/bin/bookcase_latest/entrypoint.sh
