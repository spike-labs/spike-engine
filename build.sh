#!/bin/sh

arch=$2
if [ ! $2 ]; then
    arch=`arch`
fi

if [ $1 = "editor" ]; then
    scons -C godot target=editor arch=$arch custom_modules=../modules
elif [ $1 = "macos" ]; then
    scons -C godot target=template_release arch=arm64 custom_modules=../modules
    scons -C godot target=template_debug arch=arm64 custom_modules=../modules
    scons -C godot target=template_release arch=x86_64 custom_modules=../modules
    scons -C godot target=template_debug arch=x86_64 custom_modules=../modules
    lipo -create godot/bin/godot.macos.template_release.arm64 godot/bin/godot.macos.template_release.x86_64 -output godot/bin/templates/macos_template.app/Contents/MacOS/godot.macos.release.universal
    lipo -create godot/bin/godot.macos.template_debug.arm64 godot/bin/godot.macos.template_debug.x86_64 -output godot/bin/templates/macos_template.app/Contents/MacOS/godot.macos.debug.universal
    cd godot/bin/templates
    zip -r macos.zip macos_template.app/
elif [ $1 = "android" ]; then
    scons -C godot platform=android target=template_release arch=arm64 custom_modules=../modules
    scons -C godot platform=android target=template_debug arch=arm64 custom_modules=../modules
    scons -C godot platform=android target=template_release arch=arm32 custom_modules=../modules
    scons -C godot platform=android target=template_debug arch=arm32 custom_modules=../modules

    cd ./godot/platform/android/java
    ./gradlew generateGodotTemplates
elif [ $1 = "ios" ]; then
    scons -C godot platform=ios target=template_release custom_modules=../modules arch=arm64
    scons -C godot platform=ios target=template_debug custom_modules=../modules arch=arm64
    #scons platform=ios target=release custom_modules=../modules arch=x86_64
    cp godot/bin/libgodot.ios.template_release.arm64.a godot/bin/templates/ios/libgodot.ios.release.xcframework/ios-arm64/libgodot.a
    cp godot/bin/libgodot.ios.template_debug.arm64.a godot/bin/templates/ios/libgodot.ios.debug.xcframework/ios-arm64/libgodot.a
    #cp bin/libgodot.ios.opt.x86_64.a bin/templates/ios/libgodot.ios.debug.xcframework/ios-arm64_x86_64-simulator/libgodot.a
    #cp bin/libgodot.ios.opt.x86_64.a bin/templates/ios/libgodot.ios.release.xcframework/ios-arm64_x86_64-simulator/libgodot.a
    cd godot/bin/templates/ios
    zip -r ../ios.zip .
elif [ $1 = "web" ]; then
    scons -C godot platform=web target=template_release custom_modules=../modules
    mv godot/bin/godot.web.template_release.wasm32.zip godotbin/web_release.zip
    scons -C godot platform=web target=template_debug custom_modules=../modules
    mv godot/bin/godot.web.template_debug.wasm32.zip godotbin/web_debug.zip
elif [ $1 = "doc" ]; then
    cd modules
    python3 spike/doc/translations/extract.py --path */doc_classes --output spike/doc/translations/classes.pot
    cd spike
    python3 editor/translations/extract.py --path ..
else
    echo ./build.sh editor/macos/android/ios
fi
#cd godot
#scons tools=yes target=editor custom_modules=../modules
