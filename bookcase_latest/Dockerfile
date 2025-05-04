FROM clean_ubuntu:v1

RUN apt-get update && apt-get install -y git python3 curl dos2unix

ENTRYPOINT cd /usr/local/bin/bookcase_latest
CMD ls
