FROM echolib

RUN apt-get update && apt-get install -y --no-install-recommends python3-opencv && \
    apt-get clean && rm -rf /var/lib/apt/lists/*

COPY python/examples/*.py /examples/