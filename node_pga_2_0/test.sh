#!/bin/bash -xe

curl http://localhost:8080/s/auth/login -H "Cookie: pga=$1" -v

