#!/bin/bash

echo "Deploying Scribbolyth files..."

cp ./build/bin/commands.conf ~/scribbolyth/
cp ./build/bin/regex.conf ~/scribbolyth/
cp ./build/bin/scribboleth.html ~/scribbolyth/
cp ./build/bin/scribbolyth ~/scribbolyth/

echo "Deployment complete."
