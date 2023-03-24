#!/bin/bash

sudo cp ./*.ko /opt/nfs/rootfs/lib/modules/4.1.15/
sudo cp ./$1 /opt/nfs/rootfs/lib/modules/4.1.15/
