#!/bin/bash

echo "Deploying Scribbolyth files..."

cp ./build/bin/commands.conf ~/scribbolyth/
rm -rf ~/scribbolyth/commands_detail
cp -r ./build/bin/commands_detail ~/scribbolyth/
cp ./build/bin/regex.conf ~/scribbolyth/
cp ./build/bin/scribboleth.html ~/scribbolyth/
cp ./build/bin/scribbolyth ~/scribbolyth/

echo "Deployment complete."
