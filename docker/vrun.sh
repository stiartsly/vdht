#!/bin/bash

if [ ! -e /var/run/vdht ]
then
   mkdir /var/run/vdht
fi

vdhtd -S -D -f /etc/vdht.conf


