FROM gcr.io/oss-fuzz-base/base-builder

RUN apt-get update && apt-get install -y cmake make

COPY . $SRC/project-tick
WORKDIR $SRC/project-tick

COPY .clusterfuzzlite/build.sh $SRC/build.sh
