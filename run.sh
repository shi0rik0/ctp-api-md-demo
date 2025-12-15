#!/bin/bash

cd core && make run | python3 ../http_server/wrapper.py
