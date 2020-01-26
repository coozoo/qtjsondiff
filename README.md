# QT JSON diff

## Summary

Some kind of diff viewer for Json (based on tree like json container/viewer widget).

Actually I've created this widget for myself (that's explain little bit weird current comparison logic to search child+parent and doesn't matter what whole path is).

As tester often I need to compare JSONs from different sources or simply handy viewer which able to work sometimes with really big JSONs. 

Usually online viewers are simply crashing and hanging with such data. This one viewer still able to work with such big JSONs.

For example I'm using this in my SignalR, cometD clients to visualize responses or simply to show http response and compare them from other sources.
And found this example app pretty handy as well.

Some features:

    - two view modes json formatted text or json tree;
    - load json from file, url or copy paste (CTRL+V in treeview mode to paste JSON);
    - search through json text or json model (backward, forward, casesensitivity);
    - compare two jsons with highlightings, sync scrolling, sync item selection (only treeview mode);
    - two comparison modes (switched by checkbox):
      * follow by full path;
      * try to find child+parent pair anywhere inside JSON (first occurance).
    - copy text into clipboard 
      * items - key value separated by spaces;
      * copy path to the item in such format "name(type)". 
           For example: root(Object)->widget(Object)->image(Object)->alignment(String);
      * copy full json (in pretty print or plain);
      * copy value, array, object.


JSON Tree View

<img src="https://user-images.githubusercontent.com/25594311/72466610-e78a6f00-37e1-11ea-91dd-cdbe86c317d6.png" width="60%"></img> 

JSON Compare View

<img src="https://user-images.githubusercontent.com/25594311/72466616-ea855f80-37e1-11ea-9fb5-5106b20916aa.png" width="60%"></img> 

## Build from sources
You should have preinstalled QT5 (version 5.11 if you want to use older one you need to modify few lines).
Open in QTcreator the QTjsonDiff.pro file and compile.

Or execute commands:
```bash
$ git clone https://github.com/coozoo/qtjsondiff
$ cd qtjsondiff
$ qmake-qt5 # or it can be simply qmake
$ make
```

If you want to load jsons from https source then you need openssl. Under windows default paths for openssl are C:/OpenSSL-Win64/ and C:/OpenSSL-Win32/lib depends on platform. You can change them inside .pro file

## Integrate it to your application

Actually this application it is example so you can simply to view MainWindow class and you will get idea.

You need to include those files:
```cpp
#include "qjsonitem.h"
#include "qjsonmodel.h"
#include "qjsoncontainer.h"
#include "qjsondiff.h"
```
If you need just json tree view you can ignore qjsondiff.h

Declare pointers:
```cpp
    QJsonContainer *messageJsonCont; //json treeview
    QJsonDiff *differ;               //json diff like treeview
```

You need to create some UI elements where you can put those objects.

Create objects and defined their properties:
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

Parent+Child pair - slow but it will find first occurance of pair and no matter how deep they're inside JSONs. It will be very slow if JSON are significantly different.

Full Path - much faster mode (switched by default) it search for absolute path and type. It will be faster if JSONs are significantly different.


## Special thanks to this projects:
    
    https://github.com/dridk/QJsonModel - I've used this model as basement

    https://github.com/probonopd/linuxdeployqt - nice tool to deploy application on linux
