#!/bin/sh

mkdir -p build

xcodebuild -workspace xe.xcworkspace -scheme poc_snb_name_oob_read_server -configuration Release install DSTROOT=$PWD/build
xcodebuild -workspace xe.xcworkspace -scheme poc_snb_name_oob_read_client -configuration Release install DSTROOT=$PWD/build
xcodebuild -workspace xe.xcworkspace -scheme poc_sockaddr_oob_read -configuration Release install DSTROOT=$PWD/build
xcodebuild -workspace xe.xcworkspace -scheme poc_oob_write -configuration Release install DSTROOT=$PWD/build
xcodebuild -workspace xe.xcworkspace -scheme poc_double_free -configuration Release install DSTROOT=$PWD/build
xcodebuild -workspace xe.xcworkspace -scheme poc_info_disclosure -configuration Release install DSTROOT=$PWD/build

xcodebuild -workspace xe.xcworkspace -scheme xe_smbx -configuration Release install DSTROOT=$PWD/build
xcodebuild -workspace xe.xcworkspace -scheme xe_kmem -configuration Release install DSTROOT=$PWD/build
xcodebuild -workspace xe.xcworkspace -scheme demo_sb -configuration Release install DSTROOT=$PWD/build
xcodebuild -workspace xe.xcworkspace -scheme demo_entitlements -configuration Release install DSTROOT=$PWD/build
xcodebuild -workspace xe.xcworkspace -scheme demo_wp_disable -configuration Release install DSTROOT=$PWD/build
xcodebuild -workspace xe.xcworkspace -scheme demo_av -configuration Release install DSTROOT=$PWD/build
xcodebuild -workspace xe.xcworkspace -scheme demo_sudo -configuration Release install DSTROOT=$PWD/build
xcodebuild -workspace xe.xcworkspace -scheme test_general_remote -configuration Release install DSTROOT=$PWD/build
