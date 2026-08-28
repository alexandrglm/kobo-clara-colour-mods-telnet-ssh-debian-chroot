#!/bin/sh
export PYTHONHOME=/mnt/onboard/.python/usr
export PYTHONPATH=/mnt/onboard/.python/usr/lib/python3.11
export LD_LIBRARY_PATH=/mnt/onboard/.python/usr/lib:$LD_LIBRARY_PATH
exec /mnt/onboard/.python/usr/bin/python3.11 /mnt/onboard/.python/drm.py "$@"
