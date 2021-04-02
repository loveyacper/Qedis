default: image

BUILDER_IMAGE := $(or $(BUILDER_IMAGE),hub.docker.com/r/cxspace/qedis)

image:
	docker build -f Dockerfile -t $(BUILDER_IMAGE) .

build-dev:
	docker run -it -v $(realpath ..):/Qedis hub.docker.com/r/cxspace/qedis /Qedis/ci/build-dev.sh;

build-run:
	docker run -it -p 127.0.0.1:6379:6379 -v $(realpath ..):/Qedis hub.docker.com/r/cxspace/qedis /Qedis/ci/run-server.sh;
