FROM clean_ubuntu:v1

RUN apt-get update && apt-get install -y git python3 curl dos2unix

# Копируем и исправляем перевод строк
COPY entrypoint.sh /usr/local/bin/
RUN dos2unix /usr/local/bin/entrypoint.sh && \
    chmod +x /usr/local/bin/entrypoint.sh

ENTRYPOINT ["/usr/local/bin/entrypoint.sh"]
