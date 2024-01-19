# Docker Build

This is most of what you need to screw around with docker.

Basically the Dockerifle here is ubuntu 18 + juce required deps
+ cmake 3.28 + gcc11

## Making the image

```
docker build --tag baconpaul/sst-dockerimages-ubuntu18:2
docker push baconpaul/sst-dockerimages-ubunutu18:2
```

## Running inside the image

It's all inte azure pipelines but

```
          export UID=$(id -u)
          export GID=$(id -g)

          docker pull baconpaul/sst-dockerimages-ubuntu18:3
          docker create --user $UID:$GID --name surge-build-u18 --interactive --tty \
              --volume=`pwd`:/home/build/surge baconpaul/sst-dockerimages-ubuntu18:3
          docker start surge-build-u18

          docker exec surge-build-u18 bash -c "(cd /home/build/surge && ./scripts/docker-build/build.sh build --target surge-xt_LV2 --parallel 6)"
          docker stop surge-build-u18

```
