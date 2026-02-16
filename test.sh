#!/bin/bash

# Change to the directory where this script is located
cd "$(dirname "$0")"


tests/grade.py phase1 --run-sh ./run.sh --test-dir "tests/phase1/public"
tests/grade.py phase1 --run-sh ./run.sh --test-dir "tests/phase1/private"
