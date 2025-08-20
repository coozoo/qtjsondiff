#!/bin/bash

sw_vers

QT_DIR=$(brew --prefix qt)
QMAKE=$QT_DIR/bin/qmake
MAC_DEPLOY_TOOL=$QT_DIR/bin/macdeployqt

if [ ! -x "$QMAKE" ]; then
    echo "qmake not found at $QMAKE. Make sure you have installed Qt6 with Homebrew."
    exit 1
fi

if [ ! -x "$MAC_DEPLOY_TOOL" ]; then
    echo "macdeployqt not found at $MAC_DEPLOY_TOOL. Make sure you have installed Qt6 with Homebrew."
    exit 1
fi

appget="$(cat *.pro |grep '^TARGET ='|awk -F\= '{print $2;}'| tr -d ' ')"

# BIN_DIR="./build-$appget-Release/"
APP_NAME=$appget
BUNDLE_NAME=$APP_NAME.app
VOL_NAME="$appget"

#DMG_DIR=${APP_NAME}_dmg
#DMG_PATH=./$APP_NAME.dmg

PROJECT_FILE=$(basename -- "*.pro")

main(){
    cd "${0/*}";
    compileProject

    RESULT=$?
    if [ $RESULT -ne 0 ] ; then
        echo Failed to CompileProject
        return $RESULT
    fi

    prepareBundle
    RESULT=$?
    if [ $RESULT -ne 0 ] ; then
        echo Failed to Prepare bundle
        return $RESULT
    fi
    
    app_ver=$(cat main.cpp |grep 'const QString APP'|awk -F\" '{print $2;}')
    makeDMG
    RESULT=$?
    if [ $RESULT -ne 0 ] ; then
        echo Failed to Make DMG file
        return $RESULT
    fi

    makeZIP
    RESULT=$?
    if [ $RESULT -ne 0 ] ; then
        echo Failed to Make ZIP file
        return $RESULT
    fi

    return 0
}

compileProject(){
    echo Compiling...
    
    $QMAKE $PROJECT_FILE -config release
    
    RESULT=$?
    if [ $RESULT -ne 0 ] ; then
        echo qmake failed, error code $RESULT
        return $RESULT
    fi

    make CXX="clang"
    RESULT=$?
    if [ $RESULT -ne 0 ] ; then
        echo make failed, error code $RESULT
        return $RESULT
    fi

    echo Done.
    return 0
}

prepareBundle(){
    CURRENT_DIR=$PWD
    echo $CURRENT_DIR
    echo Preparing Bundle...
    ls -l
    # cd $BIN_DIR
    if [[ ! -d $BUNDLE_NAME ]]
    then
        echo "$BUNDLE_NAME bundle doesn't exist."
        return 1
    fi
    ls -l

    
    # cd to Resources folder
    cd $BUNDLE_NAME
    ls -l
    cd Contents
    ls -l
    cd MacOS
    ls -l
    # copy appconfig to resources  
    mkdir ./lang
    cp ../../../.qm/*.qm ./lang/
    cp ../../../chart_rules.json .
    cp ../../../exec_history .
    cp ../../../filters_list .
    #cd ..
    #cd Contents
    #cd Resources

    cd ../../../
    ls -l
    # Add QtFramework libraries and required plugins into .app bundle. Update dependencies of binary files.
    $MAC_DEPLOY_TOOL $BUNDLE_NAME -verbose=3

    RESULT=$?
    if [ $RESULT -ne 0 ] ; then
        echo macdeployqt failed, error code $RESULT
        return $RESULT
    fi

    cd $CURRENT_DIR
    
  
    echo Done.
    
    otool -L $BUNDLE_NAME/Contents/MacOS/$APP_NAME
     
    return 0
}

makeDMG(){
    echo Making DMG File...

    if [ ! -d "$BUNDLE_NAME" ]
    then
        echo "$BUNDLE_NAME bundle doesn't exist."
        return 1
    fi
    
    rm -f "$APP_NAME".dmg

    hdiutil create -format UDZO -srcfolder "$BUNDLE_NAME" "$APP_NAME"_"$app_ver".dmg
    
    echo Done.
    return 0
}

makeZIP(){
    echo Making ZIP File...

    if [ ! -d "$BUNDLE_NAME" ]
    then
        echo "$BUNDLE_NAME bundle doesn't exist."
        return 1
    fi
    
    rm -f "$APP_NAME"_"$app_ver".zip

    zip -r -X "$APP_NAME"_"$app_ver".zip "$BUNDLE_NAME"
    
    echo Done.
    return 0
}

main
exit $?

