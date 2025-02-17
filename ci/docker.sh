
docker build -t chimaera docker -f docker/deps.Dockerfile
docker tag chimaera lukemartinlogan/chimaera-deps:latest
docker push lukemartinlogan/chimaera-deps:latest

VERSION=v1.0.0
docker tag chimaera lukemartinlogan/chimaera-deps:${VERSION}
docker push lukemartinlogan/chimaera-deps:${VERSION}