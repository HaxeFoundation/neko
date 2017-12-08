#!/bin/bash 

set -ex

if [ -z $haxeci_decrypt ]; then
    echo "haxeci_decrypt is unset, skip deploy"
    exit
fi

openssl aes-256-cbc -k "$haxeci_decrypt" -in haxeci_ssh.enc -out haxeci_ssh -d
chmod 600 haxeci_ssh
eval `ssh-agent -s`
ssh-add haxeci_ssh
openssl aes-256-cbc -k "$haxeci_decrypt" -in haxeci_sec.gpg.enc -out haxeci_sec.gpg -d
gpg --allow-secret-key-import --import haxeci_sec.gpg
sudo apt-get install devscripts git-buildpackage ubuntu-dev-tools dh-make dh-apache2 -y
git config --global user.name "${DEBFULLNAME}"
git config --global user.email "${DEBEMAIL}"

pushd build
    make upload_to_ppa
popd
