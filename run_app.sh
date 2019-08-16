#!/bin/bash

rm -r ./build
idf.py build erase_flash flash monitor
