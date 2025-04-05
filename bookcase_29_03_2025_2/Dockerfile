FROM clean_ubuntu:v1

RUN apt-get update && apt-get install -y cron
CMD ["sh", "-c", "service cron start && tail -f /dev/null"]