# QT JSON diff
## Summary
Some kind of diff viwer for Json (based on tree like json container/viewer widget).

Some features:

    - two view modes json formatted text or json tree;
    - load json from file, url or copy paste;
    - sort objects inside array by some kind of hash (usefull when you need to compare arrays content);
    - search through json text or json model with some options backward, forward, casesensitivity;
    - compare two jsons with highlightings, sync scrolling, sync item selection.


<img src="https://user-images.githubusercontent.com/25594311/34464595-f1261198-ee8e-11e7-819c-326080495141.png" width="60%"></img> 

Current comparison behaviour very slow it's searching for elements, first occurance with the same parent whenever it exists. Later I'm planning to implement comparison by exact path...

A lot of to do...

Special thanks to this projects:
    
    https://github.com/dridk/QJsonModel - I've used this model as basement. 

    https://github.com/probonopd/linuxdeployqt - nice tool to deploy application on linux
