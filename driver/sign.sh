#!/bin/sh

# update keys below with your secureboot signing key paths
kmodsign sha512 /etc/secrets/secureboot/MOK.priv /etc/secrets/secureboot/MOK.der main.ko
