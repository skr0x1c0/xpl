#!/usr/bin/env bash

CPP_DEFINITIONS="\$GCC_PREPROCESSOR_DEFINITIONS"
BUILD_DIR="$PWD/build"

case $1 in
    "MACOS_21E230_T6000_RELEASE")
        CPP_DEFINITIONS="$CPP_DEFINITIONS MACOS_21E230_T6000_RELEASE=1"
        BUILD_DIR="$BUILD_DIR/T6000"
        ;;
    "MACOS_21E230_T8101_RELEASE")
        CPP_DEFINITIONS="$CPP_DEFINITIONS MACOS_21E230_T8101_RELEASE=1"
        BUILD_DIR="$BUILD_DIR/T8101"
        ;;
    *)
        echo "[ERROR] invalid build configuration $1"
        echo "usage: build.sh <build configuration>"
        echo "valid configurations are MACOS_21E230_T6000_RELEASE and MACOS_21E230_T8101_RELEASE"
        exit 1
        ;;
esac

mkdir -p $BUILD_DIR

# POCs
SCHEMES=(poc_snb_name_oob_read_server poc_snb_name_oob_read_client poc_sockaddr_oob_read poc_oob_write poc_double_free poc_info_disclosure)
# Demos
SCHEMES+=(demo_sb demo_entitlements demo_wp_disable demo_av demo_sudo)
# SMB
SCHEMES+=(xe_smbx xe_kmem)
# Test
SCHEMES+=(test_general_remote)

for SCHEME in ${SCHEMES[@]}; do
    xcodebuild -workspace xe.xcworkspace -scheme $SCHEME -configuration Release install DSTROOT=$BUILD_DIR GCC_PREPROCESSOR_DEFINITIONS="$CPP_DEFINITIONS" || exit 1
done
