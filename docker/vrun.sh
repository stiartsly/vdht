#!/bin/bash

if [ ! -e /var/run/vdht ]
then
   mkdir /var/run/vdht
fi

vdhtd -S -f /etc/vdht.conf


