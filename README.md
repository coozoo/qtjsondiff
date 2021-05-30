# QT JSON diff

## Contents

* [Summary](#summary)
* [Installation](#installation)
* [Installation](#build-from-sources)
   * [MAC OS](#mac-os)
   * [Linux](#linux)
   * [Windows](#windows)
* [Integrate it to your application](#integrate-it-to-your-application)
* [Comparison modes](#comparison-modes)


## Summary

Some kind of diff viewer for Json (based on tree like json container/viewer widget).

Actually I've created this widget for myself. As tester often I need to compare JSONs from different sources or simply handy viewer which able to work sometimes with really big JSONs. 

Usually online viewers are simply crashing and hanging with such data. This one viewer still able to work with such big JSONs.

For example I'm using this in my SignalR, cometD clients to visualize responses or simply to show http response and compare them from other sources.
And found this example app pretty handy as well.

Some features:

    - two view modes json formatted text or json tree (switch by "Show JSON Text/View" button);
    - load json from file, url or copy paste (CTRL+V in treeview mode to paste JSON);
    - search through json text or json model (backward, forward, casesensitivity);
    - compare two jsons with highlightings, sync scrolling, sync item selection (only treeview mode);
    - sort objects inside arrays;
    - two comparison modes (switched by "Use Full Path" checkbox):
      * follow by full path;
      * try to find child+parent pair anywhere inside JSON (first occurrence).
    - copy text into clipboard:
      * **Copy Row** - item text "Name Type Value" separated by tab;
      * **Copy Rows** - all items text separated by tab (tabs allow spreadsheet paste);
      * **Copy Path** - path to the item in such format "name(type)->name(type)"
           For example: root(Object)->widget(Object)->image(Object)->alignment(String);
      * **Copy Plain Json** - copy full plain text JSON (not formatted);
      * **Copy Pretty Json** - copy full pretty print JSON;
      * **Copy Selected Json Value** - copy value, array or object.


JSON Tree View

<img src="https://user-images.githubusercontent.com/25594311/72466610-e78a6f00-37e1-11ea-91dd-cdbe86c317d6.png" width="60%"></img> 

JSON Compare View

<img src="https://user-images.githubusercontent.com/25594311/104792301-9b805a80-57a6-11eb-8cd5-eae3e7ceb78d.png" width="60%"></img> 

## Installation

Precompiled RPMs (**Fedora**, **RHEL**, **Centos**, **OpenSuse**) can be found in COPR:

```bash
$ sudo dnf copr enable yura/QTjsonDiff
$ sudo dnf install QTjsonDiff

```

[<img src="https://copr.fedorainfracloud.org/coprs/yura/QTjsonDiff/package/QTjsonDiff/status_image/last_build.png"></img>](https://copr.fedorainfracloud.org/coprs/yura/QTjsonDiff/)


**Debian**, **Ubuntu** and **some other Linux distros** may try precompiled and packed packages by linuxdeployqt available on releases page to use them don't forget install fuse system and make downloaded file executable.

**Windows** zip archives available just extract and run exe file.

**Mac OS** zip and DMG as well available. Don't forget mac prevents app from untrusted Dev (me not deserve trust :) ). so at first attempt it will be blocked. Go to Apple menu > System Preferences, click Security & Privacy, then click General and click open anyway..

You can find all of above in release section.

[<img src="https://github.com/coozoo/qtjsondiff/workflows/Release_Version/badge.svg"></img>](https://github.com/coozoo/qtjsondiff/releases/latest)

If you prefer to compile it by yourself then see below. 


## Build from sources

You should have preinstalled QT5 (version 5.12 if you want to use older one you need to modify few lines).
Open in QTcreator the QTjsonDiff.pro file and compile it (you will get something).

### MAC OS

Suppose you have installed and configured:
  - xCode+command line tools
  - QT (```brew install qt```)

```bash
$ cd ~
$ mkdir proj
$ cd proj
$ git clone https://github.com/coozoo/qtjsondiff
$ cd qtjsondiff
$ chmod 777 MAC_build_RELEASE.sh
$ ./MAC_build_RELEASE.sh
```

It will build and copy libs into app. You will find ready app inside this directory.

### Linux

You should have QT5 if no then install it accordingly to your distro.

You can build it with QTcreator or execute commands:
```bash
$ git clone https://github.com/coozoo/qtjsondiff
$ cd qtjsondiff
$ qmake-qt5 # or it can be simply qmake
$ make
$ make install #if you wish to do that
```


### Windows

Qt5 should be installed.

git clone https://github.com/coozoo/qtjsondiff

Open QTCreator and navigate to project dir. Open QTjsonDiff.pro and compile it.

If you want to load jsons from https source then you need openssl. Under windows QT provides their own openssl you can install it using QT installer from start menu and add this component.

## Integrate it to your application

Actually this application it is example so you can simply to view MainWindow class and you will get idea.

You need to include those files:
```cpp
#include "qjsonitem.h"
#include "qjsonmodel.h"
#include "qjsoncontainer.h"
#include "qjsondiff.h"

#include "preferences/preferences.h"
#include "preferences/preferencesdialog.h"

```
If you need just json tree view you can ignore qjsondiff.h

Declare pointers:
```cpp
    QJsonContainer *messageJsonCont; //json treeview
    QJsonDiff *differ;               //json diff like treeview
```

You need to create some UI elements where you can put those objects.

Create objects and define their properties:
```cpp
    
    // create json treeview and set tab as parent
    messageJsonCont=new QJsonContainer(ui->jsonview_tab);
    // you can hide address line and browse button
    //messageJsonCont->setBrowseVisible(false);
    // you can set json text
    messageJsonCont->loadJson(QString("{\"empty\":\"empty\"}"));
    
    // create json diff like tree view set some tab as parrent
    differ=new QJsonDiff(ui->compare_tab);
    // set some text to left diff view
    QJsonDocument data22=QJsonDocument::fromJson("{\"empty\":\"empty\"}");
    differ->loadJsonLeft(data22);
    // set some text to right diff view
    QJsonDocument data11=QJsonDocument::fromJson("{\"empty\":\"empty\"}");
    differ->loadJsonRight(data11);
```

That's all pretty simple.


## Comparison modes

Parent+Child pair - slow but it will find first occurrence of pair and no matter how deep they're inside JSONs. It will be very slow if JSONs are significantly different. This mode will find matches of different keys for example the one key with different type will be considered as different keys.

Full Path - much faster mode (switched by default) it searches for absolute path and type. It will be faster if JSONs are significantly different. This mode will consider the one key with different type as non existent because type is the part of path.


## Special thanks to this projects:
    
    https://github.com/dridk/QJsonModel - I've used this model as basement

    https://github.com/probonopd/linuxdeployqt - nice tool to deploy application on linux
