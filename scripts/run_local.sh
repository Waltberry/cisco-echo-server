#!/usr/bin/env bash
set -euo pipefail

make
./server 9090
