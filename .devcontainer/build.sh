#!/bin/bash

set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
IMAGE="haxe/neko_devcontainer"
TAG="$IMAGE:$(date +%Y%m%d%H%M%S)"

set -x

docker login
docker buildx use neko || docker buildx create --use --name neko
docker buildx build --pull --platform linux/amd64,linux/arm64 --tag "$TAG" --push "$DIR"

sed -i -e "s#$IMAGE:[0-9]*#$TAG#g" "$DIR/docker-compose.yml"
