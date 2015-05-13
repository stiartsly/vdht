#!/bin/bash

if [ ! -e /var/run/vdht ]
then
   mkdir /var/run/vdht
fi

/root/vdht/vdhtd -S


