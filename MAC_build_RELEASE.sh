#!/bin/bash

QT_DIR=/usr/local/Cellar/qt/5.14.0
QMAKE=$QT_DIR/bin/qmake
MAC_DEPLOY_TOOL=$QT_DIR/bin/macdeployqt

BIN_DIR=./build-qtjsondiff-Release/
APP_NAME=QTjsonDiff
BUNDLE_NAME=$APP_NAME.app
VOL_NAME="qtjsondiff"

#DMG_DIR=${APP_NAME}_dmg
#DMG_PATH=./$APP_NAME.dmg

PROJECT_FILE=QTjsonDiff.pro

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
    
#    makeDMG
#    RESULT=$?
#    if [ $RESULT -ne 0 ] ; then
#        echo Failed to Make DMG file
#        return $RESULT
#    fi

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

    make CXX="g++"
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
    echo Preparing Bundle...
    
    cd $BIN_DIR
    if [ ! -d $BUNDLE_NAME ]
    then
        echo "$BUNDLE_NAME bundle doesn't exist."
        return 1
    fi

    
    # cd to Resources folder
    cd $BUNDLE_NAME
    cd Contents
    cd MacOS
    # copy appconfig to resources  
    mkdir ./lang
    cp ../../../*.qm ./lang/
    cp ../../../chart_rules.json .
    cp ../../../exec_history .
    cp ../../../filters_list .
    #path to adb in my case installed by: $ brew install android-platform-tools
    cp /usr/local/Cellar/android-platform-tools/25.0.3/bin/adb .
    #cd ..
    #cd Contents
    #cd Resources

    cd ../../../

    # Add QtFramework libraries and required plugins into .app bundle. Update dependencies of binary files.
    $MAC_DEPLOY_TOOL $BUNDLE_NAME -verbose=3

    RESULT=$?
    if [ $RESULT -ne 0 ] ; then
        echo macdeployqt failed, error code $RESULT
        return $RESULT
    fi

    cd $CURRENT_DIR
    echo Done.
 
    return 0
}

makeDMG(){
    CURRENT_DIR=$PWD
    echo Making DMG File...

    cd $BIN_DIR
    if [ ! -d $BUNDLE_NAME ]
    then
        echo "$BUNDLE_NAME bundle doesn't exist."
        return 1
    fi
    
    rm -rf $DMG_DIR
    mkdir $DMG_DIR
    cp -r $BUNDLE_NAME $DMG_DIR
    
    hdiutil create -srcfolder $DMG_DIR -format UDBZ $DMG_PATH

    cd $CURRENT_DIR

    echo Done.
    return 0
}

main
exit $?

