#!/bin/bash

cd src/
rm -rf lte
git clone https://github.com/lucval/lte.git

rm -rf csma
git clone https://github.com/lucval/csma.git

rm -rf openflow
git clone https://github.com/lucval/openflow.git

rm csma/ipv4_17.h
mv csma/ipv4.* internet/model

cd ..
./waf build
